// AI/Agents/CombatAI.h
// LAW C011 + the Phase 3.75 directive: Player Action -> CombatAI ->
// GenomeDecoder -> DecisionWeights -> MoveSelector -> Motion Graph ->
// Skeleton Runtime. CombatAI now carries DecisionWeights (decoded once by
// the caller via CHARACTER::GenomeDecoder, not re-decoded here -- CombatAI
// doesn't need to know about CombatIdentity strings, only the numeric
// weights) and uses them two ways: biasing the behavior tree's own
// thresholds (a poor counter-fighter won't even attempt the counter
// branch), and picking the actual move via COMBAT::MoveSelector once a
// high-level category is chosen. Two genomes facing an identical opponent
// pattern can now genuinely decide differently -- verified by test.
#pragma once

#include <deque>
#include <string>
#include <unordered_map>

#include "AI/Agents/BehaviorTree.h"
#include "CHARACTER/Genome/DecisionWeights.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/MoveSelector.h"
#include "COMBAT/Profiles.h"

namespace dominus::ai {

class OpponentPatternTracker {
public:
    explicit OpponentPatternTracker(size_t historySize = 8) : historySize_(historySize) {}

    void RecordMove(const std::string& moveName) {
        history_.push_back(moveName);
        if (history_.size() > historySize_) history_.pop_front();
    }

    // Most frequent move in the retained window, or empty string if no
    // history yet. Ties resolve to whichever was inserted first among the
    // max-count set (stable, not random) -- deterministic AI is required
    // by LAW C014 ("no fake systems") for reproducible unit tests.
    std::string MostFrequentMove() const {
        std::unordered_map<std::string, int> counts;
        for (auto& m : history_) counts[m]++;

        std::string best;
        int bestCount = 0;
        for (auto& m : history_) {
            int c = counts[m];
            if (c > bestCount) {
                bestCount = c;
                best = m;
            }
        }
        return best;
    }

    bool LastMoveWas(const std::string& moveName) const {
        return !history_.empty() && history_.back() == moveName;
    }

    size_t HistorySize() const { return history_.size(); }

private:
    std::deque<std::string> history_;
    size_t historySize_;
};

class CombatAI {
public:
    // `weights` defaults to neutral (0.5 everywhere), reproducing Phase 3's
    // original fixed-threshold behavior exactly -- backward compatible.
    explicit CombatAI(OpponentPatternTracker& tracker, character::DecisionWeights weights = character::DecisionWeights{})
        : tracker_(tracker), weights_(weights) {
        BuildTree();
    }

    // High-level category decision: "counter", "block", or "attack".
    std::string Decide() {
        decision_.clear();
        tree_->Tick();
        return decision_.empty() ? "attack" : decision_;
    }

    // The full pipeline endpoint: Decide() a category, then -- for
    // "attack"/"counter" -- use MoveSelector to pick the actual move name
    // from what's really available, weighted by this fighter's genome.
    // Returns "" for "block" (not a move) or if no candidate moves exist.
    std::string DecideMoveName(const combat::MoveSetComponent& moves, const std::vector<std::string>& attackCandidates) {
        std::string category = Decide();
        if (category == "block") return "";

        if (category == "counter" && moves.Find("counter") != nullptr) {
            return "counter";
        }
        // Falls through to attack-move selection if "counter" was chosen
        // but no counter move exists on this fighter -- never returns a
        // move name that doesn't actually exist (LAW C014).
        return combat::MoveSelector::SelectBest(moves, attackCandidates, weights_);
    }

    // Applies an AIProfile on top of the current genome weights -- used by
    // transformations (LAW C010 expanded, Phase 3.75 Priority 3): a form
    // change can make the SAME fighter noticeably more reckless without
    // touching their base CombatIdentity. difficulty scales aggression and
    // counter_bias around their existing values rather than replacing them
    // outright, so a cautious base genome doesn't become identical to an
    // aggressive one just from a high-difficulty profile.
    void ApplyProfile(const combat::AIProfile& profile) {
        float scale = 0.5f + profile.difficulty;  // 0.5x at difficulty=0, 1.5x at difficulty=1
        weights_.aggression = Clamp01(weights_.aggression * scale);
        weights_.counter_bias = Clamp01(weights_.counter_bias * scale);
    }

    const character::DecisionWeights& Weights() const { return weights_; }

private:
    static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    void BuildTree() {
        auto root = std::make_unique<SelectorNode>();

        // Counter branch only exists at all if this genome has ANY
        // counter aptitude (counter_bias > 0.3) -- a poor counter-fighter
        // (e.g. counter_bias ~0.15 from GenomeDecoder's "poor" mapping)
        // never even attempts it, falling through to block/attack instead.
        // This is the actual behavioral difference LAW C003 requires: two
        // fighters facing the identical opponent pattern decide
        // differently based on genome, not just move-scoring after the
        // fact.
        auto counterSeq = std::make_unique<SequenceNode>();
        counterSeq->AddChild(std::make_unique<ConditionNode>([this]() {
            return weights_.counter_bias > 0.3f && tracker_.MostFrequentMove() == "jab" && tracker_.HistorySize() >= 3;
        }));
        counterSeq->AddChild(std::make_unique<ActionNode>([this]() {
            decision_ = "counter";
            return NodeStatus::kSuccess;
        }));
        root->AddChild(std::move(counterSeq));

        // If the opponent just attacked, block is the safe reactive choice
        // -- but a highly aggressive fighter (aggression > 0.8) presses
        // forward instead of blocking, another genuine genome-driven split.
        auto blockSeq = std::make_unique<SequenceNode>();
        blockSeq->AddChild(std::make_unique<ConditionNode>(
            [this]() { return tracker_.LastMoveWas("jab") && weights_.aggression <= 0.8f; }));
        blockSeq->AddChild(std::make_unique<ActionNode>([this]() {
            decision_ = "block";
            return NodeStatus::kSuccess;
        }));
        root->AddChild(std::move(blockSeq));

        // Default: press the attack.
        root->AddChild(std::make_unique<ActionNode>([this]() {
            decision_ = "attack";
            return NodeStatus::kSuccess;
        }));

        tree_ = std::move(root);
    }

    OpponentPatternTracker& tracker_;
    character::DecisionWeights weights_;
    std::unique_ptr<SelectorNode> tree_;
    std::string decision_;
};

}  // namespace dominus::ai
