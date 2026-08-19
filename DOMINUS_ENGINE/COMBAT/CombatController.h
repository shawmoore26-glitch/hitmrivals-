// COMBAT/CombatController.h
// LAW C012: combat communicates through Motion Graph / Animation Layers /
// IK / Procedural Hooks / Skeleton Runtime -- never a separate animation
// system. CombatController does NOT own a state machine of its own; it
// drives the SAME animation::MotionGraphEvaluator built in Phase 2.5 via
// Trigger() calls, and layers combat-only bookkeeping (which move is
// executing, how many frames into it, hitstun/blockstun) on top. The
// combat "states" named in LAW's Module 4 (idle/attack/block/dodge/
// counter/stagger/knockdown/recovery/transformation) are motion_trigger
// strings a MoveDef or ReactionResult names -- they must already exist as
// states/transitions in the bound .dominus object's motion graph, or
// Trigger() simply returns false (checked, not swallowed).
#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "ANIMATION/AnimationGraph/MotionGraphEvaluator.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CinematicDirector.h"
#include "COMBAT/ComboSystem/ComboEngine.h"
#include "COMBAT/ComboSystem/StyleCollector.h"
#include "COMBAT/HitSystem/CollisionEvaluator.h"
#include "COMBAT/HitSystem/CollisionResolver.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/HitSystem/MoveDef.h"
#include "COMBAT/PhysicsCombat/ImpactSolver.h"
#include "COMBAT/Provenance/ImpactEvent.h"
#include "COMBAT/Provenance/ImpactEventCompiler.h"
#include "COMBAT/Provenance/ImpactEventLog.h"
#include "COMBAT/Provenance/ImpactEventWorldHistoryHook.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"
#include "WORLD/Core/WorldHistory.h"

namespace dominus::combat {

enum class CombatPhase {
    kNeutral,
    kStartup,
    kActive,
    kRecovery,
    kHitstun,
    kBlockstun,
    kKnockdown,
};

// Phase 4.1.5: bundles the genome components one side of an impact can
// optionally carry. Any field may be nullptr -- an entity whose
// .dominus file doesn't reference a given genome simply doesn't
// contribute that genome's multiplier, and BuildImpactContext degrades
// to ImpactContext's own neutral defaults, never fails. Same
// missing-data discipline RigBinder itself already uses for every
// optional ref component.
struct ImpactGenomeInputs {
    const character::CombatPhysicsGenomeComponent* physics = nullptr;
    const character::CombatStyleGenomeComponent* style = nullptr;
    const character::MaterialGenomeComponent* material = nullptr;
    const character::GameDesignGenomeComponent* design = nullptr;
    const character::VisualStyleGenomeComponent* visualStyle = nullptr;
};

// Phase 4.1.5: "gathering attacker/defender, reading attached genomes,
// constructing ImpactContext" -- literally this function's whole job,
// and ImpactSolver's ONLY caller-facing input type. Free function, not
// a CombatController member, so it's independently testable without a
// live MotionGraphEvaluator/MoveSetComponent.
inline ImpactContext BuildImpactContext(const ImpactGenomeInputs& attacker,
                                                        const ImpactGenomeInputs& defender,
                                                        float attackerVelocity,
                                                        const std::string& struckBodyPart, float impactDirX,
                                                        bool defenderBlocking, bool defenderAlreadyStaggered) {
    ImpactContext ctx;
    ctx.attacker_velocity = attackerVelocity;
    ctx.struck_body_part = struckBodyPart;
    ctx.impact_dir_x = impactDirX;
    ctx.defender_blocking = defenderBlocking;
    ctx.defender_already_staggered = defenderAlreadyStaggered;

    if (attacker.physics) {
        ctx.attacker_mass_kg = attacker.physics->genome.body.mass_kg;
        ctx.attacker_strike_force_multiplier = attacker.physics->genome.impact.strike_force;
    }
    if (attacker.style) {
        ctx.attacker_aggression = attacker.style->genome.aggression;
    }

    if (defender.physics) {
        ctx.defender_durability = defender.physics->genome.impact.durability;
        ctx.defender_armor = defender.physics->genome.body.armor;
    }
    if (defender.material) {
        ctx.defender_material_wear = defender.material->genome.properties.wear_state;
    }
    if (defender.visualStyle) {
        ctx.visual_style_id = defender.visualStyle->genome.style_id;
    }

    // Design tuning describes the game, not either fighter specifically
    // -- read from whichever side actually carries a ref, attacker
    // first. If neither does, ImpactContext's own neutral default (0.5,
    // exact 1.0x multiplier) applies.
    if (attacker.design) {
        ctx.design_risk_reward_balance = attacker.design->genome.risk_reward_balance;
    } else if (defender.design) {
        ctx.design_risk_reward_balance = defender.design->genome.risk_reward_balance;
    }

    return ctx;
}

class CombatController {
public:
    CombatController(animation::MotionGraphEvaluator& evaluator, const MoveSetComponent& moves)
        : evaluator_(evaluator), moves_(moves) {}

