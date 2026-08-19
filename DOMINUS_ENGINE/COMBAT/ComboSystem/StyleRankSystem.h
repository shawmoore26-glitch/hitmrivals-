// COMBAT/ComboSystem/StyleRankSystem.h
// Devil May Cry-style live style grade. Score is a weighted combination of
// what the fight looked like -- variety (distinct moves used), aggression
// (hit volume), risk (how many high-risk moves landed), creativity (a
// bonus for genuinely varied play, not just spamming one risky move), and
// damage avoidance (damage dealt vs taken). Thresholds are foundation-
// scope constants, tuned against a handful of hand-checked scenarios, not
// a fully balanced design -- a real content pass would tune these against
// actual playtested matches, not invent them in isolation.
#pragma once

#include <algorithm>

namespace dominus::combat {

enum class StyleRank { kD, kC, kB, kA, kS, kSS, kSSS };

struct StyleMetrics {
    int hit_count = 0;
    int distinct_move_count = 0;
    int high_risk_move_count = 0;
    float damage_dealt = 0.0f;
    float damage_taken = 0.0f;

    // Phase 3.75 additions: recordable via StyleCollector, NOT yet folded
    // into Score() -- no real detection exists in-engine for these events
    // yet (see StyleCollector.h). Present so the telemetry API is complete
    // and future detection systems have somewhere to report into, without
    // silently changing every existing StyleRank test's expected score.
    int counter_count = 0;
    int perfect_dodge_count = 0;
    int parry_count = 0;
    float air_time_seconds = 0.0f;
};

class StyleRankSystem {
public:
    static float Score(const StyleMetrics& m) {
        float varietyScore = static_cast<float>(m.distinct_move_count) * 10.0f;
        float aggressionScore = static_cast<float>(m.hit_count) * 2.0f;
        float riskScore = static_cast<float>(m.high_risk_move_count) * 5.0f;
        float creativityBonus = m.distinct_move_count >= 3 ? 10.0f : 0.0f;

        // Damage avoidance: dealing damage while taking little is rewarded;
        // taking more than dealing is penalized, but never lets total score
        // go meaningfully negative from this term alone (clamped below).
        float avoidanceTerm = m.damage_dealt - m.damage_taken;

        float total = varietyScore + aggressionScore + riskScore + creativityBonus + avoidanceTerm;
        return std::max(total, 0.0f);
    }

    static StyleRank Evaluate(const StyleMetrics& m) {
        float score = Score(m);
        if (score >= 140.0f) return StyleRank::kSSS;
        if (score >= 100.0f) return StyleRank::kSS;
        if (score >= 70.0f) return StyleRank::kS;
        if (score >= 45.0f) return StyleRank::kA;
        if (score >= 25.0f) return StyleRank::kB;
        if (score >= 10.0f) return StyleRank::kC;
        return StyleRank::kD;
    }

    static const char* RankName(StyleRank rank) {
        switch (rank) {
            case StyleRank::kD: return "D";
            case StyleRank::kC: return "C";
            case StyleRank::kB: return "B";
            case StyleRank::kA: return "A";
            case StyleRank::kS: return "S";
            case StyleRank::kSS: return "SS";
            case StyleRank::kSSS: return "SSS";
        }
        return "?";
    }
};

}  // namespace dominus::combat
