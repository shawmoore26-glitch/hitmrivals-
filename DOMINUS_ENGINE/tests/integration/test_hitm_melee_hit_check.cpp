// tests/integration/test_hitm_melee_hit_check.cpp
// ROADMAP.md Track H, Phase 2 step 5
// (HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md) -- direct coverage of
// MeleeHitConnects(), a pure port of hitm-engine's own real
// `CombatSystem.js:370-374` (`_melee`). Boundary-exact arithmetic
// throughout (not rounded literals), matching this session's established
// float-precision discipline -- every "just inside"/"just outside"
// assertion below is derived from the identical formula the production
// code itself uses, not an approximation.
#include "CHARACTER/HitmBridge/HitmMeleeHitCheck.h"

#include "tests/TestFramework.h"

using dominus::character::hitm::HitmFighterSnapshot;
using dominus::character::hitm::HitmMoveInstance;
using dominus::character::hitm::kMeleeFighterWidth;
using dominus::character::hitm::kMeleeHeightFallback;
using dominus::character::hitm::MeleeHitConnects;

namespace {

// Real Brooklyn "special" values (DRUNKEN LAUNCHER KICK):
// range=88, height=132 -- see HitmMoveInstance_Extract tests /
// HITM_FIGHTER_RUNTIME_REPORT.md for the real signature.json source.
HitmMoveInstance BrooklynSpecialShaped() {
    HitmMoveInstance move;
    move.range = 88.0;
    move.height = 132.0;
    return move;
}

HitmFighterSnapshot SnapshotAt(float x, float y, int facing) {
    HitmFighterSnapshot snap;
    snap.x = x;
    snap.y = y;
    snap.facing = facing;
    return snap;
}

}  // namespace

// --- Real constants -----------------------------------------------------

DOMINUS_TEST(HitmMeleeHitCheck_RealConstants_MatchFighterJs) {
    // Fighter.js:28, "this.w = 52". CombatSystem.js:373, "def.height || 105".
    DOMINUS_EXPECT(kMeleeFighterWidth == 52.0);
    DOMINUS_EXPECT(kMeleeHeightFallback == 105.0);
}

// --- Horizontal reach, facing right --------------------------------------

DOMINUS_TEST(HitmMeleeHitCheck_FacingRight_ConnectsJustInsideRealReach) {
    // hx = 500 + 1*(52/2 + 88/2) = 500 + (26+44) = 570.
    // xHit: |hx - foe.x| < (88/2 + 52/2) = 70.
    auto attacker = SnapshotAt(500.0f, 538.0f, /*facing=*/1);
    auto move = BrooklynSpecialShaped();
    // foe.x = 501 -> |570-501| = 69 < 70 -- connects.
    auto defenderJustInLow = SnapshotAt(501.0f, 538.0f, 1);
    DOMINUS_EXPECT(MeleeHitConnects(attacker, move, defenderJustInLow));
    // foe.x = 639 -> |570-639| = 69 < 70 -- connects.
    auto defenderJustInHigh = SnapshotAt(639.0f, 538.0f, 1);
    DOMINUS_EXPECT(MeleeHitConnects(attacker, move, defenderJustInHigh));
}

DOMINUS_TEST(HitmMeleeHitCheck_FacingRight_DoesNotConnectAtOrBeyondRealBoundary) {
    auto attacker = SnapshotAt(500.0f, 538.0f, /*facing=*/1);
    auto move = BrooklynSpecialShaped();
    // foe.x = 500 -> |570-500| = 70, NOT < 70 -- real boundary, exclusive.
    auto defenderAtLowBoundary = SnapshotAt(500.0f, 538.0f, 1);
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defenderAtLowBoundary));
    // foe.x = 640 -> |570-640| = 70 -- same real boundary, other side.
    auto defenderAtHighBoundary = SnapshotAt(640.0f, 538.0f, 1);
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defenderAtHighBoundary));
    // Far away -- unambiguously out of real reach.
    auto defenderFar = SnapshotAt(2000.0f, 538.0f, 1);
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defenderFar));
}

// --- Facing determines which side is "in front" --------------------------

