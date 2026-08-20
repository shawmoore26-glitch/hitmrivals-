// CHARACTER/HitmBridge/HitmMeleeHitCheck.cpp
#include "CHARACTER/HitmBridge/HitmMeleeHitCheck.h"

#include <cmath>

namespace dominus::character::hitm {

bool MeleeHitConnects(const HitmFighterSnapshot& attacker, const HitmMoveInstance& move,
                      const HitmFighterSnapshot& defender) {
    // hx = f.x + f.facing*(f.w/2 + def.range/2)
    double hx = static_cast<double>(attacker.x) +
                static_cast<double>(attacker.facing) * (kMeleeFighterWidth / 2.0 + move.range / 2.0);

    // def.height || 105 -- real JS falsy-zero fallback, replicated exactly
    // (see this file's header comment).
    double height = move.height != 0.0 ? move.height : kMeleeHeightFallback;

    bool xHit = std::abs(hx - static_cast<double>(defender.x)) < (move.range / 2.0 + kMeleeFighterWidth / 2.0);
    bool yHit =
        std::abs((static_cast<double>(attacker.y) - 80.0) - (static_cast<double>(defender.y) - 80.0)) < height;

    return xHit && yHit;
}

}  // namespace dominus::character::hitm
