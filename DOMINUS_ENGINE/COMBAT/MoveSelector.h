// COMBAT/MoveSelector.h
// LAW C003's "Move Selection" stage: given the moves actually available
// (MoveSetComponent) and a fighter's DecisionWeights, score each candidate
// and pick the highest. Deterministic (no RNG) so this stays unit-testable
// -- "unpredictability" is expressed as which moves the weights favor, not
// as literal randomness, matching LAW C014's determinism requirement.
#pragma once

#include <limits>
#include <string>
#include <vector>

#include "CHARACTER/Genome/DecisionWeights.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/HitSystem/MoveDef.h"

namespace dominus::combat {

class MoveSelector {
public:
    // `candidates` restricts selection to a subset of move names (e.g. only
    // moves legal to cancel into right now) -- pass every move's name from
    // MoveSetComponent for an unrestricted choice.
    static std::string SelectBest(const MoveSetComponent& moves, const std::vector<std::string>& candidates,
                                   const character::DecisionWeights& weights) {
        std::string best;
        float bestScore = -std::numeric_limits<float>::infinity();

        for (const auto& name : candidates) {
            const MoveDef* move = moves.Find(name);
            if (!move) continue;
            float score = Score(*move, weights);
            if (score > bestScore) {
                bestScore = score;
                best = name;
            }
        }
        return best;
    }

    static float Score(const MoveDef& move, const character::DecisionWeights& weights) {
        // Aggressive fighters weight raw power heavily; passive fighters
        // barely care about it.
        float powerScore = move.power * weights.aggression;

        // Risk cost is paid down by risk_tolerance -- a high-risk-tolerance
        // fighter treats a risky move's cost as near zero, a cautious one
        // pays it almost in full.
        float riskCost = RiskCost(move.intent.risk) * (1.0f - weights.risk_tolerance);

        // Unpredictability rewards moves with MORE followups (keeps options
        // open / varies the sequence) over dead-end moves.
        float varietyBonus = static_cast<float>(move.intent.followups.size()) * weights.unpredictability * 3.0f;

        return powerScore - riskCost + varietyBonus;
    }

private:
    static float RiskCost(const std::string& risk) {
        if (risk == "high") return 15.0f;
        if (risk == "medium") return 7.0f;
        if (risk == "low") return 0.0f;
        return 5.0f;
    }
};

}  // namespace dominus::combat
