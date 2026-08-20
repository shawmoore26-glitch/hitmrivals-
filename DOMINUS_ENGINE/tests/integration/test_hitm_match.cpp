// tests/integration/test_hitm_match.cpp
// ROADMAP.md Track H, Phase 3 (HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md)
// -- the first real, complete, CPU-observable two-fighter HITM match.
// Every assertion here traces to a real, authored HITM value (game.json/
// combat_genome.json/signature.json/character_dna.json, all imported via
// Modules 1/2/4) or to a real, cited hitm-engine CombatSystem.js formula
// or hardcoded constant -- see HitmMatch.h's own header comment for
// every citation.
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmMatch.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>
#include <utility>
#include <vector>

using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmFighterState;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmInputCommand;
using dominus::character::hitm::HitmMatch;
using dominus::character::hitm::HitmMatchPhase;

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

HitmMatch MakeBrooklynVsRocketMatch() {
    auto identityA = RealIdentity("brooklyn");
    auto genomeA = RealGenome(identityA);
    auto identityB = RealIdentity("rocket");
    auto genomeB = RealGenome(identityB);
    auto rules = RealRules();
    auto result = HitmMatch::Create(identityA, genomeA, identityB, genomeB, rules);
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}

void SkipRoundIntro(HitmMatch& match) {
    while (match.Snapshot().phase == HitmMatchPhase::kRoundIntro) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    }
}

// Drives one full real Brooklyn-special attack cycle
// (startup -> active -> recovery -> idle) to completion, or until the
// round itself ends (a KO mid-cycle). Assumes fighter A is currently
// action-ready (kIdle/kWalking) when called.
void ThrowOneBrooklynSpecial(HitmMatch& match) {
    match.AdvanceFrame(HitmInputCommand::kSpecial, HitmInputCommand::kNeutral);
    while (match.Snapshot().phase == HitmMatchPhase::kFight && match.Snapshot().fighter_a.state != HitmFighterState::kIdle) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    }
}

}  // namespace

// --- 1. Match initialization: real starting positions/facing/timer -------

DOMINUS_TEST(HitmMatch_Create_InitializesRealStartingPositionsFacingAndTimer) {
    auto match = MakeBrooklynVsRocketMatch();
    auto rules = RealRules();
    auto snap = match.Snapshot();

    DOMINUS_EXPECT(snap.phase == HitmMatchPhase::kRoundIntro);
    DOMINUS_EXPECT(snap.round == 1);
    DOMINUS_EXPECT(snap.round_intro_frames_remaining == 120);  // real CombatSystem.js:31
    DOMINUS_EXPECT(snap.timer_frames_remaining == static_cast<int>(rules.Rounds().timer_seconds) * 60);
    DOMINUS_EXPECT(snap.fighter_a_rounds_won == 0);
    DOMINUS_EXPECT(snap.fighter_b_rounds_won == 0);
    DOMINUS_EXPECT(!snap.match_over);

    // Real per-match starting positions/facing (CombatSystem.js:26).
    DOMINUS_EXPECT(snap.fighter_a.x == 300.0f);
    DOMINUS_EXPECT(snap.fighter_a.facing == 1);
    DOMINUS_EXPECT(snap.fighter_b.x == 760.0f);
    DOMINUS_EXPECT(snap.fighter_b.facing == -1);
    DOMINUS_EXPECT(snap.fighter_a.y == static_cast<float>(rules.Physics().ground));
    DOMINUS_EXPECT(snap.fighter_b.y == static_cast<float>(rules.Physics().ground));

    // Real hp: Brooklyn round(1000*0.94)=940, Rocket round(1000*1.09)=1090.
    DOMINUS_EXPECT(snap.fighter_a.hp == 940);
    DOMINUS_EXPECT(snap.fighter_a.max_hp == 940);
    DOMINUS_EXPECT(snap.fighter_b.hp == 1090);
    DOMINUS_EXPECT(snap.fighter_b.max_hp == 1090);

    DOMINUS_EXPECT(snap.fighter_a.state == HitmFighterState::kIdle);
    DOMINUS_EXPECT(snap.fighter_b.state == HitmFighterState::kIdle);
}

