// CHARACTER/HitmBridge/HitmRushAttack.h
// ROADMAP.md Track H, Phase 4 of HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md
// (authorized as its own checkpoint after Phase 3's) -- Rocket's real
// "Ghost Dash" special.
//
// A direct, line-by-line port of hitm-engine's own real
// `CombatSystem.js:283-296` (`_applyRush`):
//   _applyRush(f,foe,def){
//     const r=def.rush;
//     if(f.move.t===1){ f.vx=(r.velocityX||0)*f.facing; if(r.velocityY) f.vy=r.velocityY; }
//     f.x+=f.vx; f.vx*=(r.friction||0.93);
//     if(r.leaveGhost && f.animT%2===0) f.ghosts.push({x:f.x,y:f.y,life:12});
//     if(def.vfx) this.e.bus.emit('vfx:play',{name:def.vfx, fighter:f, anchor:'hand'});
//     if(!f.hitDone && f.move.t>=(def.startup+(f.startupRoll||0)) &&
//        Math.abs(f.x-foe.x)<(r.hitRangeX||80) && Math.abs(f.y-foe.y)<(r.hitRangeY||110)){
//       f.hitDone=true;
//       this.applyHit(foe,f,{damage:def.damage,pushback:def.pushback,hitstun:def.hitstun,
//         knockdown:def.knockdown, hitstop:'heavy'});
//     }
//   }
//
// Fundamentally NOT `HitmMeleeHitCheck.h`'s formula: real rush reach is
// a facing-INDEPENDENT, absolute-position box (`|f.x-foe.x|`, no
// attacker-width/facing offset at all -- the attacker is bodily dashing
// into the defender, so proximity alone decides a hit, not "am I facing
// the right way"), and it is checked EVERY real frame from real startup
// onward (through active AND recovery) until it connects, not just once
// on the real first active frame the way `_melee` resolves.
//
// This needs real, per-attack, cross-frame state (the decaying dash
// velocity, and whether this activation has already landed) -- unlike
// `MeleeHitConnects`, a rush attack is NOT a pure function of one
// snapshot, the same "explicit, caller-owned state across frames"
// distinction `HitmSecondaryMotionState` already draws for spring
// motion.
//
// Real, documented gap, not silently wrong: blocking is not resolved
// here. See `HitmFighterRuntime.h`'s own "PHASE 4" header comment for
// the full real finding this rests on (the real engine's own
// `applyHit()` doesn't lock the defender's `state` on a blocked hit at
// all, and no real `blockstun` value exists anywhere in Rocket's own
// rush-type move data to drive this class's already-closed `TakeHit()`
// design regardless). `TickRushAttack()` below always resolves a landed
// hit as unblocked.
#pragma once

#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"

namespace dominus::character::hitm {

// Per-fighter, caller-owned state for one real rush-type attack
// activation.
struct HitmRushAttackState {
    // The real, currently-decaying dash velocity (`f.vx` in the real
    // engine) -- 0 whenever no rush attack is in flight.
    double velocity_x = 0.0;
    // Real `f.hitDone`, scoped to this one activation.
    bool hit_done = false;
};

// True exactly when `attacker` and `defender`'s current positions
// satisfy the real rush hit box for `move` -- a direct port of
// `_applyRush`'s own real `Math.abs(f.x-foe.x)<hitRangeX &&
// Math.abs(f.y-foe.y)<hitRangeY`, including its real JS `||` fallback
// quirk on `hitRangeX`/`hitRangeY` (already faithfully replicated
// elsewhere in this track for `_melee`'s own `height||105`). Returns
// false (never a crash) if `move` has no real rush data.
bool RushHitConnects(const HitmFighterSnapshot& attacker, const HitmMoveInstance& move,
                      const HitmFighterSnapshot& defender);

// Called once per real frame, for one attacker/defender pair, whenever
// `attacker` is in ANY attack sub-state and its current move
// (`attacker.SpecialMove()`) has real rush data -- the real engine's
// own `_applyRush` runs for the move's ENTIRE real duration
// (startup+active+recovery), not just one sub-state, matching this
// class's own single dispatch point for the whole thing.
//
// Real behavior:
//   - On the real first tick of the move (this class's own
//     `kAttackStartup`+`state_frame==0` convention for "the move just
//     started"), sets `state.velocity_x` to the real dash kick
//     (`rush.velocity_x * attacker`'s real current facing) and clears
//     `state.hit_done`.
//   - Every tick: applies the current `state.velocity_x` to
//     `attacker`'s real position (via `SetPosition()` -- see that
//     method's own header comment for why an explicit external seam,
//     not something this class computes internally), then decays
//     `state.velocity_x` by the real `rush.friction`.
//   - Once real startup has elapsed (this class's own `kAttackActive`/
//     `kAttackRecovery`), checks the real rush hit box every tick until
//     it connects (`RushHitConnects()`, above) or `state.hit_done` is
//     already true -- on a real connect, resolves it via
//     `defender.TakeHit(move, /*blocking=*/false)` (see this file's own
//     top comment for why always unblocked) and
//     `attacker.ResolveOutgoingHitLanded(move)`.
//   - Resets `state` to inert (`{0.0, false}`) whenever `attacker` is
//     not in any attack sub-state -- so a stale velocity/hit_done from a
//     PRIOR rush activation never leaks into a later, unrelated one.
//
// `move` must have real rush data (`move.rush.has_value()`) -- callers
// (a match driver) are expected to check `attacker.SpecialMove()` and
// its own `->rush` before calling this, the same "explicit seam, caller
// already has the data" discipline the rest of this track already uses.
void TickRushAttack(HitmFighterRuntime& attacker, HitmFighterRuntime& defender, const HitmMoveInstance& move,
                     HitmRushAttackState& state);

}  // namespace dominus::character::hitm
