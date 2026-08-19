// tests/integration/test_hitm_fighter_runtime.cpp
// ROADMAP.md Track H Module 5A -- the CPU-observable HITM runtime
// vertical slice. Every assertion in this file traces to a real,
// authored HITM value (game.json / combat_genome.json / signature.json,
// all imported via Modules 1/2/4) or to COMBAT::ReactionSystem's own
// existing, unmodified logic -- see HITM_FIGHTER_RUNTIME_REPORT.md for
// the full PROVEN / IMPLEMENTED-BUT-UNVERIFIABLE / NOT-IMPLEMENTED
// accounting this file is evidence for.
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <utility>
#include <vector>

using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmFighterRuntime;
using dominus::character::hitm::HitmFighterSnapshot;
using dominus::character::hitm::HitmFighterState;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmInputCommand;
using dominus::character::hitm::HitmMoveInstance;

namespace {

std::filesystem::path IdentityDir(const std::string& fighter) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_identity") / fighter,
        std::filesystem::path("../tests/fixtures/hitm_identity") / fighter,
        std::filesystem::path("../../tests/fixtures/hitm_identity") / fighter,
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture dir not found: hitm_identity/" + fighter);
}

std::filesystem::path GameRulesPath() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_game_rules/game.json"),
        std::filesystem::path("../tests/fixtures/hitm_game_rules/game.json"),
        std::filesystem::path("../../tests/fixtures/hitm_game_rules/game.json"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture not found: hitm_game_rules/game.json");
}

HitmIdentityRecord RealIdentity(const std::string& fighter) {
    auto r = HitmIdentityImporter::Import(IdentityDir(fighter));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return *r.value;
}

HitmCombatGenome RealGenome(const HitmIdentityRecord& record) {
    auto r = HitmCombatGenome::FromRecord(record);
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}

HitmGameRules RealRules() {
    auto r = HitmGameRules::Import(GameRulesPath());
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}

HitmFighterRuntime MakeBrooklynRuntime() {
    auto identity = RealIdentity("brooklyn");
    auto genome = RealGenome(identity);
    auto rules = RealRules();
    auto result = HitmFighterRuntime::Create(identity, genome, rules);
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}

}  // namespace

// --- 1. Fighter initialization -------------------------------------------

DOMINUS_TEST(HitmFighterRuntime_InitializesFromRealBrooklynData) {
    auto runtime = MakeBrooklynRuntime();
    DOMINUS_EXPECT(runtime.FighterId() == "brooklyn");
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kIdle);

    auto rules = RealRules();
    auto snap = runtime.Snapshot();
    // Real starting position: centered between the real wallL/wallR,
    // standing on the real ground level.
    float expectedX = (static_cast<float>(rules.Physics().wall_l) + static_cast<float>(rules.Physics().wall_r)) / 2.0f;
    DOMINUS_EXPECT(snap.x == expectedX);
    DOMINUS_EXPECT(snap.y == static_cast<float>(rules.Physics().ground));
    DOMINUS_EXPECT(snap.grounded);
    DOMINUS_EXPECT(snap.meter == 0.0);
    DOMINUS_EXPECT(snap.read_engine_reads == 0);
    DOMINUS_EXPECT(snap.frame == 0);
}

DOMINUS_TEST(HitmFighterRuntime_Break_MismatchedIdentityAndGenome_Fails) {
    auto brooklynIdentity = RealIdentity("brooklyn");
    auto rocketGenome = RealGenome(RealIdentity("rocket"));  // real data, wrong fighter
    auto rules = RealRules();
    auto result = HitmFighterRuntime::Create(brooklynIdentity, rocketGenome, rules);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("brooklyn") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("rocket") != std::string::npos);
}