// --- 2. Round intro: real 120-frame window, nothing ticks -----------------

DOMINUS_TEST(HitmMatch_RoundIntro_NeitherFighterTicksUntilRealIntroWindowElapses) {
    auto match = MakeBrooklynVsRocketMatch();
    for (int i = 0; i < 119; ++i) {
        // Real input during round intro has no effect at all -- even
        // movement commands are dropped, matching CombatSystem.js's own
        // early `return;` right after the intro check.
        match.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kLeft);
        DOMINUS_EXPECT(match.Snapshot().phase == HitmMatchPhase::kRoundIntro);
        DOMINUS_EXPECT(match.Snapshot().fighter_a.frame == 0);
        DOMINUS_EXPECT(match.Snapshot().fighter_a.x == 300.0f);
        DOMINUS_EXPECT(match.Snapshot().fighter_b.x == 760.0f);
    }
    match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(match.Snapshot().phase == HitmMatchPhase::kFight);
    // The transition frame itself still doesn't tick either fighter --
    // matches the real engine's own `return;` unconditionally following
    // the intro check.
    DOMINUS_EXPECT(match.Snapshot().fighter_a.frame == 0);
}

// --- 3. Action -> attack -> real _melee -> real damage, exactly once -----

DOMINUS_TEST(HitmMatch_Fight_BrooklynSpecialConnectsExactlyOnceAndDealsRealDamage) {
    auto match = MakeBrooklynVsRocketMatch();
    SkipRoundIntro(match);

    // Real: close the real 460px starting gap until Brooklyn's real
    // 88-range special can reach Rocket. 75 real frames of Brooklyn's
    // own real walkSpeed (4.4/frame) lands at x=630, well inside the
    // real reach window MeleeHitConnects' own tests already proved
    // (300+70=hx=... this specific value re-derived and boundary-tested
    // there; here the scenario is realistic-position, not boundary).
    for (int i = 0; i < 75; ++i) {
        match.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kNeutral);
    }
    auto beforeAttack = match.Snapshot();
    // Not an exact literal: 75 repeated real +4.4 float additions
    // genuinely drift a hair below 630.0 (float accumulation, not a
    // bug) -- the real, meaningful property this scenario needs is
    // "inside Brooklyn's real 88-range reach window" (already boundary-
    // proven exactly, in HitmMeleeHitCheck's own tests), not a specific
    // literal position.
    DOMINUS_EXPECT(beforeAttack.fighter_a.x > 620.0f && beforeAttack.fighter_a.x < 630.5f);
    DOMINUS_EXPECT(beforeAttack.fighter_a.state == HitmFighterState::kWalking);
    DOMINUS_EXPECT(beforeAttack.fighter_a.facing == 1);  // real continuous auto-facing, toward Rocket
    DOMINUS_EXPECT(beforeAttack.fighter_b.hp == beforeAttack.fighter_b.max_hp);  // untouched so far

    // Real: 14 real frames (Brooklyn's own real startup) to reach the
    // real first active frame, where the real hit resolves.
    match.AdvanceFrame(HitmInputCommand::kSpecial, HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(match.Snapshot().fighter_a.state == HitmFighterState::kAttackStartup);
    for (int i = 1; i < 14; ++i) match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);

    auto afterHit = match.Snapshot();
    DOMINUS_EXPECT(afterHit.fighter_a.state == HitmFighterState::kAttackActive);
    DOMINUS_EXPECT(afterHit.fighter_a.state_frame == 0);
    DOMINUS_EXPECT(afterHit.fighter_b.state == HitmFighterState::kHitstun);
    DOMINUS_EXPECT(afterHit.fighter_b.hp == afterHit.fighter_b.max_hp - 62);  // real Brooklyn power, unblocked

    // Real: the hit resolves exactly once per activation -- Rocket's hp
    // must not decrease again across the rest of Brooklyn's real 4-frame
    // active window (state_frame is only 0 on the one frame just
    // checked; Rocket is also real-hitstop-frozen for a few of these,
    // which independently guarantees no further change).
    int hpAfterHit = afterHit.fighter_b.hp;
    for (int i = 0; i < 3; ++i) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
        DOMINUS_EXPECT(match.Snapshot().fighter_b.hp == hpAfterHit);
    }
}