DOMINUS_TEST(HitmMeleeHitCheck_FacingLeft_ReachesTheOppositeSide) {
    // hx = 500 + (-1)*70 = 430. Real reach window: (360, 500) exclusive.
    auto attacker = SnapshotAt(500.0f, 538.0f, /*facing=*/-1);
    auto move = BrooklynSpecialShaped();
    auto defenderInFront = SnapshotAt(431.0f, 538.0f, 1);  // |430-431|=1<70
    DOMINUS_EXPECT(MeleeHitConnects(attacker, move, defenderInFront));
    // The SAME real defender position that connected while facing right
    // (foe.x=501) must NOT connect while facing left -- facing genuinely
    // changes which side is reachable, not a symmetric "range around me."
    auto defenderBehind = SnapshotAt(501.0f, 538.0f, 1);
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defenderBehind));
}

// --- Vertical gate ---------------------------------------------------------

DOMINUS_TEST(HitmMeleeHitCheck_VerticalGate_RealHeightBoundary) {
    // In real x-reach (foe.x=550, well inside the 500..640 window).
    auto attacker = SnapshotAt(500.0f, 538.0f, /*facing=*/1);
    auto move = BrooklynSpecialShaped();  // height=132
    // |( (538-80) - (y-80) )| < 132  ->  |538-y| < 132.
    auto defenderJustInReach = SnapshotAt(550.0f, 538.0f - 131.0f, 1);  // diff=131<132
    DOMINUS_EXPECT(MeleeHitConnects(attacker, move, defenderJustInReach));
    auto defenderAtBoundary = SnapshotAt(550.0f, 538.0f - 132.0f, 1);  // diff=132, not<132
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defenderAtBoundary));
}

// --- Real JS falsy-zero quirk on `height` (matching this track's own,
// already-established secondary-motion precedent for the identical `||`
// fallback pattern) ---------------------------------------------------------

DOMINUS_TEST(HitmMeleeHitCheck_RealZeroHeight_FallsBackToRealDefault) {
    HitmMoveInstance move;
    move.range = 88.0;
    move.height = 0.0;  // real authored 0 (JS `def.height || 105` still substitutes 105)
    auto attacker = SnapshotAt(500.0f, 538.0f, /*facing=*/1);
    // In x-reach; y-diff of 104 would fail against a real height of 0
    // (nothing could ever connect), but must succeed against the real
    // fallback of 105.
    auto defender = SnapshotAt(550.0f, 538.0f - 104.0f, 1);
    DOMINUS_EXPECT(MeleeHitConnects(attacker, move, defender));
    auto defenderJustOutside = SnapshotAt(550.0f, 538.0f - 105.0f, 1);  // diff=105, not<105
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defenderJustOutside));
}

// --- A real, non-boundary scenario using Brooklyn's own real starting
// position convention (HitmFighterRuntime::Create's real
// (wallL+wallR)/2), proving the function works end to end with realistic
// numbers, not just crafted boundaries. ------------------------------------

DOMINUS_TEST(HitmMeleeHitCheck_RealScenario_AdjacentFightersConnect) {
    // Real wallL=64, wallR=996 (game.json) -- two fighters standing close
    // together near the center, well within Brooklyn's real 88-range special.
    auto attacker = SnapshotAt(500.0f, 538.0f, /*facing=*/1);
    auto defender = SnapshotAt(540.0f, 538.0f, /*facing=*/-1);
    auto move = BrooklynSpecialShaped();
    DOMINUS_EXPECT(MeleeHitConnects(attacker, move, defender));
}

DOMINUS_TEST(HitmMeleeHitCheck_RealScenario_FightersAcrossTheStageDoNotConnect) {
    // Real wallL=64, wallR=996 -- opposite ends of the real stage, far
    // beyond any real move's range/height.
    auto attacker = SnapshotAt(64.0f, 538.0f, /*facing=*/1);
    auto defender = SnapshotAt(996.0f, 538.0f, /*facing=*/-1);
    auto move = BrooklynSpecialShaped();
    DOMINUS_EXPECT(!MeleeHitConnects(attacker, move, defender));
}