    // LAW C001: Intent -> Combat Logic -> Motion Request. StartMove IS that
    // boundary -- it looks up data-driven move identity, then requests
    // motion via the graph; it never plays a clip directly.
    bool StartMove(const std::string& name) {
        const MoveDef* move = moves_.Find(name);
        if (!move) return false;
        if (!evaluator_.Trigger(move->motion_trigger)) return false;

        currentMove_ = move;
        elapsedFrames_ = 0;
        phase_ = CombatPhase::kStartup;
        collisionResolver_.ResetForNewActivation();  // a new move is a fresh opportunity to land one hit
        return true;
    }

    // LAW C005/C007: cancel legality reads off the current move's own
    // cancel_window and followups -- ComboEngine, not a hardcoded table.
    bool TryCancelInto(const std::string& nextMoveName) {
        if (!currentMove_) return false;
        if (!ComboEngine::CanCancelInto(*currentMove_, elapsedFrames_, nextMoveName)) return false;
        bool started = StartMove(nextMoveName);
        if (started) combo_.RecordHit(nextMoveName);
        return started;
    }

    void Update(float dt) {
        evaluator_.Update(dt);
        if (!currentMove_) return;

        elapsedFrames_ += static_cast<int>(std::round(dt * kFramesPerSecond));
        int activeStart = currentMove_->frames.startup;
        int recoveryStart = activeStart + currentMove_->frames.active;
        int totalFrames = currentMove_->frames.TotalFrames();

        if (elapsedFrames_ >= totalFrames) {
            currentMove_ = nullptr;
            phase_ = CombatPhase::kNeutral;
        } else if (elapsedFrames_ >= recoveryStart) {
            phase_ = CombatPhase::kRecovery;
        } else if (elapsedFrames_ >= activeStart) {
            phase_ = CombatPhase::kActive;
        } else {
            phase_ = CombatPhase::kStartup;
        }
    }

    // LAW C001's "Reaction State" stage: takes a physics-derived
    // ReactionInput, drives the reaction through the SAME motion graph via
    // Trigger(), interrupting whatever move was in flight. If a
    // StyleCollector is attached, hit_power is auto-recorded as damage
    // taken -- the "damage taken" metric StyleRankSystem needs no longer
    // has to be hand-typed by a caller.
    ReactionResult ApplyHit(const ReactionInput& input, float impactDirX) {
        if (styleCollector_) styleCollector_->RecordDamageTaken(input.hit_power);
        return ApplyPrecomputedReaction(ReactionSystem::Determine(input, impactDirX));
    }

    // Phase 4.1.5: the genome-driven pipeline --
    // CombatController -> ImpactContext -> ImpactSolver::Solve() ->
    // ImpactResult -> ReactionSystem::Apply(). CombatController's own
    // role is exactly gathering + constructing + delegating: it builds
    // the context (via the free BuildImpactContext above), hands it to
    // the pure solver, hands the solver's pure result to
    // ReactionSystem::Apply for reaction determination, then drives the
    // result through the SAME existing ApplyPrecomputedReaction seam
    // ApplyHit/Environment/Cinematic reactions already use. This method
    // never computes damage itself and never mutates state outside that
    // one existing, already-tested seam -- it is additive alongside
    // ApplyHit, not a replacement for it; ApplyHit's plain-ReactionInput
    // path keeps working byte-identically for every existing caller.
    //
    // Phase 4.1.7: attackerId/tick are NEW optional parameters (default
    // "" / 0) -- every Phase 4.1.5 call site keeps compiling and
    // behaving identically. When a provenance log IS attached (see
    // SetProvenanceLog below), every real impact that flows through
    // this method automatically gets an ImpactEvent recorded -- not a
    // new mechanic, just closing the "who actually calls
    // ImpactEventCompiler::Compile during a real fight" gap Phase 4.1.6
    // deliberately left open.
    ReactionResult ApplyImpact(const ImpactGenomeInputs& attacker, const ImpactGenomeInputs& defender,
                                float attackerVelocity, const std::string& struckBodyPart, float impactDirX,
                                bool defenderBlocking, bool defenderAlreadyStaggered,
                                const std::string& attackerId = "", std::uint64_t tick = 0) {
        auto ctx = BuildImpactContext(attacker, defender, attackerVelocity, struckBodyPart, impactDirX,
                                       defenderBlocking, defenderAlreadyStaggered);
        ImpactResult impactResult = ImpactSolver::Solve(ctx);
        if (styleCollector_) styleCollector_->RecordDamageTaken(impactResult.damage);

        // Automatic provenance capture -- opt-in (see SetProvenanceLog),
        // zero effect on any CombatController that hasn't attached one
        // (Phase 4.1.5's own tests never do, and still pass unchanged).
        if (provenanceLog_) {
            auto event = ImpactEventCompiler::Compile(attackerId, entityId_, ctx, impactResult, tick);
            provenanceLog_->Record(event);
            if (worldHistory_) {
                RecordImpactEvent(*worldHistory_, event, static_cast<float>(tick));
            }
        }

        ReactionResult reaction = ReactionSystem::Apply(impactResult);
        return ApplyPrecomputedReaction(reaction);
    }

