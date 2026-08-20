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
//
// PHASE 1 -- RUNTIME FOUNDATION (HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md):
// the first of that audit's four explicitly-authorized, narrowly-scoped
// steps toward a real Brooklyn-vs-Rocket CPU match. Three real, additive
// changes, nothing else:
//   1. The read engine is now OPTIONAL. `Create()` previously hard-failed
//      any fighter with no `read_engine` in their real combat genome.
//      Rocket and Static genuinely, permanently have none -- that is real,
//      verified data about them, not a gap -- so failing on it was a
//      DOMINUS implementation choice, not a reflection of missing
//      authoring. `GainRead()`/`LoseRead()`/`ResolveOutgoingDamage()` all
//      degrade to real, documented no-ops/1.0x defaults for such a
//      fighter (see each one's own comment in the .cpp); `ReadEngineState()`
//      returns nullptr instead of a reference, and the new `HasReadEngine()`
//      lets a caller ask explicitly rather than guess. Brooklyn's own
//      behavior is byte-identical -- he still has a real read engine, so
//      every existing code path for him is unchanged.
//      NOTE: this does NOT make Rocket/Static's `Create()` call succeed --
//      they still fail at the earlier "special" move extraction (their
//      real move schemas don't fit `HitmMoveInstance::Extract`'s current
//      required-field set, a separate, still-open gap the audit's Step 2
//      names explicitly). This step only removes the read-engine
//      requirement as a SECOND, independent blocker -- verified by its own
//      test proving Rocket's `Create()` failure message now names the move
//      schema, not `read_engine`.
//   2. Real per-fighter HP. `maxHp = round(1000 * healthMult)` -- `1000` is
//      a real, hardcoded, uniform-across-every-fighter engine constant
//      (hitm-engine's own `Fighter.js:23`, not authored per-fighter data);
//      `healthMult` is real, per-fighter data already imported losslessly
//      by Module 1 (`HitmIdentityRecord::character_dna`'s real
//      `frames.healthMult`) but never before extracted into a typed field.
//      `hp` starts at `maxHp` and PHASE 1 DOES NOT REDUCE IT -- `TakeHit`
//      still only resolves reaction type/hitstun/blockstun/meter/hitstop,
//      exactly as before; wiring real damage into `hp` and detecting KO is
//      the audit's own separately-scoped Phase 2, deliberately not done
//      here.
//   3. Real facing, exposed the same way `GainRead()`/`LoseRead()` already
//      are: an explicit, publicly-callable seam (`SetFacing()`), not
//      something this single-fighter runtime computes on its own -- there
//      is still no opponent here to derive a real value from (the same
//      reasoning `TakeHit`'s own `impactDirX=1.0f` default already
//      documents). Defaults to `+1` (facing right) at `Create()`. The one
//      piece of real logic this runtime DOES enforce itself, because it
//      needs no opponent to know: the real engine's own rule that facing
//      never changes while a fighter is committed to any attack sub-state
//      (`CombatSystem.js:488,509`, `f.state!==ATTACK`) -- `SetFacing()`
//      below is a real no-op, not a caller error, during
//      kAttackStartup/Active/Recovery. A future two-fighter match driver,
//      which will know both fighters' real positions, is the real caller
//      for every other frame.
//
// PHASE 2 -- REAL COMBAT (HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md,
// steps 5/6 of that audit's proposed sequence; step 7, rounds/timer, is
// addressed separately below, not in this class -- see that paragraph).
//   - Step 5 (position-based hit detection) is deliberately NOT a member
//     of this class -- `CHARACTER/HitmBridge/HitmMeleeHitCheck.h`'s
//     `MeleeHitConnects()` is a pure, standalone port of the real
//     engine's own `_melee()`, taking two `HitmFighterSnapshot`s (this
//     class's own already-public, already-comparable state) plus a
//     `HitmMoveInstance`. Same "lives outside the runtime as a pure
//     consumer" shape as `HitmSpriteDrawData`'s `BuildSpriteDrawData()`.
//   - Step 6: `TakeHit()` now reduces real `hp`, using ONLY data this
//     class already has honest access to: the incoming move's own real,
//     authored `power`, times the real block-chip multiplier
//     (`HitmGameRules::Combat().chip_mult`) when blocking. Three real
//     pieces of the full formula are DELIBERATELY EXCLUDED, not silently
//     defaulted to 1.0 -- each is a real, found, and documented
//     architectural gap, not an oversight:
//       * The attacker's own read-engine damage multiplier (the law
//         "the read engine multiplies OUTPUT, never the table" already
//         lives in `ResolveOutgoingDamage()`, an ATTACKER-side method
//         this defender-focused `TakeHit()` has no access to -- combining
//         them needs a caller that knows both fighters, i.e. a future
//         match driver, which already calls `ResolveOutgoingDamage()` on
//         the attacker and could pass that real number in).
//       * Real combo damage scaling (`game.json`'s real
//         `combat.scaleMin`/`scaleStep`, already imported by
//         `HitmGameRules` -- but applying it needs a real combo-hit
//         counter this class does not track, the same gap Module 5B's
//         own report already named).
//       * The real engine's own `atk.power` stat
//         (`CombatSystem.js:424`, `final=damage*scale*atk.power*...`).
//         Found, this phase, to come from `character.json`'s
//         `stats.power` -- and `character.json` is itself
//         self-labeled `"_generated": "genome_compiler.py"` in the real
//         source tree, the SAME "generated, not authored" category this
//         track has refused to import since its very first module (the
//         basic-normals gap). `character_dna.json`'s own real, already-
//         imported `frames.damageMult` is a DIFFERENT, genuinely
//         authored per-fighter multiplier in the same spirit -- but it
//         is not the same value the real engine actually multiplies by
//         (Brooklyn: `damageMult=1.03` vs. `character.json`'s compiled
//         `power=1.01`), so substituting one for the other would be
//         real data used dishonestly, not the real formula. Left out
//         entirely rather than guessed.
//     Real quirk faithfully preserved: the real engine applies chip
//     damage to `hp` and THEN unconditionally checks `hp<=0` for KO --
//     no special-case preventing a blocked hit from KO'ing
//     (`CombatSystem.js:436,455`). This class does the same: a
//     `blocking=true` `TakeHit()` call CAN result in `kKO` if chip
//     damage is the killing blow. Not a bug -- the real engine's own
//     behavior, ported exactly, not softened.
//   - Step 7 (rounds won / match timer) is deliberately NOT added to
//     this class at all, even though `HitmGameRules::Rounds()` already
//     has the real `to_win`/`timer_seconds` data imported and ready.
//     In the real engine these live on the SHARED match state
//     (`CombatSystem.js`'s own `state.round`/`state.timer`/
//     `state.phase`), never on an individual `Fighter` -- a single
//     fighter genuinely has no "rounds it has won" without a second
//     fighter's outcome to compare against. Inventing a per-fighter
//     rounds counter here would manufacture an incoherent concept the
//     real architecture itself does not have, the same category of
//     mistake as fabricating missing authored data. This is real,
//     match-level state that belongs in the audit's own Phase 3 (the
//     two-fighter match driver), not here.
//
// PHASE 3 -- THE MATCH DRIVER'S OWN TWO REQUIREMENTS OF THIS CLASS
// (`CHARACTER/HitmBridge/HitmMatch.h`, the real Match -> Round -> Timer
// -> Fighter A / Fighter B -> Combat resolution structure the audit's
// Phase 3 called for). Two small, additive changes, both driven by a
// real need the match driver has that a single fighter cannot supply
// itself -- nothing here adds round/timer/match state to this class,
// which stays exactly the boundary the "Step 7" paragraph above draws:
//   1. `Create()` no longer hard-fails a fighter whose real "special"
//      move doesn't extract with today's schema (Rocket's real
//      "Ghost Dash" -- see `HitmMoveInstance.h`'s own top comment for
//      why). That was blocking Rocket from existing as a runtime fighter
//      AT ALL, for a reason unrelated to Ghost Dash itself: a fighter
//      with no working special can still genuinely walk, block, take
//      real damage, and get KO'd -- not having a working special is
//      real, honestly-representable information about this vertical
//      slice's current coverage, not a reason to refuse to construct the
//      fighter. `FrameState::specialMove` is now
//      `std::optional<HitmMoveInstance>`; `kSpecial` input is a real,
//      documented no-op for a fighter with none (falls through to the
//      same "no input recognized" path idle/walking already uses); the
//      new `SpecialMove()`/`HasSpecialMove()` let a caller (the match
//      driver) query which real move, if any, to hand
//      `HitmMeleeHitCheck.h`'s `MeleeHitConnects()` once this fighter
//      reaches `kAttackActive`. This is NOT Ghost Dash -- Rocket's own
//      real rush-type special still does not execute (his `kSpecial`
//      input is simply inert); making it real is the audit's own,
//      still-unauthorized Phase 4.
//   2. A new `ResetForNewRound(x, y, facing)` -- a direct port of the
//      real engine's own `resetRound()` (`CombatSystem.js:38-47`):
//      real `hp` back to `max_hp`, `state` back to `kIdle`, every
//      countdown (`state_frames_remaining`, `state_frame`,
//      `hitstop_frames_remaining`) cleared, position/velocity/facing set
//      to the given real values, `grounded` back to true. Deliberately
//      does NOT touch `meter` or the read-engine's reads -- the real
//      engine's own `resetRound()` doesn't reset `f.meter` or `f.reads`
//      either (confirmed by direct read of its real body: every field it
//      touches is listed above; meter/reads are absent), meaning both
//      genuinely persist across rounds within a real match. Getting this
//      wrong (resetting meter/reads every round) would have been a real,
//      silent fidelity bug -- caught by reading the real function in
//      full rather than assuming "reset" means "reset everything."
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
    // Real hp<=0 (PHASE 2 -- see this header's top comment). A direct
    // port of the real engine's own `_ko()` (CombatSystem.js:458): once
    // entered, terminal -- input is dropped (same "locked" treatment as
    // kAttackStartup/Active/Recovery/kHitstun/kBlockstun), and TakeHit()
    // real no-ops for a fighter already here (a KO'd fighter cannot be
    // hit again). Deliberately NOT implemented here: the real
    // `vy=-9.5, vx=6.5*winner.facing` knockback the real engine applies
    // on KO -- it needs the ATTACKER's own facing, which this
    // defender-focused TakeHit() does not receive (the same "needs an
    // opponent this single-fighter runtime doesn't model" boundary
    // documented throughout this class) -- deferred to whatever
    // eventually calls TakeHit with real two-fighter knowledge, not
    // guessed at here.
    kKO,
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
    // Real: round(1000 * this fighter's own real healthMult). See this
    // header's top comment ("PHASE 1 -- RUNTIME FOUNDATION"). `hp` starts
    // equal to `max_hp` and nothing in this class reduces it yet -- that
    // is real, separately-scoped future work, not silently implied here.
    int max_hp = 0;
    int hp = 0;
    // +1 = facing right, -1 = facing left. Defaults to +1 at Create() and
    // only ever changes via the explicit SetFacing() seam -- see this
    // header's top comment for why this runtime cannot compute a real
    // value on its own.
    int facing = 1;

    bool operator==(const HitmFighterSnapshot&) const = default;
};

