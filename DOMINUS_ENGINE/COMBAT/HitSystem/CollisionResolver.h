// COMBAT/HitSystem/CollisionResolver.h
// Closes the real gap: CollisionEvaluator is a deliberately pure query
// (see its own header comment) -- it returns every overlapping
// hitbox/hurtbox pair and explicitly leaves "how many hits actually
// apply" to a caller, naming the exact rule needed: "one hit per
// active-frame window, not one per overlapping box pair." No caller
// in this engine implemented that rule until now.
//
// CollisionResolver IS that caller-side decision, and nothing else --
// it doesn't touch physics, doesn't build an ImpactContext, doesn't
// call ImpactSolver. Its entire job is: given everything
// CollisionEvaluator::Evaluate returned this call, and everything it
// has already resolved since the last reset, decide whether exactly
// one new impact should occur right now.
//
// Determinism: CollisionEvaluator::Evaluate iterates move.hitboxes then
// defenderHurtboxes.boxes in the fixed order the caller's own data
// provides them in -- "first" below is reproducible from the same
// inputs, not a race or an arbitrary pick.
#pragma once

#include <optional>
#include <vector>

#include "COMBAT/HitSystem/CollisionEvaluator.h"

namespace dominus::combat {

class CollisionResolver {
public:
    // Call when a move enters a fresh opportunity to land a hit --
    // CombatController::StartMove is the real trigger (a new move
    // instance is a new activation; LAW C005's frame data still gates
    // WHEN hitboxes are meaningful via CombatPhase::kActive, this only
    // gates HOW MANY hits one activation can produce).
    void ResetForNewActivation() { hasResolvedThisActivation_ = false; }

    // Returns the single HitResult to actually apply, or std::nullopt
    // if there was nothing to hit, or this activation has already
    // resolved one. Never mutates `hits`, never calls anything outside
    // this class -- pure decision, same discipline CollisionEvaluator
    // itself already follows.
    std::optional<HitResult> Resolve(const std::vector<HitResult>& hits) {
        if (hasResolvedThisActivation_) return std::nullopt;
        if (hits.empty()) return std::nullopt;
        hasResolvedThisActivation_ = true;
        return hits.front();
    }

    bool HasResolvedThisActivation() const { return hasResolvedThisActivation_; }

private:
    bool hasResolvedThisActivation_ = false;
};

}  // namespace dominus::combat
