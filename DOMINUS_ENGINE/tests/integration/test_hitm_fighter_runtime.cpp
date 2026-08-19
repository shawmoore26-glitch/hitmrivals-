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
