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
//   - dominus::world::World / EntityRegistry / SpatialComponent / WorldTick
//     (WORLD LAW 001/002/003) hold the fighter as a real entity and drive
//     its per-frame logic through a registered `world::WorldSystemFn`,
//     the same plugin pattern Phase 4.0's own milestone test
//     (tests/world/test_hitm_rivals_as_world_entity.cpp) established.
//   - dominus::physics::PhysicsSystem / RigidBody perform the actual
//     gravity/velocity/position integration, constructed with HITM's
//     real gravity value, not a placeholder.
//   - dominus::combat::ReactionSystem::Determine (COMBAT/ReactionSystem/
//     ReactionSystem.h) — real, existing, already-tested logic — decides
//     reaction type from real hit_power/defense_bias, unmodified.
//
// A REAL BUG FOUND, FIXED PROPERLY (not routed around), and documented:
// the first version of this class registered its per-frame logic as a
// `this`-capturing `WorldSystemFn` closure on `world_.Systems()`. Every
// factory function returns a `HitmFighterRuntime` by value through
// `Result<T>::Ok(std::move(...))`, and a raw `this` pointer captured
// inside a `std::function` does NOT get rewritten to the new address
// when the object holding it is moved -- the closure kept calling
// `RunOneFrame` on the OLD, now-destroyed object. A first fix removed
// the WorldTick registration entirely and called the frame logic
// directly instead; that made the symptom go away but gave up genuine
// WorldTick/WorldSystemFn integration to do it, which is not actually
// required to fix a dangling-pointer bug -- the REAL fix is to stop the
// pointer from dangling.
//
// THE ACTUAL FIX: every mutable field this class owns (`World`,
// `PhysicsSystem`, gameplay state, read-engine, ...) lives in a private
// `FrameState`, allocated ONCE on the heap via `std::unique_ptr<FrameState>`
// and never relocated for the lifetime of that FrameState object. Moving
// a `HitmFighterRuntime` moves only the `unique_ptr` itself -- a pointer-
// value transfer -- which changes nothing about the address the
// `FrameState` actually lives at. The registered `WorldSystemFn` closure
// captures a raw `FrameState*` (obtained once, right after allocation),
// never `this` -- so the closure stays valid across arbitrarily many
// moves, `Result<T>` returns, and container storage, because the thing
// it points at never moves. This is the standard "stable pImpl block"
// pattern for a self-referential object, applied here specifically
// because `HitmFighterRuntime` needed to remain both move-safe AND keep
// its real `WorldSystemFn` registration -- neither requirement traded
// against the other. Construction, move, `AdvanceFrame` after the move,
// and destruction are all covered by
// tests/integration/test_hitm_fighter_runtime.cpp's
// `HitmFighterRuntime_LifetimeSafety_*` tests, and the whole suite is
// clean under AddressSanitizer (see HITM_FIGHTER_RUNTIME_REPORT.md for
// the exact command and result) -- not just "didn't crash in N runs."
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
//
// A DELIBERATELY SCOPED EXTENSION (Track H, Track A gap closeout): Track
// H Module 5B's own report (HITM_SPRITE_ASSET_REPORT.md) identified one
// real, specific gap this class's public snapshot left open: hitm-engine's
// own real `AnimationSystem.js` samples idle/walk/jump clips at `f.animT`,
// a counter that RESETS to 0 on every state transition; this class only
// ever exposed the match-wide monotonic `frame` counter, so Module 5B
// could not reproduce that reset behavior. `HitmFighterSnapshot::
// state_frame` below closes exactly that one gap -- frames elapsed since
// `state` last changed -- and nothing else. It does not add a landing-
// recovery timer, a facing/opponent concept, or touch any existing
// gameplay number (position, velocity, damage, meter, hitstop, or the
// existing `frame`/`state_frames_remaining` fields are computed exactly
// as before). Frozen (neither reset nor incremented) during hitstop,
// matching the existing "nothing else in the simulation advances" hitstop
// convention -- the real, intended effect: the displayed animation frame
// holds during a hitstop freeze, the same way a real fighting game's
// hit-freeze visually holds the current pose.
#pragma once