    // Closes Combat's real broken loop:
    //   CollisionEvaluator -> CollisionResolver -> ImpactContext ->
    //   ImpactSolver -> ImpactResult -> ReactionSystem::Apply
    // CollisionEvaluator itself stays a pure query (its own contract,
    // untouched, unchanged) -- this method is the caller-side decision
    // it always said it needed: gate to at most one impact per move
    // activation via CollisionResolver, compute a real impact direction
    // from the two skeletons' own root positions (never from the
    // hitbox/hurtbox midpoint, which says nothing about which way a
    // body should fly), then reuse ApplyImpact exactly as it already
    // exists -- no duplicated solve/reaction logic.
    //
    // Called on the ATTACKER's controller (this owns currentMove_, the
    // hitboxes, and the per-activation CollisionResolver gate), but
    // applies the reaction to `defenderController` -- ApplyImpact's own
    // doc comment has always said "CombatController is always the
    // entity being controlled/reacting"; a hit lands on the defender,
    // it does not self-interrupt the attacker's own swing. Attacker and
    // defender must be genuinely different controllers -- passing the
    // same controller for both would mean an attack interrupts itself
    // the instant it lands, which is exactly the bug an early version
    // of this method had before its own test caught it.
    //
    // Requires an active move with real hitboxes (LAW C005: hitboxes
    // are only meaningful during CombatPhase::kActive) -- returns
    // std::nullopt, not a crash or a silent no-op that could be
    // mistaken for "checked and found nothing," for every input that
    // isn't a legitimate live collision opportunity.
    std::optional<ReactionResult> EvaluateCollisionAndApplyImpact(
        CombatController& defenderController, const animation::Skeleton& attackerSkeleton,
        const animation::Pose& attackerPose, const animation::Skeleton& defenderSkeleton,
        const animation::Pose& defenderPose, const HurtboxSet& defenderHurtboxes,
        const ImpactGenomeInputs& attackerGenomes, const ImpactGenomeInputs& defenderGenomes, float attackerVelocity,
        const std::string& attackerId = "", std::uint64_t tick = 0) {
        if (!currentMove_) return std::nullopt;
        if (phase_ != CombatPhase::kActive) return std::nullopt;

        auto hits = CollisionEvaluator::Evaluate(attackerSkeleton, attackerPose, *currentMove_, defenderSkeleton,
                                                   defenderPose, defenderHurtboxes);
        auto resolved = collisionResolver_.Resolve(hits);
        if (!resolved) return std::nullopt;

        // Real impact direction: attacker's root relative to defender's
        // root, not the hit midpoint (a hit landing dead-center between
        // two overlapping bones says nothing about which way the
        // defender's body should be pushed). Missing root bone fails
        // gracefully to a fixed direction, same "optional data, neutral
        // default" discipline as every genome-driven default elsewhere
        // in this pipeline -- never a crash.
        float impactDirX = 1.0f;
        auto attackerRoot = attackerSkeleton.FindBoneIndex("root");
        auto defenderRoot = defenderSkeleton.FindBoneIndex("root");
        if (attackerRoot && defenderRoot) {
            float dx = attackerPose[*attackerRoot].x - defenderPose[*defenderRoot].x;
            if (dx != 0.0f) impactDirX = dx > 0.0f ? -1.0f : 1.0f;  // push defender AWAY from attacker
        }

        ReactionResult reaction = defenderController.ApplyImpact(attackerGenomes, defenderGenomes, attackerVelocity,
                                                                   resolved->defender_bone, impactDirX,
                                               /*defenderBlocking=*/false, /*defenderAlreadyStaggered=*/false,
                                               attackerId, tick);
        return reaction;
    }

