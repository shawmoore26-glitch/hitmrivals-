// CHARACTER/Genome/VisualMemorySummary.h
// The real version of "objects remember: battles, kills, damage%."
// Deliberately does NOT invent a second history-tracking system --
// this derives a summary purely from WORLD::WorldHistory
// (Society Phase 0), which already records real, timestamped,
// entity-tagged events. No fabricated "kills: 1204" -- event_count is
// however many real events were actually recorded against this entity.
//
// This is the one file in CHARACTER/Genome that depends on WORLD, and
// deliberately so: it's a connector, not a core genome type. Same
// pattern as every CLI/test file that already freely combines
// WORLD+CHARACTER+COMBAT (Phase 4.0's world-entity proof, MASTER OF
// COMBAT's fixtures) -- this just makes that connection reusable and
// testable instead of re-deriving it inline every time.
#pragma once

#include <string>

#include "WORLD/Core/WorldHistory.h"

namespace dominus::character {

struct VisualMemorySummary {
    size_t event_count = 0;
    bool has_history = false;
    float first_event_time = 0.0f;
    float last_event_time = 0.0f;
};

class VisualMemoryDeriver {
public:
    static VisualMemorySummary Derive(const world::WorldHistory& history, const std::string& entityId) {
        VisualMemorySummary summary;
        auto events = history.EventsForEntity(entityId);
        summary.event_count = events.size();
        summary.has_history = !events.empty();
        if (!events.empty()) {
            summary.first_event_time = events.front().tick_time;
            summary.last_event_time = events.back().tick_time;
        }
        return summary;
    }
};

}  // namespace dominus::character
