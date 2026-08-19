// COMBAT/ComboSystem/StyleCollector.h
// Combat Event -> Style Collector -> Metrics -> Style Rank. Replaces
// hand-typed StyleMetrics with automatic collection from real combat
// events: RecordHitLanded is called wherever a move actually connects
// (see CombatController::ApplyHit, which now calls RecordDamageTaken on
// the defender's collector, and the demo/CLI layer, which calls
// RecordHitLanded on the attacker's collector once CollisionEvaluator
// confirms a hit). hit_count/distinct_move_count/high_risk_move_count/
// damage_dealt/damage_taken are genuinely auto-populated. perfect_dodge/
// parry/air_time counters exist and are recordable, but nothing in the
// engine automatically detects those events yet (no dodge-timing window,
// no parry system, no airborne state machine) -- flagged honestly, not
// wired into StyleRankSystem::Score until real detection exists.
#pragma once

#include <unordered_set>

#include "COMBAT/ComboSystem/StyleRankSystem.h"
#include "COMBAT/HitSystem/MoveDef.h"

namespace dominus::combat {

class StyleCollector {
public:
    void RecordHitLanded(const MoveDef& move, float damageDealt) {
        metrics_.hit_count++;
        if (usedMoves_.insert(move.name).second) {
            metrics_.distinct_move_count++;
        }
        if (move.intent.risk == "high") metrics_.high_risk_move_count++;
        metrics_.damage_dealt += damageDealt;
        if (move.name == "counter") metrics_.counter_count++;
    }

    void RecordDamageTaken(float damage) { metrics_.damage_taken += damage; }
    void RecordPerfectDodge() { metrics_.perfect_dodge_count++; }
    void RecordParry() { metrics_.parry_count++; }
    void RecordAirTime(float seconds) { metrics_.air_time_seconds += seconds; }

    const StyleMetrics& Metrics() const { return metrics_; }
    StyleRank CurrentRank() const { return StyleRankSystem::Evaluate(metrics_); }
    float CurrentScore() const { return StyleRankSystem::Score(metrics_); }

    void Reset() {
        metrics_ = StyleMetrics{};
        usedMoves_.clear();
    }

private:
    StyleMetrics metrics_;
    std::unordered_set<std::string> usedMoves_;
};

}  // namespace dominus::combat