DOMINUS_TEST(HitmFighterRuntime_Break_FighterWithoutReadEngine_Fails) {
    // Isolates the read_engine check specifically. None of the three
    // real fighters combines "special move extracts cleanly" with "no
    // read_engine" (Rocket/Static lack a read_engine AND their real
    // special moves fail extraction for an unrelated, earlier reason --
    // see HitmMoveInstance_Break_* -- so testing Create() against them
    // directly would report the wrong failure reason and not actually
    // exercise this check). The one real, controlled way to isolate it:
    // Brooklyn's real identity (whose special move extracts cleanly)
    // paired with his own real genome data, minus the read_engine key --
    // still 100% real data everywhere else, one deliberate removal.
    auto identity = RealIdentity("brooklyn");
    identity.combat_genome.AsObject().erase("read_engine");
    auto genome = RealGenome(identity);
    DOMINUS_EXPECT(!genome.HasReadEngine());

    auto rules = RealRules();
    auto result = HitmFighterRuntime::Create(identity, genome, rules);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("read_engine") != std::string::npos);
}

// --- 2/3. Idle, movement, jump/gravity ------------------------------------

DOMINUS_TEST(HitmFighterRuntime_MovementUsesRealWalkSpeed) {
    auto runtime = MakeBrooklynRuntime();
    auto rules = RealRules();
    float startX = runtime.Snapshot().x;

    runtime.AdvanceFrame(HitmInputCommand::kRight);
    auto snap = runtime.Snapshot();
    DOMINUS_EXPECT(snap.state == HitmFighterState::kWalking);
    DOMINUS_EXPECT(snap.velocity_x == static_cast<float>(rules.Physics().walk_speed));
    // Position already moved by exactly one frame of the real walk speed
    // -- PHYSICS::PhysicsSystem::Integrate ran within this same
    // AdvanceFrame call.
    DOMINUS_EXPECT(snap.x == startX + static_cast<float>(rules.Physics().walk_speed));

    runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kIdle);
    DOMINUS_EXPECT(runtime.Snapshot().velocity_x == 0.0f);
}

DOMINUS_TEST(HitmFighterRuntime_WallClampUsesRealWallBounds) {
    auto runtime = MakeBrooklynRuntime();
    auto rules = RealRules();
    // Real wallR is close enough to the start position that a handful of
    // real walk-speed steps reaches it -- drive well past that many
    // frames and confirm the clamp holds at the real bound, not beyond.
    for (int i = 0; i < 500; ++i) runtime.AdvanceFrame(HitmInputCommand::kRight);
    DOMINUS_EXPECT(runtime.Snapshot().x == static_cast<float>(rules.Physics().wall_r));
}

DOMINUS_TEST(HitmFighterRuntime_JumpUsesRealJumpVelAndGravity) {
    auto runtime = MakeBrooklynRuntime();
    auto rules = RealRules();

    runtime.AdvanceFrame(HitmInputCommand::kJump);
    auto snap = runtime.Snapshot();
    DOMINUS_EXPECT(snap.state == HitmFighterState::kJumping);
    DOMINUS_EXPECT(!snap.grounded);
    // Real jumpVel applied, then real gravity integrated within the same
    // frame (PhysicsSystem runs after the jump-velocity assignment).
    float expectedVy = static_cast<float>(rules.Physics().jump_vel) + static_cast<float>(rules.Physics().gravity);
    DOMINUS_EXPECT(snap.velocity_y == expectedVy);

    // One more frame: gravity accumulates again, by exactly the real
    // per-frame delta.
    runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    float expectedVy2 = expectedVy + static_cast<float>(rules.Physics().gravity);
    DOMINUS_EXPECT(runtime.Snapshot().velocity_y == expectedVy2);
}

DOMINUS_TEST(HitmFighterRuntime_JumpEventuallyReturnsToRealGroundLevel) {
    auto runtime = MakeBrooklynRuntime();
    auto rules = RealRules();
    runtime.AdvanceFrame(HitmInputCommand::kJump);
    DOMINUS_EXPECT(!runtime.Snapshot().grounded);

    bool landed = false;
    for (int i = 0; i < 200 && !landed; ++i) {
        runtime.AdvanceFrame(HitmInputCommand::kNeutral);
        if (runtime.Snapshot().grounded) landed = true;
    }
    DOMINUS_EXPECT(landed);
    auto snap = runtime.Snapshot();
    DOMINUS_EXPECT(snap.y == static_cast<float>(rules.Physics().ground));
    DOMINUS_EXPECT(snap.velocity_y == 0.0f);
    DOMINUS_EXPECT(snap.state == HitmFighterState::kIdle);
}

