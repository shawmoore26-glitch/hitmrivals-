// CHARACTER/HitmBridge/HitmMeleeHitCheck.h
// ROADMAP.md Track H -- Phase 2 (real combat), step 5 of
// HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md's proposed sequence.
//
// A direct, line-by-line port of hitm-engine's own real
// `CombatSystem.js:370-374` (`_melee`):
//   _melee(f, foe, def) {
//     const hx = f.x + f.facing*(f.w/2 + def.range/2);
//     if (Math.abs(hx - foe.x) < (def.range/2 + foe.w/2) &&
//         Math.abs((f.y-80)-(foe.y-80)) < (def.height || 105))
//       this.applyHit(foe, f, def);
//   }
//
// Pure position/range/height check -- NOT COMBAT::CollisionEvaluator, and
// deliberately so: the audit found the real engine itself never uses
// bone-relative hitboxes for 2D fighters either (see
// HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md's "what's missing and
// why"). No Skeleton, no Pose, no bind pose -- nothing here touches the
// permanently-blocked FK gap.
//
// Real, hardcoded constants (`f.w`/`f.hgt` in `Fighter.js:28`) are
// uniform across every fighter, not per-character authored data --
// ported here directly, same category as COMBAT::kFramesPerSecond or
// this module's own kBaseHp (HitmFighterRuntime.cpp).
//
// This function is a pure query -- it does not call TakeHit, does not
// know about hitstop/hitstun/damage, and does not require its two
// fighters to belong to the same match/world. A future match driver is
// the real caller: check MeleeHitConnects() while the attacker is in
// kAttackActive, and if it returns true, call the defender's own
// TakeHit() with the attacker's move.
#pragma once

#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"

namespace dominus::character::hitm {

// Real, hardcoded, uniform-across-every-fighter engine constant
// (`Fighter.js:28`, "this.w = 52") -- NOT authored per-fighter data.
constexpr double kMeleeFighterWidth = 52.0;

// Real per-move fallback when `height` isn't authored
// (`CombatSystem.js:373`, "def.height || 105"). HitmMoveInstance::Extract
// always requires `height` today (Brooklyn's and Static's real specials
// both author it -- Rocket's real "rush"-type special is a structurally
// different move this function does not model, see the audit's Step 2/4
// findings), so this fallback is currently unreachable in practice but
// kept faithful to the real formula for whichever future move type omits
// it, replicating the real JS `||` falsy-zero quirk exactly (a real
// authored 0 would also hit this fallback in the real engine -- the same
// quirk this track's secondary-motion work already found and
// deliberately preserved for stiffness/damping/maxAngle).
constexpr double kMeleeHeightFallback = 105.0;

// True exactly when `attacker`'s attack, using `move`'s real
// range/height and `attacker`'s current position/facing, connects with
// `defender`'s current position. Does not check `attacker.state` --
// real gating on "is this fighter actually in an active attack frame
// right now" is the caller's job (the caller already has
// `attacker.state`/`attacker.state_frame` on the same snapshot this
// takes), matching this track's standing "explicit seam, not guessed"
// discipline for anything that needs two-fighter/match awareness this
// single query does not have.
bool MeleeHitConnects(const HitmFighterSnapshot& attacker, const HitmMoveInstance& move,
                      const HitmFighterSnapshot& defender);

}  // namespace dominus::character::hitm