class HitmFighterRuntime {
public:
    // Builds a real fighter from already-imported, already-validated real
    // data (Modules 1/2/4). Fails (Result::Fail) on:
    //  - record/genome fighter_id mismatch (same fail-closed pattern as
    //    every prior module's directory/id cross-check)
    //  - no real defense_profile.blockPreference in the genome
    //  - missing/malformed real character_dna.frames.healthMult
    // Does NOT fail when HitmMoveInstance::Extract("special") fails
    // (Rocket's real "Ghost Dash", for one) -- see this header's top
    // comment ("PHASE 3") for why that fighter still gets constructed,
    // just with no working special (SpecialMove() returns nullptr).
    // Never fabricates a fighter from partial data otherwise.
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
    // damage/hitstun/blockstun/meter/hp numbers, COMBAT::ReactionSystem
    // decides the reaction type from this fighter's own real
    // defense_profile.blockPreference as defense_bias. See this header's
    // top comment for why this does not use COMBAT::CollisionEvaluator,
    // and "PHASE 2" for exactly what real damage this applies to hp (and
    // what it deliberately still does not: the attacker's own read-engine
    // multiplier, combo scaling, and the real per-fighter `power` stat --
    // see that comment). Real no-op if this fighter is already kKO (a
    // KO'd fighter cannot be hit again, matching the real engine's own
    // guard).
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
    //
    // Real, documented no-op for a fighter with no real read engine
    // (Rocket/Static) -- see HasReadEngine(). Not an error: there is
    // nothing to gain or lose.
    void GainRead();
    void LoseRead();