DOMINUS_TEST(HitmFighterRuntime_JumpInputIgnoredWhileAirborne_NoDoubleJump) {
    auto runtime = MakeBrooklynRuntime();
    auto rules = RealRules();
    runtime.AdvanceFrame(HitmInputCommand::kJump);
    float vyAfterFirstJump = runtime.Snapshot().velocity_y;

    // Real data has no double jump -- a second kJump input while airborne
    // must be dropped, not re-apply jumpVel.
    runtime.AdvanceFrame(HitmInputCommand::kJump);
    float vyAfterSecondInput = runtime.Snapshot().velocity_y;
    // If the jump had re-applied, velocity_y would snap back to
    // jumpVel(+gravity); instead it should simply be one more frame of
    // gravity added to where it already was.
    DOMINUS_EXPECT(vyAfterSecondInput == vyAfterFirstJump + static_cast<float>(rules.Physics().gravity));
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kJumping);
}

// --- 4. Attack / state transition using real frame counts -----------------

DOMINUS_TEST(HitmFighterRuntime_AttackTransitionsThroughRealFrameCounts) {
    auto identity = RealIdentity("brooklyn");
    auto specialMove = HitmMoveInstance::Extract(identity, "special");
    DOMINUS_EXPECT(specialMove.ok);
    int startup = specialMove.value->move_def.frames.startup;    // real: 14
    int active = specialMove.value->move_def.frames.active;      // real: 4
    int recovery = specialMove.value->move_def.frames.recovery;  // real: 18

    auto runtime = MakeBrooklynRuntime();
    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kAttackStartup);

    for (int i = 1; i < startup; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kAttackActive);

    for (int i = 0; i < active; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kAttackRecovery);

    for (int i = 0; i < recovery; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kIdle);
}

DOMINUS_TEST(HitmFighterRuntime_Break_SpecialInputIgnoredMidAttack) {
    auto runtime = MakeBrooklynRuntime();
    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    int remainingAfterTrigger = runtime.Snapshot().state_frames_remaining;

    // Re-triggering mid-startup must be dropped, not restart the attack.
    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kAttackStartup);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == remainingAfterTrigger - 1);
}

// --- 5/8. Hit/damage resolution, meter, hitstop ---------------------------

DOMINUS_TEST(HitmFighterRuntime_TakeHitAsDefenderUsesRealNumbers) {
    auto identity = RealIdentity("brooklyn");
    auto incoming = *HitmMoveInstance::Extract(identity, "special").value;  // real damage=62, hitstun=34, hitstop=heavy
    auto rules = RealRules();

    auto runtime = MakeBrooklynRuntime();
    runtime.TakeHit(incoming, /*blocking=*/false);

    auto snap = runtime.Snapshot();
    DOMINUS_EXPECT(snap.state == HitmFighterState::kHitstun);
    DOMINUS_EXPECT(snap.state_frames_remaining == 34);  // real hitstun_frames
    DOMINUS_EXPECT(snap.hitstop_frames_remaining == static_cast<int>(rules.Combat().hitstop_heavy));  // real: 7
    DOMINUS_EXPECT(snap.meter == rules.Meter().on_hit_take);  // real: 5

    // Brooklyn's real defense_profile.blockPreference (0.18) shifts
    // COMBAT::ReactionSystem's real thresholds down; 62 damage clears
    // even the shifted knockdown threshold -- a real, computed reaction,
    // not an assumed one.
    DOMINUS_EXPECT(runtime.LastReaction().type == dominus::combat::ReactionType::kKnockdown);
}