#include <cstdint>
#include <memory>
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
    // Frames elapsed since `state` last changed -- 0 on the frame a
    // transition happens, incrementing every real frame after that,
    // frozen during hitstop. See this header's top comment ("A
    // DELIBERATELY SCOPED EXTENSION") for exactly what this does and
    // does not add.
    int state_frame = 0;

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

    // Movable (required by Result<T>/std::optional and by returning this
    // type from factory functions); intentionally NOT copyable -- a live
    // simulation with a registered WorldTick closure has no sensible
    // "duplicate this in-flight fighter" semantics, and nothing in this
    // module needs one (two independent fighters come from two
    // independent Create() calls, see the determinism test). Both moves
    // are safe by construction -- see this header's top comment.
    HitmFighterRuntime(HitmFighterRuntime&&) noexcept = default;
    HitmFighterRuntime& operator=(HitmFighterRuntime&&) noexcept = default;
    HitmFighterRuntime(const HitmFighterRuntime&) = delete;
    HitmFighterRuntime& operator=(const HitmFighterRuntime&) = delete;
    ~HitmFighterRuntime() = default;

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
    void GainRead();
    void LoseRead();

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
    HitmFighterState State() const;
    // Named ReadEngineState(), not ReadEngine(), deliberately: a member
    // function named ReadEngine() would shadow the free ReadEngine type
    // (CHARACTER/HitmBridge/HitmCombatGenome.h) inside every member
    // function of this class per ordinary C++ name lookup -- found the
    // hard way while implementing Create() below, not a style preference.
    const HitmReadEngineState& ReadEngineState() const;
    const std::string& FighterId() const;

    // The real, existing COMBAT::ReactionSystem's decision from the most
    // recent TakeHit() call -- exposed so callers/tests can verify real
    // defense_bias data genuinely changes the reaction type, not just
    // that TakeHit ran without crashing.
    const combat::ReactionResult& LastReaction() const;

private:
    // Everything this runtime owns lives here, allocated once and never
    // relocated -- see this header's top comment for why. `FrameState`
    // has no public API of its own; it is purely HitmFighterRuntime's
    // private, stable-address storage.
    struct FrameState {
        world::World world;
        physics::PhysicsSystem physics;
        std::string entityId;
        std::string fighterId;

        HitmGameRules rules;
        HitmReadEngineState readEngine;
        HitmMoveInstance specialMove;
        double defenseBlockPreference;

        HitmFighterState state = HitmFighterState::kIdle;
        int stateFramesRemaining = 0;
        int stateFrame = 0;  // see header comment "A DELIBERATELY SCOPED EXTENSION"
        int hitstopFramesRemaining = 0;
        double meter = 0.0;
        bool grounded = true;
        uint64_t frame = 0;
        combat::ReactionResult lastReaction;
        HitmInputCommand pendingInput = HitmInputCommand::kNeutral;

        FrameState(HitmGameRules rulesIn, HitmReadEngineState readEngineIn, HitmMoveInstance specialMoveIn,
                   double defenseBlockPreferenceIn, std::string fighterIdIn)
            : physics(static_cast<float>(rulesIn.Physics().gravity)),
              entityId(fighterIdIn),
              fighterId(std::move(fighterIdIn)),
              rules(std::move(rulesIn)),
              readEngine(std::move(readEngineIn)),
              specialMove(std::move(specialMoveIn)),
              defenseBlockPreference(defenseBlockPreferenceIn) {}
    };

    explicit HitmFighterRuntime(std::unique_ptr<FrameState> state);

    static double ClampMeter(const FrameState& state, double value);
    static int HitstopFramesFor(const FrameState& state, HitmHitstopCategory category);
    // Static and free of any `this`/HitmFighterRuntime dependency,
    // deliberately: this is exactly the function the registered
    // WorldSystemFn closure calls, and it must only ever touch the
    // FrameState it's handed, never the HitmFighterRuntime wrapper
    // (which is the object that moves).
    static void RunOneFrame(FrameState& state, HitmInputCommand input);

    std::unique_ptr<FrameState> state_;
};

}  // namespace dominus::character::hitm
