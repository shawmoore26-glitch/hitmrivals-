// CHARACTER/Genome/GenomeDecoder.h
// Deterministic decode of CombatIdentity's genome strings into
// DecisionWeights. Unknown/unrecognized strings fall back to the neutral
// 0.5 default (see DecisionWeights) rather than throwing -- new genome
// content should degrade to "average" behavior, not crash the engine
// (LAW C014: no fake systems, but also no fragile ones).
#pragma once

#include <string>

#include "CHARACTER/Genome/CombatIdentity.h"
#include "CHARACTER/Genome/DecisionWeights.h"

namespace dominus::character {

class GenomeDecoder {
public:
    static DecisionWeights Decode(const CombatIdentity& identity) {
        DecisionWeights w;
        w.aggression = PressureToAggression(identity.pressure);
        w.risk_tolerance = RiskToTolerance(identity.risk);
        w.unpredictability = MobilityToUnpredictability(identity.mobility);
        w.counter_bias = CounterToBias(identity.counter);
        w.defense_bias = RangeToDefenseBias(identity.range);
        return w;
    }

private:
    static float PressureToAggression(const std::string& pressure) {
        if (pressure == "relentless") return 0.9f;
        if (pressure == "cautious") return 0.2f;
        if (pressure == "reactive") return 0.4f;
        if (pressure == "reckless") return 1.0f;
        return 0.5f;
    }
    static float RiskToTolerance(const std::string& risk) {
        if (risk == "low") return 0.2f;
        if (risk == "medium") return 0.5f;
        if (risk == "high") return 0.85f;
        return 0.5f;
    }
    static float MobilityToUnpredictability(const std::string& mobility) {
        if (mobility == "unpredictable") return 0.9f;
        if (mobility == "grounded") return 0.25f;
        if (mobility == "aerial") return 0.6f;
        return 0.5f;
    }
    static float CounterToBias(const std::string& counter) {
        if (counter == "expert") return 0.9f;
        if (counter == "average") return 0.5f;
        if (counter == "poor") return 0.15f;
        return 0.5f;
    }
    static float RangeToDefenseBias(const std::string& range) {
        // Foundation-scope heuristic: close-range fighters tend to eat more
        // hits and need to be tankier to be viable; far-range fighters lean
        // on distance instead of raw durability. A real tuning pass should
        // read a dedicated "durability" genome field once designers want to
        // set it independently -- this derivation is a placeholder mapping,
        // not a design claim.
        if (range == "close") return 0.6f;
        if (range == "mid") return 0.5f;
        if (range == "far") return 0.35f;
        return 0.5f;
    }
};

}  // namespace dominus::character
