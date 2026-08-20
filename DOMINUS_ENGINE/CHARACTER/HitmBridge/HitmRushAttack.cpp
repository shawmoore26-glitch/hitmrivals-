// CHARACTER/HitmBridge/HitmRushAttack.cpp
#include "CHARACTER/HitmBridge/HitmRushAttack.h"

#include <cmath>

namespace dominus::character::hitm {

bool RushHitConnects(const HitmFighterSnapshot& attacker, const HitmMoveInstance& move,
                      const HitmFighterSnapshot& defender) {
    if (!move.rush) return false;

    // Real JS `||` fallback quirk, faithfully replicated (see this
    // file's header comment) -- Rocket's own real Ghost Dash data
    // authors both nonzero, so this is currently unreachable in
    // practice, kept faithful for whichever future rush move omits one.
    double hitRangeX = move.rush->hit_range_x != 0.0 ? move.rush->hit_range_x : 80.0;
    double hitRangeY = move.rush->hit_range_y != 0.0 ? move.rush->hit_range_y : 110.0;

    double dx = std::abs(static_cast<double>(attacker.x) - static_cast<double>(defender.x));
    double dy = std::abs(static_cast<double>(attacker.y) - static_cast<double>(defender.y));
    return dx < hitRangeX && dy < hitRangeY;
}

void TickRushAttack(HitmFighterRuntime& attacker, HitmFighterRuntime& defender, const HitmMoveInstance& move,
                     HitmRushAttackState& state) {
    if (!move.rush) return;

    auto snap = attacker.Snapshot();
    bool inAttack = snap.state == HitmFighterState::kAttackStartup || snap.state == HitmFighterState::kAttackActive ||
                     snap.state == HitmFighterState::kAttackRecovery;
    if (!inAttack) {
        state = HitmRushAttackState{};
        return;
    }

    // Real: `if(f.move.t===1){ f.vx=(r.velocityX||0)*f.facing; ...}` --
    // this class's own convention for "the real first tick of the
    // move" is state==kAttackStartup && state_frame==0 (the same
    // transition-frame convention `state_frame` itself already uses
    // throughout this track).
    bool isFirstTick = snap.state == HitmFighterState::kAttackStartup && snap.state_frame == 0;
    if (isFirstTick) {
        // Real: `(r.velocityX||0)*f.facing` -- the real `||0` fallback
        // here is a no-op (0 substituted for 0 is still 0), unlike the
        // friction/hitRange fallbacks below, whose real defaults are
        // genuinely nonzero -- so there is nothing extra to replicate
        // for this one field.
        state.velocity_x = move.rush->velocity_x * static_cast<double>(snap.facing);
        state.hit_done = false;
    }

    // Real: `f.x+=f.vx; f.vx*=(r.friction||0.93);` -- every real tick of
    // the whole move, not just one frame.
    float newX = snap.x + static_cast<float>(state.velocity_x);
    attacker.SetPosition(newX, snap.y);
    double friction = move.rush->friction != 0.0 ? move.rush->friction : 0.93;  // real `||0.93` fallback
    state.velocity_x *= friction;

    if (state.hit_done) return;
    // Real: `f.move.t>=(def.startup+(f.startupRoll||0))` -- this class
    // has no real startupRoll concept (not authored on any real rush
    // move), so this is exactly "startup has elapsed", i.e. no longer
    // kAttackStartup.
    if (snap.state == HitmFighterState::kAttackStartup) return;

    auto attackerSnap = attacker.Snapshot();  // refreshed: position just changed above
    auto defenderSnap = defender.Snapshot();
    if (RushHitConnects(attackerSnap, move, defenderSnap)) {
        state.hit_done = true;
        // Real, documented gap: always unblocked -- see this file's own
        // header comment.
        defender.TakeHit(move, /*blocking=*/false);
        attacker.ResolveOutgoingHitLanded(move);
    }
}

}  // namespace dominus::character::hitm
