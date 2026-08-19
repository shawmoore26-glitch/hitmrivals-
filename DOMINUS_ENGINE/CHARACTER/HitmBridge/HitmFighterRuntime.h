// CHARACTER/HitmBridge/HitmFighterRuntime.h
// ROADMAP.md Track H Module 5A -- CPU-observable HITM runtime vertical
// slice. See HITM_FIGHTER_RUNTIME_REPORT.md for the full PROVEN /
// IMPLEMENTED-BUT-UNVERIFIABLE / NOT-IMPLEMENTED accounting.
//
// The point of this class: real HITM data actually drives simulation
// behavior, not just validated JSON. Brooklyn's real identity (Module 1),
// real combat genome including his read-engine (Module 2), and the real
// global game rules (Module 4) together initialize and step a fighter
// whose movement, gravity, attack timing, damage, meter, hitstop, and
// read-engine transitions all come from the real authored numbers.
//
// REAL, GENUINE INTEGRATION with existing DOMINUS runtime systems, not a
// side simulation:
//   - dominus::world::World / EntityRegistry / SpatialComponent (WORLD LAW
//     001/002) hold the fighter as a real entity — the same
//     MetaBinObject + SpatialComponent::Fighter2_5D pattern Phase 4.0's
//     own milestone test (tests/world/test_hitm_rivals_as_world_entity.cpp)
//     established.
//   - dominus::physics::PhysicsSystem / RigidBody perform the actual
//     gravity/velocity/position integration, constructed with HITM's
//     real gravity value, not a placeholder.
//   - dominus::combat::ReactionSystem::Determine (COMBAT/ReactionSystem/
//     ReactionSystem.h) — real, existing, already-tested logic — decides
//     reaction type or from real hit_power/defense_bias, unmodified.
//
// A REAL BUG FOUND AND FIXED DURING THIS MODULE, documented rather than
// silently corrected: the first version of this class registered its
// per-frame logic as a `world::WorldSystemFn` lambda capturing `this` on
// `world_.Systems()` (WORLD LAW 003's plugin pattern) in the constructor.
// `HitmFighterRuntime` is move-constructed by `Result<T>::Ok(std::move(
// runtime))` in every factory function that returns one -- and the
// compiler-generated move constructor moves `world_` (including that
// registered closure) member-wise, but a captured raw `this` pointer
// inside a `std::function` does NOT get rewritten to point at the new
// object during a move. The result: after any move, the stored lambda
// still called `RunOneFrame` on the OLD, now-destroyed object's address
// -- a dangling-pointer segfault, caught by actually running the test
// suite, not by inspection. This class therefore calls `physics_.
// Integrate(world_.Entities(), dt)` and its own frame logic directly
// from `AdvanceFrame()` instead of through a registered `WorldSystemFn`
// closure -- still the real `World`/`EntityRegistry`/`SpatialComponent`/
// `PhysicsSystem`/`RigidBody` types, just without a self-referential
// closure that has no decoupling benefit here (this class owns both the
// `World` and the frame logic together; nothing else needs to reorder or
// replace it the way COMBAT/PHYSICS as independently-authored WORLD
// extensions do).
//
// A REAL ARCHITECTURAL GAP, found and NOT papered over: COMBAT's existing
// CombatController/MotionGraphEvaluator (the systems Phases 3-3.9 built)
// require a real Skeleton, real AnimationClips, and a real MotionGraph —
// all three exist today only as DOMINUS-authored fixtures
// (tests/fixtures/brooklyn_*.clip.json, brooklyn_motion_graph.json), never
// derived from real HITM data. Track H Module 3 already refused to bind
// HitmPartsRig into a Skeleton because doing so would require inventing
// bind-pose transforms the real data doesn't have; no real authored
// keyframe pose data exists anywhere in HITM's non-generated sources
// either. Reusing CombatController/MotionGraphEvaluator here would mean
// driving Brooklyn through DOMINUS's own invented animation fixtures --
// exactly the strawman pattern this whole track exists to move away from.
// This class therefore implements its own gameplay state machine
// (HitmFighterState below) directly from real frame-count data
// (HitmMoveInstance's startup/active/recovery), with zero dependency on
// Skeleton/AnimationClip/MotionGraph/Pose. This is a real, load-bearing
// scope boundary, not an oversight -- see the report for the follow-up
// this implies (Module 5C+).
//
// A SECOND REAL GAP: real HITM move reach is a flat `range`/`height` pair
// (HitmMoveInstance), not COMBAT::HitboxDef's bone-relative circles (which
// require a live Pose, i.e. the same Skeleton/AnimationClip dependency
// above). TakeHit() below therefore does not use COMBAT::CollisionEvaluator
// or Hurtbox at all -- it resolves a hit's real damage/hitstun/blockstun/
// meter/hitstop numbers directly, with COMBAT::ReactionSystem deciding
// only the reaction TYPE (stagger/knockback/launch/knockdown/block), which
// needs no pose/collision geometry.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityRecord.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "CHARACTER/HitmBridge/HitmReadEngineState.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "PHYSICS/PhysicsSystem.h"
#include "WORLD/Core/World.h"