DOMINUS_TEST(HitmFighterRuntime_TakeHitWhileBlockingUsesRealBlockNumbers) {
    auto identity = RealIdentity("brooklyn");
    auto incoming = *HitmMoveInstance::Extract(identity, "special").value;
    auto rules = RealRules();

    auto runtime = MakeBrooklynRuntime();
    runtime.TakeHit(incoming, /*blocking=*/true);

    auto snap = runtime.Snapshot();
    DOMINUS_EXPECT(snap.state == HitmFighterState::kBlockstun);
    DOMINUS_EXPECT(snap.state_frames_remaining == 13);  // real blockstun_frames
    DOMINUS_EXPECT(snap.meter == rules.Meter().on_block_take);  // real: 2
    DOMINUS_EXPECT(runtime.LastReaction().type == dominus::combat::ReactionType::kNone);
    DOMINUS_EXPECT(runtime.LastReaction().motion_trigger == "block_impact");
}

DOMINUS_TEST(HitmFighterRuntime_HitstopFreezesEverythingExceptItself) {
    auto identity = RealIdentity("brooklyn");
    auto incoming = *HitmMoveInstance::Extract(identity, "special").value;
    auto rules = RealRules();
    int hitstopFrames = static_cast<int>(rules.Combat().hitstop_heavy);  // real: 7

    auto runtime = MakeBrooklynRuntime();
    runtime.TakeHit(incoming, false);
    int hitstunBefore = runtime.Snapshot().state_frames_remaining;

    for (int i = 0; i < hitstopFrames; ++i) {
        runtime.AdvanceFrame(HitmInputCommand::kNeutral);
        // Hitstun countdown must NOT move while hitstop is active.
        DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == hitstunBefore);
    }
    DOMINUS_EXPECT(runtime.Snapshot().hitstop_frames_remaining == 0);

    // Now hitstop is over -- the next frame genuinely decrements hitstun.
    runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == hitstunBefore - 1);
}

DOMINUS_TEST(HitmFighterRuntime_MeterClampedAtRealMax) {
    auto identity = RealIdentity("brooklyn");
    auto move = *HitmMoveInstance::Extract(identity, "special").value;  // real meterGain=14
    auto rules = RealRules();

    auto runtime = MakeBrooklynRuntime();
    for (int i = 0; i < 20; ++i) runtime.ResolveOutgoingHitLanded(move);
    DOMINUS_EXPECT(runtime.Snapshot().meter == rules.Meter().max);  // real: 100, clamped not overflowed
}

// --- 8/10. Read engine transitions through the runtime ---------------------

DOMINUS_TEST(HitmFighterRuntime_ReadEngineTransitionsAndMultipliesRealDamage) {
    auto identity = RealIdentity("brooklyn");
    auto move = *HitmMoveInstance::Extract(identity, "special").value;  // real power=62

    auto runtime = MakeBrooklynRuntime();
    DOMINUS_EXPECT(runtime.Snapshot().read_engine_reads == 0);
    DOMINUS_EXPECT(runtime.ResolveOutgoingDamage(move) == 62.0);  // tier 0 mult 1.0

    runtime.GainRead();
    runtime.GainRead();
    runtime.GainRead();
    DOMINUS_EXPECT(runtime.Snapshot().read_engine_reads == 3);
    // Real tier-3 multiplier: 1.21. 62 * 1.21 = 75.02.
    DOMINUS_EXPECT(runtime.ResolveOutgoingDamage(move) == 62.0 * 1.21);

    runtime.LoseRead();
    DOMINUS_EXPECT(runtime.Snapshot().read_engine_reads == 2);
}

// --- 9. Determinism ---------------------------------------------------------