// --- 4. Blocked attack: real chip damage only -----------------------------

DOMINUS_TEST(HitmMatch_Fight_BlockedAttackDealsRealChipDamageOnly) {
    auto match = MakeBrooklynVsRocketMatch();
    auto rules = RealRules();
    SkipRoundIntro(match);

    for (int i = 0; i < 75; ++i) match.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kBlock);
    DOMINUS_EXPECT(match.Snapshot().fighter_b.state == HitmFighterState::kBlockingStance);
    DOMINUS_EXPECT(match.Snapshot().fighter_b.x == 760.0f);  // held ground while blocking

    match.AdvanceFrame(HitmInputCommand::kSpecial, HitmInputCommand::kBlock);
    for (int i = 1; i < 14; ++i) match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kBlock);

    auto snap = match.Snapshot();
    DOMINUS_EXPECT(snap.fighter_b.state == HitmFighterState::kBlockstun);
    // Same floating-point operation sequence as production
    // (HitmFighterRuntime.cpp's TakeHit): power * real chip_mult, rounded.
    int expectedChip = static_cast<int>(std::lround(62.0 * rules.Combat().chip_mult));
    DOMINUS_EXPECT(snap.fighter_b.hp == snap.fighter_b.max_hp - expectedChip);
}

// --- 5. KO -> round reset -> round win, meter/reads persist ---------------

DOMINUS_TEST(HitmMatch_KO_EndsRoundResetsBothFightersAndAwardsRealRoundWin) {
    auto match = MakeBrooklynVsRocketMatch();
    auto rules = RealRules();
    SkipRoundIntro(match);

    for (int i = 0; i < 75; ++i) match.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kNeutral);

    // Real: Rocket's hp = round(1000*1.09) = 1090; Brooklyn's real
    // special deals 62 unblocked per real hit -- repeat the real full
    // attack cycle (startup+active+recovery) until Rocket is KO'd.
    int maxCycles = 1090 / 62 + 3;
    for (int i = 0; i < maxCycles && match.Snapshot().phase == HitmMatchPhase::kFight; ++i) {
        ThrowOneBrooklynSpecial(match);
    }
    DOMINUS_EXPECT(match.Snapshot().phase == HitmMatchPhase::kKO);
    DOMINUS_EXPECT(match.Snapshot().fighter_b.state == HitmFighterState::kKO);
    DOMINUS_EXPECT(match.Snapshot().fighter_b.hp == 0);
    DOMINUS_EXPECT(match.Snapshot().ko_frames_remaining == 150);  // real CombatSystem.js:460

    // Real: Brooklyn's meter (14 meterGain + 8 onHitGive per real landed
    // hit) clamps at the real max well before 17 real hits land.
    DOMINUS_EXPECT(match.Snapshot().fighter_a.meter == rules.Meter().max);

    // Real: the post-KO settle window (150 frames), then the round
    // resolves -- fighter A's real rounds-won increments, both fighters
    // real-reset to the real per-match start.
    for (int i = 0; i < 150; ++i) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    }
    auto snap = match.Snapshot();
    DOMINUS_EXPECT(snap.phase == HitmMatchPhase::kRoundIntro);
    DOMINUS_EXPECT(snap.round == 2);
    DOMINUS_EXPECT(snap.fighter_a_rounds_won == 1);
    DOMINUS_EXPECT(snap.fighter_b_rounds_won == 0);
    DOMINUS_EXPECT(!snap.match_over);  // real rounds.toWin=2, only 1 round won so far

    // Real: both fighters real-reset to the real per-match start.
    DOMINUS_EXPECT(snap.fighter_a.x == 300.0f);
    DOMINUS_EXPECT(snap.fighter_b.x == 760.0f);
    DOMINUS_EXPECT(snap.fighter_a.hp == snap.fighter_a.max_hp);
    DOMINUS_EXPECT(snap.fighter_b.hp == snap.fighter_b.max_hp);
    DOMINUS_EXPECT(snap.fighter_a.state == HitmFighterState::kIdle);
    DOMINUS_EXPECT(snap.fighter_b.state == HitmFighterState::kIdle);

    // Real, evidenced fidelity: meter genuinely persists across rounds
    // (hitm-engine's own resetRound() never resets f.meter -- see
    // HitmFighterRuntime.h's own "PHASE 3" comment).
    DOMINUS_EXPECT(snap.fighter_a.meter == rules.Meter().max);
}