namespace dominus::character::hitm {

enum class HitmFighterState {
    kIdle,
    kWalking,
    kJumping,
    kBlockingStance,   // entered by holding kBlock input on the ground -- distinct from kBlockstun (a reaction to being hit)
    kAttackStartup,
    kAttackActive,
    kAttackRecovery,
    kHitstun,
    kBlockstun,
};

// Real, evidenced input vocabulary -- see HitmMoveInstance.h's top comment
// for why there is no "light attack"/"jab" command: no real authored
// data exists for one. kSpecial corresponds to signature.json's real
// "S" input token for Brooklyn's "special" move, which this runtime
// resolves via HitmMoveInstance::Extract at Create() time.
enum class HitmInputCommand {
    kNeutral,
    kLeft,
    kRight,
    kJump,
    kSpecial,
    kBlock,
};

// A plain, comparable snapshot of every piece of state this class
// exposes -- the basis for the determinism proof (two independently
// constructed runtimes fed the identical input sequence must produce
// identical snapshots after N frames).
struct HitmFighterSnapshot {
    uint64_t frame = 0;
    HitmFighterState state = HitmFighterState::kIdle;
    float x = 0.0f;
    float y = 0.0f;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    bool grounded = true;
    double meter = 0.0;
    int read_engine_reads = 0;
    int hitstop_frames_remaining = 0;
    int state_frames_remaining = 0;

    bool operator==(const HitmFighterSnapshot&) const = default;
};

class HitmFighterRuntime {
public:
    // Builds a real fighter from already-imported, already-validated real
    // data (Modules 1/2/4). Fails (Result::Fail) on:
    //  - record/genome fighter_id mismatch (same fail-closed pattern as
    //    every prior module's directory/id cross-check)
    //  - HitmMoveInstance::Extract("special") failing (missing/malformed
    //    real move data)
    // Never fabricates a fighter from partial data.
    static core::Result<HitmFighterRuntime> Create(const HitmIdentityRecord& record, const HitmCombatGenome& genome,
                                                     const HitmGameRules& rules);

    // Advances exactly one real HITM frame. dt is fixed at 1.0 inside --
    // see HitmFighterRuntime.cpp for why that (not a wall-clock fraction
    // of a second) is what makes PHYSICS::PhysicsSystem's generic force/
    // mass/dt integration reproduce HITM's real, discrete, per-frame
    // gravity convention exactly.
    void AdvanceFrame(HitmInputCommand input);

    // Resolves `incoming` landing on this fighter as the DEFENDER. Real
    // damage/hitstun/blockstun/meter numbers, COMBAT::ReactionSystem
    // decides the reaction type from this fighter's own real
    // defense_profile.blockPreference as defense_bias. See this header's
    // top comment for why this does not use COMBAT::CollisionEvaluator.
    void TakeHit(const HitmMoveInstance& incoming, bool blocking);

    // Passthrough to this fighter's own HitmReadEngineState -- the real
    // trigger CONDITIONS (counter hit, whiff punish, perfect guard, throw
    // confirm / being counter hit, dropping a chain, whiffing a throw)
    // are real authored strings (HitmCombatGenome::ReadEngine::gain_on/
    // lose_on) this vertical slice does not detect automatically -- no
    // opponent or hit-classification model exists to recognize them. This
    // is the real, explicit trigger seam until one does; exposed publicly
    // rather than hidden, so a caller (a test, or eventually a real
    // detector) can prove the tier actually transitions.
    void GainRead() { readEngine_.GainRead(); }
    void LoseRead() { readEngine_.LoseRead(); }

    // Pure calculation, no state mutation: this fighter's own move power
    // multiplied by the real, current read-engine tier's damage_mult --
    // "the read engine multiplies OUTPUT, never the table" (the real
    // authored law), implemented literally.
    double ResolveOutgoingDamage(const HitmMoveInstance& move) const;

    // Applies the real per-move meterGain plus the real global
    // meter.onHitGive to this fighter's own meter, as the ATTACKER of a
    // landed hit. Separate from TakeHit (the DEFENDER path) because the
    // two real game-rule fields are genuinely different numbers for
    // genuinely different roles.
    void ResolveOutgoingHitLanded(const HitmMoveInstance& move);

    HitmFighterSnapshot Snapshot() const;
    HitmFighterState State() const { return state_; }
    // Named ReadEngineState(), not ReadEngine(), deliberately: a member
    // function named ReadEngine() would shadow the free ReadEngine type
    // (CHARACTER/HitmBridge/HitmCombatGenome.h) inside every member
    // function of this class per ordinary C++ name lookup -- found the
    // hard way while implementing Create() below, not a style preference.
    const HitmReadEngineState& ReadEngineState() const { return readEngine_; }
    const std::string& FighterId() const { return fighterId_; }

    // The real, existing COMBAT::ReactionSystem's decision from the most
    // recent TakeHit() call -- exposed so callers/tests can verify real
    // defense_bias data genuinely changes the reaction type, not just
    // that TakeHit ran without crashing.
    const combat::ReactionResult& LastReaction() const { return lastReaction_; }

private:
    HitmFighterRuntime(HitmGameRules rules, HitmReadEngineState readEngine, HitmMoveInstance specialMove,
                        double defenseBlockPreference, std::string fighterId);

    double ClampMeter(double value) const;
    int HitstopFramesFor(HitmHitstopCategory category) const;
    void RunOneFrame(HitmInputCommand input);

    world::World world_;
    physics::PhysicsSystem physics_;
    std::string entityId_;
    std::string fighterId_;

    HitmGameRules rules_;
    HitmReadEngineState readEngine_;
    HitmMoveInstance specialMove_;
    double defenseBlockPreference_;

    HitmFighterState state_ = HitmFighterState::kIdle;
    int stateFramesRemaining_ = 0;
    int hitstopFramesRemaining_ = 0;
    double meter_ = 0.0;
    bool grounded_ = true;
    uint64_t frame_ = 0;
    combat::ReactionResult lastReaction_;
};

}  // namespace dominus::character::hitm