DOMINUS_TEST(HitmFighterRuntime_IdenticalInputSequencesProduceIdenticalStates) {
    auto runtimeA = MakeBrooklynRuntime();
    auto runtimeB = MakeBrooklynRuntime();

    std::vector<HitmInputCommand> script = {
        HitmInputCommand::kRight, HitmInputCommand::kRight, HitmInputCommand::kRight, HitmInputCommand::kJump,
        HitmInputCommand::kNeutral, HitmInputCommand::kNeutral, HitmInputCommand::kNeutral, HitmInputCommand::kNeutral,
        HitmInputCommand::kNeutral, HitmInputCommand::kNeutral, HitmInputCommand::kNeutral, HitmInputCommand::kNeutral,
        HitmInputCommand::kLeft,   HitmInputCommand::kLeft,    HitmInputCommand::kSpecial,  HitmInputCommand::kNeutral,
        HitmInputCommand::kNeutral, HitmInputCommand::kBlock,  HitmInputCommand::kBlock,    HitmInputCommand::kNeutral,
    };

    for (auto input : script) {
        runtimeA.AdvanceFrame(input);
        runtimeB.AdvanceFrame(input);
        DOMINUS_EXPECT(runtimeA.Snapshot() == runtimeB.Snapshot());
    }

    // A real, non-trivial state was actually reached -- this determinism
    // proof isn't vacuous over an unchanging idle fighter.
    DOMINUS_EXPECT(runtimeA.Snapshot().frame == script.size());
}

DOMINUS_TEST(HitmFighterRuntime_DivergentInputSequencesProduceDivergentStates) {
    // The negative control for the determinism test above: DIFFERENT
    // input must produce different state, proving the equality check
    // above isn't trivially true (e.g. from an always-default-constructed
    // snapshot).
    auto runtimeA = MakeBrooklynRuntime();
    auto runtimeB = MakeBrooklynRuntime();
    runtimeA.AdvanceFrame(HitmInputCommand::kRight);
    runtimeB.AdvanceFrame(HitmInputCommand::kLeft);
    DOMINUS_EXPECT(!(runtimeA.Snapshot() == runtimeB.Snapshot()));
}