// --- 6. Match win: real rounds.toWin ---------------------------------------

DOMINUS_TEST(HitmMatch_MatchWin_AfterRealRoundsToWin) {
    auto match = MakeBrooklynVsRocketMatch();
    auto rules = RealRules();
    int roundsToWin = static_cast<int>(rules.Rounds().to_win);
    DOMINUS_EXPECT(roundsToWin == 2);  // real game.json value

    for (int round = 0; round < roundsToWin; ++round) {
        SkipRoundIntro(match);
        for (int i = 0; i < 75; ++i) match.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kNeutral);
        int maxCycles = 1090 / 62 + 3;
        for (int i = 0; i < maxCycles && match.Snapshot().phase == HitmMatchPhase::kFight; ++i) {
            ThrowOneBrooklynSpecial(match);
        }
        DOMINUS_EXPECT(match.Snapshot().phase == HitmMatchPhase::kKO);
        for (int i = 0; i < 150; ++i) match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    }

    auto snap = match.Snapshot();
    DOMINUS_EXPECT(snap.phase == HitmMatchPhase::kMatchOver);
    DOMINUS_EXPECT(snap.match_over);
    DOMINUS_EXPECT(snap.match_winner_index == 0);  // fighter A (Brooklyn)
    DOMINUS_EXPECT(snap.fighter_a_rounds_won == roundsToWin);

    // Real: update() has no case for 'matchOver' -- nothing advances
    // once the match is over.
    auto beforeExtra = match.Snapshot();
    match.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kRight);
    DOMINUS_EXPECT(match.Snapshot() == beforeExtra);
}

// --- 7. Rocket's real "no working special" stays honest at match level ---

DOMINUS_TEST(HitmMatch_Fight_RocketGhostDashConnectsAndDealsRealDamage) {
    // PHASE 4 (HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md): Rocket's
    // real "Ghost Dash" now actually connects and deals real damage --
    // a real, structurally different move type from Brooklyn's melee
    // special, resolved via the real _applyRush port
    // (CHARACTER/HitmBridge/HitmRushAttack.h), not MeleeHitConnects.
    auto match = MakeBrooklynVsRocketMatch();
    SkipRoundIntro(match);

    // Real: close enough of the real 460px starting gap that Rocket's
    // real Ghost Dash (velocityX=16, friction=0.93 -- a real geometric-
    // decay closure of roughly 212px more over its own real 36-frame
    // duration) can reach Brooklyn. 50 real frames of Rocket's own real
    // walkSpeed (4.4/frame) leaves a real ~240px gap, comfortably inside
    // reach (real hitRangeX=80, boundary-proven exactly in
    // test_hitm_rush_attack.cpp).
    for (int i = 0; i < 50; ++i) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kLeft);
    }
    auto beforeDash = match.Snapshot();
    DOMINUS_EXPECT(beforeDash.fighter_b.state == HitmFighterState::kWalking);
    DOMINUS_EXPECT(beforeDash.fighter_a.hp == beforeDash.fighter_a.max_hp);  // untouched so far

    match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kSpecial);
    DOMINUS_EXPECT(match.Snapshot().fighter_b.state == HitmFighterState::kAttackStartup);

    int hpBefore = match.Snapshot().fighter_a.hp;
    bool connected = false;
    // Real: the whole move is startup(5)+active(18)+recovery(13)=36
    // frames -- run through it (plus a small safety margin) and confirm
    // the real dash hit lands somewhere in there.
    for (int i = 0; i < 40 && !connected; ++i) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
        if (match.Snapshot().fighter_a.hp < hpBefore) connected = true;
    }
    DOMINUS_EXPECT(connected);
    DOMINUS_EXPECT(match.Snapshot().fighter_a.hp == hpBefore - 96);  // real Ghost Dash damage
    DOMINUS_EXPECT(match.Snapshot().fighter_a.state == HitmFighterState::kHitstun);

    // Real: resolves exactly once -- Brooklyn's hp must not drop again
    // for the rest of the real move.
    int hpAfterHit = match.Snapshot().fighter_a.hp;
    for (int i = 0; i < 30 && match.Snapshot().fighter_b.state != HitmFighterState::kIdle; ++i) {
        match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
        DOMINUS_EXPECT(match.Snapshot().fighter_a.hp == hpAfterHit);
    }
}

