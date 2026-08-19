// COMBAT/HitSystem/AssetValidation.h
// LAW C014 (no fake systems), made checkable before runtime: a move whose
// motion_trigger has no corresponding transition anywhere in the bound
// motion graph will always have StartMove() honestly refuse it -- that
// refusal is correct behavior, but it usually means content is missing,
// not that the refusal itself is wrong. This validator surfaces that gap
// as a report instead of discovering it move-by-move at runtime.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "ANIMATION/AnimationGraph/MotionGraph.h"
#include "COMBAT/HitSystem/CombatComponents.h"

namespace dominus::combat {

struct MotionCoverageReport {
    std::vector<std::string> unreachable_moves;  // sorted, deterministic
    bool AllMovesResolve() const { return unreachable_moves.empty(); }
};

class AssetValidation {
public:
    // A move "resolves" if at least one transition in the graph fires on
    // its motion_trigger -- checking transitions rather than state names
    // catches the (currently theoretical but real) case of a state that
    // exists but nothing actually transitions into it.
    static bool MoveResolves(const MoveDef& move, const animation::MotionGraph& graph) {
        for (const auto& t : graph.transitions) {
            if (t.trigger == move.motion_trigger) return true;
        }
        return false;
    }

    static MotionCoverageReport CheckMotionCoverage(const MoveSetComponent& moves, const animation::MotionGraph& graph) {
        MotionCoverageReport report;
        for (const auto& [name, move] : moves.moves) {
            if (!MoveResolves(move, graph)) report.unreachable_moves.push_back(name);
        }
        std::sort(report.unreachable_moves.begin(), report.unreachable_moves.end());
        return report;
    }
};

}  // namespace dominus::combat