// --- 6/7. Lifetime safety: construction -> move/return -> frame execution
// -> destruction. See HitmFighterRuntime.h's top comment for the real bug
// these tests guard against: a WorldTick-registered closure capturing
// `this` went dangling the moment the object was moved, because a raw
// `HitmFighterRuntime*` baked into a std::function does not get rewritten
// by a move. The fix makes the registered closure capture a stable
// FrameState* instead -- these tests exercise every relocation path that
// could have re-triggered the old bug's symptom (a segfault or a frame
// silently not applying real HITM data), specifically to prove none of
// them do.

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_MoveConstructThenAdvanceFrame) {
    auto original = MakeBrooklynRuntime();
    auto rules = RealRules();
    float startX = original.Snapshot().x;

    // Move-construct a second wrapper from the first. The registered
    // WorldTick closure must keep pointing at the real, still-live
    // FrameState -- not at `original`, which is now moved-from.
    HitmFighterRuntime moved(std::move(original));
    moved.AdvanceFrame(HitmInputCommand::kRight);

    auto snap = moved.Snapshot();
    DOMINUS_EXPECT(snap.state == HitmFighterState::kWalking);
    // Real walkSpeed still drives the moved-to instance correctly.
    DOMINUS_EXPECT(snap.x == startX + static_cast<float>(rules.Physics().walk_speed));
    DOMINUS_EXPECT(moved.FighterId() == "brooklyn");
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_MoveAssignThenAdvanceFrame) {
    auto destination = MakeBrooklynRuntime();
    auto source = MakeBrooklynRuntime();
    source.AdvanceFrame(HitmInputCommand::kRight);
    source.AdvanceFrame(HitmInputCommand::kRight);
    uint64_t sourceFrameBeforeAssign = source.Snapshot().frame;

    // Move-assign: destination's original FrameState is destroyed, its
    // WorldTick closure (which pointed at ITS OWN FrameState, never at
    // `destination` the wrapper) is destroyed along with it -- no
    // dangling reference is possible because nothing outlives its own
    // FrameState. destination now owns source's former FrameState.
    destination = std::move(source);
    DOMINUS_EXPECT(destination.Snapshot().frame == sourceFrameBeforeAssign);

    destination.AdvanceFrame(HitmInputCommand::kJump);
    auto rules = RealRules();
    DOMINUS_EXPECT(destination.Snapshot().state == HitmFighterState::kJumping);
    DOMINUS_EXPECT(destination.Snapshot().velocity_y ==
                    static_cast<float>(rules.Physics().jump_vel) + static_cast<float>(rules.Physics().gravity));
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_MultipleSequentialMoves) {
    // Chains several relocations -- the shape Result<T>/std::optional/
    // return-by-value actually produce in practice (Create() alone
    // already moves the object at least twice before a caller ever sees
    // it). Real Brooklyn data must still drive the runtime correctly at
    // the end of the chain.
    auto rules = RealRules();
    HitmFighterRuntime a = MakeBrooklynRuntime();
    HitmFighterRuntime b = std::move(a);
    HitmFighterRuntime c = std::move(b);
    HitmFighterRuntime d = std::move(c);

    // Expected position accumulated the same way the runtime itself
    // accumulates it (repeated float addition, not a single
    // multiplication) -- float addition is not associative with
    // multiplication at this precision (verified: 5 sequential +4.4f
    // steps land one ULP-scale away from a single *5.0f), and this test
    // is about lifetime safety, not floating-point rounding, so it
    // compares against the same accumulation method under test.
    float startX = d.Snapshot().x;
    float expectedX = startX;
    for (int i = 0; i < 5; ++i) expectedX += static_cast<float>(rules.Physics().walk_speed);
    for (int i = 0; i < 5; ++i) d.AdvanceFrame(HitmInputCommand::kRight);
    DOMINUS_EXPECT(d.Snapshot().x == expectedX);
    DOMINUS_EXPECT(d.Snapshot().frame == 5);
    DOMINUS_EXPECT(d.FighterId() == "brooklyn");
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_OriginalWrapperDestroyedAfterMove) {
    // The original wrapper's scope ends (its destructor runs on a
    // moved-from, null unique_ptr -- a safe no-op) while the moved-to
    // instance is still very much alive and in use. This is the literal
    // shape of the bug: does anything the closure touches still exist
    // after the ORIGINAL object is gone?
    auto MakeAndReturn = []() -> HitmFighterRuntime {
        auto inner = MakeBrooklynRuntime();
        inner.AdvanceFrame(HitmInputCommand::kRight);  // real work done before the move-out
        return inner;  // `inner`'s destructor runs immediately after this move
    };

    auto rules = RealRules();
    // Same accumulation-consistent expected value as
    // MultipleSequentialMoves above -- one real frame already happened
    // inside MakeAndReturn() before the wrapper it ran on was destroyed.
    float expectedX = (static_cast<float>(rules.Physics().wall_l) + static_cast<float>(rules.Physics().wall_r)) / 2.0f;
    expectedX += static_cast<float>(rules.Physics().walk_speed);  // the frame run inside MakeAndReturn()

    HitmFighterRuntime runtime = MakeAndReturn();
    // `inner` from inside the lambda is unambiguously destroyed by now.
    for (int i = 0; i < 10; ++i) {
        runtime.AdvanceFrame(HitmInputCommand::kRight);
        expectedX += static_cast<float>(rules.Physics().walk_speed);
    }
    DOMINUS_EXPECT(runtime.Snapshot().frame == 11);  // 1 from inside the lambda + 10 here
    DOMINUS_EXPECT(runtime.Snapshot().x == expectedX);
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_StoredInVectorAndReallocated) {
    // std::vector reallocation is a real, common relocation path
    // (push_back beyond capacity move-constructs every existing element
    // into new storage and destroys the old storage) -- exactly the
    // "stored in containers" case. Reserve nothing, so growth is
    // guaranteed to reallocate at least once across these insertions.
    std::vector<HitmFighterRuntime> fighters;
    for (int i = 0; i < 8; ++i) {
        fighters.push_back(MakeBrooklynRuntime());
    }
    DOMINUS_EXPECT(fighters.size() == 8);

    auto rules = RealRules();
    for (auto& f : fighters) {
        float startX = f.Snapshot().x;
        f.AdvanceFrame(HitmInputCommand::kRight);
        f.AdvanceFrame(HitmInputCommand::kJump);
        auto snap = f.Snapshot();
        DOMINUS_EXPECT(snap.state == HitmFighterState::kJumping);
        DOMINUS_EXPECT(snap.x == startX + static_cast<float>(rules.Physics().walk_speed));
        DOMINUS_EXPECT(snap.velocity_y ==
                        static_cast<float>(rules.Physics().jump_vel) + static_cast<float>(rules.Physics().gravity));
    }

    // Every element independently reached frame 2 -- proves each
    // fighter's own FrameState (and its own registered WorldTick
    // closure) survived the vector's internal reallocation(s) without
    // cross-talk between fighters.
    for (auto& f : fighters) {
        DOMINUS_EXPECT(f.Snapshot().frame == 2);
    }
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_ManyMovesThenManyFrames) {
    // An aggressive stress case: several real relocations (each a genuine
    // move-construction into a new named object, not a redundant cast)
    // followed by a long real run (attack cycle + landing + more
    // movement), maximizing exposure for any lingering lifetime issue --
    // not just a couple of frames right after a single move.
    HitmFighterRuntime hop1 = MakeBrooklynRuntime();
    HitmFighterRuntime hop2 = std::move(hop1);
    HitmFighterRuntime hop3 = std::move(hop2);
    HitmFighterRuntime runtime = std::move(hop3);

    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    for (int i = 0; i < 40; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kIdle);  // full 14+4+18=36 frame cycle completed

    runtime.AdvanceFrame(HitmInputCommand::kJump);
    while (!runtime.Snapshot().grounded) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state == HitmFighterState::kIdle);
    DOMINUS_EXPECT(runtime.Snapshot().frame > 40);
    DOMINUS_EXPECT(runtime.FighterId() == "brooklyn");
}