DOMINUS_TEST(HitmMatch_Fight_RocketGhostDash_TooFarAwayDoesNotConnect) {
    // Negative control: Rocket's real dash has a real, finite reach
    // (velocityX=16 decaying by real friction=0.93 every frame) -- from
    // the real, unmodified starting gap (460px) it cannot reach Brooklyn
    // at all. A real, evidenced boundary, not an assumption.
    auto match = MakeBrooklynVsRocketMatch();
    SkipRoundIntro(match);

    match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kSpecial);
    DOMINUS_EXPECT(match.Snapshot().fighter_b.state == HitmFighterState::kAttackStartup);
    int hpBefore = match.Snapshot().fighter_a.hp;
    for (int i = 1; i < 40; ++i) match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);

    DOMINUS_EXPECT(match.Snapshot().fighter_a.hp == hpBefore);  // never connected
    DOMINUS_EXPECT(match.Snapshot().fighter_b.state == HitmFighterState::kIdle);  // real move ran its course
}

// --- 8. Determinism ----------------------------------------------------------

DOMINUS_TEST(HitmMatch_IdenticalInputSequencesProduceIdenticalStates) {
    auto matchA = MakeBrooklynVsRocketMatch();
    auto matchB = MakeBrooklynVsRocketMatch();

    std::vector<std::pair<HitmInputCommand, HitmInputCommand>> script;
    for (int i = 0; i < 120; ++i) script.push_back({HitmInputCommand::kNeutral, HitmInputCommand::kNeutral});
    for (int i = 0; i < 75; ++i) script.push_back({HitmInputCommand::kRight, HitmInputCommand::kBlock});
    script.push_back({HitmInputCommand::kSpecial, HitmInputCommand::kBlock});
    for (int i = 1; i < 14; ++i) script.push_back({HitmInputCommand::kNeutral, HitmInputCommand::kBlock});
    for (int i = 0; i < 10; ++i) script.push_back({HitmInputCommand::kNeutral, HitmInputCommand::kNeutral});

    for (auto& [a, b] : script) {
        matchA.AdvanceFrame(a, b);
        matchB.AdvanceFrame(a, b);
        DOMINUS_EXPECT(matchA.Snapshot() == matchB.Snapshot());
    }
    // A real, non-trivial state was actually reached -- this determinism
    // proof isn't vacuous over two matches that never left round intro.
    DOMINUS_EXPECT(matchA.Snapshot().phase == HitmMatchPhase::kFight);
    DOMINUS_EXPECT(matchA.Snapshot().fighter_b.state == HitmFighterState::kBlockstun);
}

DOMINUS_TEST(HitmMatch_DivergentInputSequencesProduceDivergentStates) {
    auto matchA = MakeBrooklynVsRocketMatch();
    auto matchB = MakeBrooklynVsRocketMatch();
    for (int i = 0; i < 120; ++i) {
        matchA.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
        matchB.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    }
    matchA.AdvanceFrame(HitmInputCommand::kRight, HitmInputCommand::kNeutral);
    matchB.AdvanceFrame(HitmInputCommand::kLeft, HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(!(matchA.Snapshot() == matchB.Snapshot()));
}
