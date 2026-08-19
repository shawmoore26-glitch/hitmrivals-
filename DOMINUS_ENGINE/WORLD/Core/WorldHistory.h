// WORLD/Core/WorldHistory.h
// "A character should remember: the player saved my village 10 years
// ago." Foundation scope: an append-only, in-memory log of timestamped,
// generic events (tick_time, event_type, entity_id, description). WORLD
// doesn't know what a "battle" or "betrayal" is any more than it knows
// what a "fighter" is -- event_type and description are free-form
// strings an extension chooses, same pattern as everything else WORLD
// stays deliberately ignorant of. This is the substrate a real memory/
// history system would be built on, not that system itself -- no
// querying by entity, no causal linking between events, no decay or
// summarization. Just: append, and don't lose it.
#pragma once

#include <string>
#include <vector>

namespace dominus::world {

struct HistoryEvent {
    float tick_time = 0.0f;
    std::string event_type;
    std::string entity_id;
    std::string description;
    std::vector<std::string> consequences;  // free-form ("reputation +20", ...) -- WORLD doesn't interpret these
};

class WorldHistory {
public:
    void Record(float tickTime, std::string eventType, std::string entityId, std::string description,
                std::vector<std::string> consequences = {}) {
        events_.push_back(HistoryEvent{tickTime, std::move(eventType), std::move(entityId), std::move(description),
                                        std::move(consequences)});
    }

    const std::vector<HistoryEvent>& Events() const { return events_; }
    size_t Count() const { return events_.size(); }

    // "A character should remember: the player saved my village 10 years
    // ago" needs to ask "what happened involving THIS entity" -- a plain
    // linear filter, same honest-scope tradeoff as EntityRegistry's own
    // WithComponent<T>() scan. No indexing, no causal linking between
    // events, just retrieval.
    std::vector<HistoryEvent> EventsForEntity(const std::string& entityId) const {
        std::vector<HistoryEvent> result;
        for (const auto& event : events_) {
            if (event.entity_id == entityId) result.push_back(event);
        }
        return result;
    }

private:
    std::vector<HistoryEvent> events_;
};

}  // namespace dominus::world