// --- The explicit critical-sequence tests, named to match exactly what
// was asked for: construct -> register WorldTick callback -> move runtime
// -> execute frame -> destroy runtime -> no callback into a dead object.
// Run under both a normal build and an ASan+UBSan build (see
// HITM_FIGHTER_RUNTIME_REPORT.md for both sets of results) -- these tests
// don't change between builds, the sanitizer instrumentation is what
// makes a dead-object access unmissable rather than "didn't crash today."

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_ConstructionActuallyRegistersAWorldTickSystem) {
    // Not "AdvanceFrame doesn't crash" -- specifically that the
    // registered WorldTick system genuinely runs and mutates real state,
    // proving Create() really did register something rather than
    // AdvanceFrame silently being a no-op. frame_ only increments inside
    // RunOneFrame, which only ever runs via the registered closure.
    auto runtime = MakeBrooklynRuntime();
    DOMINUS_EXPECT(runtime.Snapshot().frame == 0);
    runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().frame == 1);
    runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().frame == 2);
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_ConstructMoveExecuteDestroy) {
    // The literal minimal critical sequence: construct -> move -> execute
    // a frame -> destroy (falls out of scope at the end of this test).
    // Real Brooklyn walkSpeed must still drive the post-move instance.
    auto rules = RealRules();
    HitmFighterRuntime original = MakeBrooklynRuntime();
    float startX = original.Snapshot().x;

    HitmFighterRuntime moved(std::move(original));  // construct -> move
    moved.AdvanceFrame(HitmInputCommand::kRight);   // execute a frame after the move

    DOMINUS_EXPECT(moved.Snapshot().frame == 1);
    DOMINUS_EXPECT(moved.Snapshot().x == startX + static_cast<float>(rules.Physics().walk_speed));
    // `moved` is destroyed at end of scope -- its FrameState (and the
    // WorldTick closure inside its own World) are destroyed together,
    // atomically, with nothing else able to reference either afterward.
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_ConstructMoveMoveAgainExecuteDestroy) {
    // The "ideally also" sequence: construct -> move -> move again ->
    // execute -> destroy.
    auto rules = RealRules();
    HitmFighterRuntime original = MakeBrooklynRuntime();
    float startX = original.Snapshot().x;

    HitmFighterRuntime moved1(std::move(original));
    HitmFighterRuntime moved2(std::move(moved1));
    moved2.AdvanceFrame(HitmInputCommand::kRight);
    moved2.AdvanceFrame(HitmInputCommand::kRight);

    DOMINUS_EXPECT(moved2.Snapshot().frame == 2);
    float expectedX = startX;
    expectedX += static_cast<float>(rules.Physics().walk_speed);
    expectedX += static_cast<float>(rules.Physics().walk_speed);
    DOMINUS_EXPECT(moved2.Snapshot().x == expectedX);
    // moved2 destroyed at end of scope; moved1/original are already
    // moved-from (null unique_ptr, safe no-op destructors).
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_DestructionDoesNotAffectSiblingRuntimeOrItsWorld) {
    // Two independent runtimes -- each owns its own World/WorldTick
    // registration entirely privately (per-FrameState, not shared).
    // Destroying one must have zero effect on the other: no shared
    // registry, no shared registered-system list, nothing the survivor's
    // ticks could dangle into. "Destruction while the world/tick system
    // remains alive" + "subsequent world ticks after runtime destruction"
    // are both this test, from the survivor's point of view.
    auto rules = RealRules();
    auto survivor = MakeBrooklynRuntime();
    float survivorStartX = survivor.Snapshot().x;

    {
        auto doomed = MakeBrooklynRuntime();
        doomed.AdvanceFrame(HitmInputCommand::kRight);
        doomed.AdvanceFrame(HitmInputCommand::kJump);
        DOMINUS_EXPECT(doomed.Snapshot().state == HitmFighterState::kJumping);
        // `doomed`'s FrameState -- its World, its registered WorldTick
        // system, its RigidBody/SpatialComponent -- is fully destroyed
        // here, at the closing brace, while `survivor`'s own World is
        // still very much alive and about to keep ticking.
    }

    // The survivor keeps ticking correctly, with real HITM data, entirely
    // unaffected by the sibling's destruction -- proves no cross-talk
    // and that a subsequent world.Tick() call (survivor's own, real,
    // still-live World) is unaffected by an unrelated runtime's teardown.
    for (int i = 0; i < 5; ++i) survivor.AdvanceFrame(HitmInputCommand::kRight);
    float expectedX = survivorStartX;
    for (int i = 0; i < 5; ++i) expectedX += static_cast<float>(rules.Physics().walk_speed);
    DOMINUS_EXPECT(survivor.Snapshot().x == expectedX);
    DOMINUS_EXPECT(survivor.Snapshot().frame == 5);
    DOMINUS_EXPECT(survivor.FighterId() == "brooklyn");
}