    // Real, explicit query -- true for a fighter whose real combat genome
    // has a read_engine (Brooklyn), false for one that genuinely does not
    // (Rocket/Static). GainRead()/LoseRead() are safe to call either way;
    // ReadEngineState() returns nullptr exactly when this is false.
    bool HasReadEngine() const;

    // Sets this fighter's facing direction explicitly (+1 = right, -1 =
    // left) -- see this header's top comment ("PHASE 1") for why this is
    // an explicit seam rather than something this runtime computes on its
    // own, and for the one real rule (facing locks during any attack
    // sub-state) this method DOES enforce itself.
    void SetFacing(int facing);

    // The real move this fighter would execute on a real kSpecial input,
    // if any -- nullptr for a fighter whose real special doesn't extract
    // with today's schema (see this header's top comment, "PHASE 3").
    // Exposed so a caller (a match driver) can retrieve the real
    // range/height/damage data `HitmMeleeHitCheck.h`'s
    // `MeleeHitConnects()` needs once this fighter reaches
    // `kAttackActive`, without this class knowing anything about hit
    // detection itself.
    const HitmMoveInstance* SpecialMove() const;
    bool HasSpecialMove() const;

    // A direct port of the real engine's own `resetRound()` -- see this
    // header's top comment ("PHASE 3") for exactly which real fields
    // this touches (hp, state, every countdown, position/velocity/
    // facing) and which it deliberately does not (meter, read-engine
    // reads -- both genuinely persist across rounds in the real engine).
    // The real caller is a match driver, at the start of a match and
    // after every round reset; this single-fighter runtime has no
    // round/match concept of its own to trigger it internally.
    void ResetForNewRound(float x, float y, int facing);

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
    //
    // Returns nullptr for a fighter with no real read engine
    // (Rocket/Static) -- see HasReadEngine(). Non-null, and unchanged, for
    // every fighter that has one (Brooklyn).
    const HitmReadEngineState* ReadEngineState() const;
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
        // Optional: real data (Rocket/Static genuinely have none) -- see
        // header top comment "PHASE 1". std::nullopt for such a fighter;
        // every member function that touches this checks first, rather
        // than assuming it is always present the way earlier modules did.
        std::optional<HitmReadEngineState> readEngine;
        // Optional: real data -- a fighter whose real special doesn't
        // extract with today's schema (Rocket's "Ghost Dash") still gets
        // constructed, just with no working special. See header top
        // comment "PHASE 3".
        std::optional<HitmMoveInstance> specialMove;
        double defenseBlockPreference;
        // See header top comment "PHASE 1". hp starts at maxHp; nothing
        // in Phase 1 reduces it.
        int maxHp = 0;
        int hp = 0;

        HitmFighterState state = HitmFighterState::kIdle;
        int stateFramesRemaining = 0;
        int stateFrame = 0;  // see header comment "A DELIBERATELY SCOPED EXTENSION"
        int hitstopFramesRemaining = 0;
        double meter = 0.0;
        bool grounded = true;
        int facing = 1;  // see header comment "PHASE 1"; only ever set via SetFacing()
        uint64_t frame = 0;
        combat::ReactionResult lastReaction;
        HitmInputCommand pendingInput = HitmInputCommand::kNeutral;

        FrameState(HitmGameRules rulesIn, std::optional<HitmReadEngineState> readEngineIn,
                   std::optional<HitmMoveInstance> specialMoveIn, double defenseBlockPreferenceIn, int maxHpIn,
                   std::string fighterIdIn)
            : physics(static_cast<float>(rulesIn.Physics().gravity)),
              entityId(fighterIdIn),
              fighterId(std::move(fighterIdIn)),
              rules(std::move(rulesIn)),
              readEngine(std::move(readEngineIn)),
              specialMove(std::move(specialMoveIn)),
              defenseBlockPreference(defenseBlockPreferenceIn),
              maxHp(maxHpIn),
              hp(maxHpIn) {}
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