    // Phase 4.1.7: opt-in provenance attachment, same pattern as
    // SetStyleCollector/SetCinematicDirector above -- a raw, optional,
    // non-owning pointer. entityId is THIS controller's own id (the
    // defender in every ApplyImpact call -- CombatController is always
    // "the entity being controlled/reacting", same framing ApplyHit has
    // always had).
    void SetProvenanceLog(ImpactEventLog* log) { provenanceLog_ = log; }
    ImpactEventLog* ProvenanceLog() const { return provenanceLog_; }
    void SetWorldHistory(world::WorldHistory* history) { worldHistory_ = history; }
    world::WorldHistory* WorldHistoryLog() const { return worldHistory_; }
    void SetEntityId(std::string id) { entityId_ = std::move(id); }
    const std::string& EntityId() const { return entityId_; }

    // Call once CollisionEvaluator confirms the CURRENTLY EXECUTING move
    // actually landed -- auto-records hit_count/distinct_move_count/
    // high_risk_move_count/damage_dealt on the attached StyleCollector via
    // the real MoveDef data, not a hand-typed literal. No-op if no move is
    // in flight or no collector is attached.
    void RecordMoveLanded(float damageDealt) {
        if (styleCollector_ && currentMove_) styleCollector_->RecordHitLanded(*currentMove_, damageDealt);
    }

    void SetStyleCollector(StyleCollector* collector) { styleCollector_ = collector; }
    StyleCollector* Style() const { return styleCollector_; }

    // Accepts an already-computed ReactionResult -- the seam Environmental
    // Combat (COMBAT/Environment.h) upgrades a reaction through (e.g.
    // ReactionSystem::Determine produces a plain knockback, then
    // Environment::ApplyEnvironment upgrades it to kWallImpact if the
    // predicted trajectory crosses a wall) before it reaches the motion
    // graph and cinematic layer.
    ReactionResult ApplyPrecomputedReaction(const ReactionResult& result) {
        evaluator_.Trigger(result.motion_trigger);  // no-op (false) if the
                                                      // bound graph doesn't
                                                      // define this state yet

        switch (result.type) {
            case ReactionType::kKnockdown:
                phase_ = CombatPhase::kKnockdown;
                if (cinematic_) cinematic_->Trigger(CombatEventType::kFinisher);
                break;
            case ReactionType::kNone:
                phase_ = CombatPhase::kBlockstun;
                break;
            case ReactionType::kWallImpact:
                phase_ = CombatPhase::kHitstun;
                if (cinematic_) cinematic_->Trigger(CombatEventType::kWallImpact);
                break;
            case ReactionType::kGroundImpact:
                phase_ = CombatPhase::kHitstun;
                break;
            case ReactionType::kStagger:
            case ReactionType::kKnockback:
            case ReactionType::kLaunch:
                phase_ = CombatPhase::kHitstun;
                break;
        }
        currentMove_ = nullptr;
        combo_.Reset();
        return result;
    }

    // Optional -- combat works identically with no director attached
    // (existing Phase 3 behavior/tests are unaffected by this being unset).
    void SetCinematicDirector(CinematicDirector* director) { cinematic_ = director; }
    CinematicDirector* Cinematic() const { return cinematic_; }

    CombatPhase Phase() const { return phase_; }
    bool IsActiveFrame() const { return phase_ == CombatPhase::kActive; }
    const MoveDef* CurrentMove() const { return currentMove_; }
    const ComboEngine& Combo() const { return combo_; }

private:
    animation::MotionGraphEvaluator& evaluator_;
    const MoveSetComponent& moves_;
    const MoveDef* currentMove_ = nullptr;
    int elapsedFrames_ = 0;
    CombatPhase phase_ = CombatPhase::kNeutral;
    ComboEngine combo_;
    CinematicDirector* cinematic_ = nullptr;
    StyleCollector* styleCollector_ = nullptr;
    ImpactEventLog* provenanceLog_ = nullptr;
    world::WorldHistory* worldHistory_ = nullptr;
    std::string entityId_;
    CollisionResolver collisionResolver_;
};

}  // namespace dominus::combat