DOMINUS_TEST(HitmFighterRuntime_LifetimeSafety_RepeatedConstructDestroyCyclesStressAllocatorReuse) {
    // The strongest bait for a stale-pointer bug under a normal
    // allocator: repeatedly destroy a FrameState and immediately
    // allocate a new one, which frequently reuses the same freed
    // address. A dangling WorldTick closure from a prior cycle touching
    // that address would corrupt (or, under ASan, immediately flag) the
    // NEW cycle's live FrameState. Every cycle's real Brooklyn data must
    // come back correct, not just "no crash."
    auto rules = RealRules();
    for (int cycle = 0; cycle < 25; ++cycle) {
        auto runtime = MakeBrooklynRuntime();
        float startX = runtime.Snapshot().x;
        runtime.AdvanceFrame(HitmInputCommand::kRight);
        runtime.AdvanceFrame(HitmInputCommand::kRight);
        DOMINUS_EXPECT(runtime.Snapshot().frame == 2);
        DOMINUS_EXPECT(runtime.Snapshot().x ==
                        startX + static_cast<float>(rules.Physics().walk_speed) + static_cast<float>(rules.Physics().walk_speed));
        DOMINUS_EXPECT(runtime.FighterId() == "brooklyn");
        // `runtime` destroyed here, every iteration, immediately before
        // the next cycle allocates a fresh one.
    }
}
