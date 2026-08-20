// tests/integration/test_hitm_rush_attack.cpp
// ROADMAP.md Track H, Phase 4 (HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md)
// -- direct coverage of RushHitConnects(), a pure port of hitm-engine's
// own real `CombatSystem.js:291-292` (the hit-box check inside
// `_applyRush`). Boundary-exact arithmetic throughout, matching this
// track's established float-precision discipline, and this file's own
// sibling `test_hitm_melee_hit_check.cpp`'s exact testing style --
// deliberately parallel, since the two functions are the real engine's
// two genuinely different real hit-detection formulas.
#include "CHARACTER/HitmBridge/HitmRushAttack.h"

#include "tests/TestFramework.h"

using dominus::character::hitm::HitmFighterSnapshot;
using dominus::character::hitm::HitmMoveInstance;
using dominus::character::hitm::HitmRushData;
using dominus::character::hitm::RushHitConnects;

namespace {

// Real Rocket "special" values (Ghost Dash): hitRangeX=80, hitRangeY=110.
HitmMoveInstance GhostDashShaped() {
    HitmMoveInstance move;
    HitmRushData rush;
    rush.velocity_x = 16.0;
    rush.friction = 0.93;
    rush.hit_range_x = 80.0;
    rush.hit_range_y = 110.0;
    move.rush = rush;
    return move;
}

HitmFighterSnapshot SnapshotAt(float x, float y) {
    HitmFighterSnapshot snap;
    snap.x = x;
    snap.y = y;
    return snap;
}

}  // namespace

DOMINUS_TEST(HitmRushAttack_NonRushMove_NeverConnects) {
    HitmFighterSnapshot attacker = SnapshotAt(500.0f, 538.0f);
    HitmFighterSnapshot defender = SnapshotAt(500.0f, 538.0f);  // same position, would connect if rush were set
    HitmMoveInstance meleeMove;  // no rush data at all
    DOMINUS_EXPECT(!RushHitConnects(attacker, meleeMove, defender));
}

// --- Horizontal reach, facing-independent -----------------------------------

DOMINUS_TEST(HitmRushAttack_ConnectsJustInsideRealHorizontalReach) {
    auto move = GhostDashShaped();
    auto attacker = SnapshotAt(500.0f, 538.0f);
    // |500-x| < 80 -- foe.x=579 -> diff=79<80.
    auto defenderJustIn = SnapshotAt(579.0f, 538.0f);
    DOMINUS_EXPECT(RushHitConnects(attacker, move, defenderJustIn));
    // Symmetric on the other side too -- real formula has no facing term
    // at all, unlike _melee.
    auto defenderJustInOtherSide = SnapshotAt(421.0f, 538.0f);
    DOMINUS_EXPECT(RushHitConnects(attacker, move, defenderJustInOtherSide));
}

DOMINUS_TEST(HitmRushAttack_DoesNotConnectAtOrBeyondRealHorizontalBoundary) {
    auto move = GhostDashShaped();
    auto attacker = SnapshotAt(500.0f, 538.0f);
    auto defenderAtBoundary = SnapshotAt(580.0f, 538.0f);  // diff=80, not<80
    DOMINUS_EXPECT(!RushHitConnects(attacker, move, defenderAtBoundary));
    auto defenderFar = SnapshotAt(1200.0f, 538.0f);
    DOMINUS_EXPECT(!RushHitConnects(attacker, move, defenderFar));
}

// --- Vertical gate -----------------------------------------------------------

DOMINUS_TEST(HitmRushAttack_VerticalGate_RealHeightBoundary) {
    auto move = GhostDashShaped();
    auto attacker = SnapshotAt(500.0f, 538.0f);  // hitRangeY=110
    auto defenderJustIn = SnapshotAt(550.0f, 538.0f - 109.0f);  // diff=109<110
    DOMINUS_EXPECT(RushHitConnects(attacker, move, defenderJustIn));
    auto defenderAtBoundary = SnapshotAt(550.0f, 538.0f - 110.0f);  // diff=110, not<110
    DOMINUS_EXPECT(!RushHitConnects(attacker, move, defenderAtBoundary));
}

// --- Real JS falsy-zero fallback quirk (matching this track's own,
// already-established precedent for the identical `||` pattern in both
// _melee's height fallback and the secondary-motion spring params) -----------

DOMINUS_TEST(HitmRushAttack_RealZeroHitRangeX_FallsBackToRealDefault) {
    auto move = GhostDashShaped();
    move.rush->hit_range_x = 0.0;  // real authored 0 -- JS `hitRangeX||80` still substitutes 80
    auto attacker = SnapshotAt(500.0f, 538.0f);
    auto defenderJustIn = SnapshotAt(579.0f, 538.0f);  // diff=79 -- fails against real 0, succeeds against fallback 80
    DOMINUS_EXPECT(RushHitConnects(attacker, move, defenderJustIn));
}

DOMINUS_TEST(HitmRushAttack_RealZeroHitRangeY_FallsBackToRealDefault) {
    auto move = GhostDashShaped();
    move.rush->hit_range_y = 0.0;
    auto attacker = SnapshotAt(500.0f, 538.0f);
    auto defenderJustIn = SnapshotAt(550.0f, 538.0f - 109.0f);  // diff=109 -- fails against 0, succeeds against fallback 110
    DOMINUS_EXPECT(RushHitConnects(attacker, move, defenderJustIn));
}
