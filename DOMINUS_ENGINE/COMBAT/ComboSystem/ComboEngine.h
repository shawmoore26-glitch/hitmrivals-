// COMBAT/ComboSystem/ComboEngine.h
// LAW C005/C007: cancels are legal only inside a move's own cancel_window,
// and only into a move that's actually a listed followup for the current
// move -- combo legality reads off MoveDef data, never a hardcoded combo
// table.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "COMBAT/HitSystem/MoveDef.h"

namespace dominus::combat {

class ComboEngine {
public:
    // `elapsedFrames` is how far into the currently-executing move the
    // fighter is. Cancel is legal only if elapsedFrames falls inside
    // [startup, startup+active+cancel_window) -- i.e. the move has at
    // least started, and the cancel window hasn't closed -- AND
    // `nextMoveName` appears in the current move's intent.followups.
    static bool CanCancelInto(const MoveDef& currentMove, int elapsedFrames, const std::string& nextMoveName) {
        int cancelOpensAt = currentMove.frames.startup;
        int cancelClosesAt = currentMove.frames.startup + currentMove.frames.active + currentMove.frames.cancel_window;
        if (elapsedFrames < cancelOpensAt || elapsedFrames >= cancelClosesAt) return false;

        for (const auto& followup : currentMove.intent.followups) {
            if (followup == nextMoveName) return true;
        }
        return false;
    }

    // Tracks a running combo (move name sequence) for display/scoring
    // purposes (LAW C004's "style rating" concept from stylish-action
    // combat) -- purely additive bookkeeping, doesn't gate legality itself.
    void RecordHit(const std::string& moveName) { chain_.push_back(moveName); }
    void Reset() { chain_.clear(); }
    const std::vector<std::string>& Chain() const { return chain_; }
    size_t ComboCount() const { return chain_.size(); }

private:
    std::vector<std::string> chain_;
};

}  // namespace dominus::combat
