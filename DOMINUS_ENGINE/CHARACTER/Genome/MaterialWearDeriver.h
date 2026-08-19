// CHARACTER/Genome/MaterialWearDeriver.h
// The document's own worked example: "Sword is damaged because: EVENT
// Battle_482, RESULT Durability -15%. Not: 'Renderer decided sword
// looks damaged.'" Same principle as VisualMemoryDeriver, proven a
// second time on a different genome type: wear_state is derived from
// WORLD::WorldHistory's real recorded events, never hand-invented.
//
// The per-event wear increment (0.05, i.e. 20 damage-type events before
// an object reads as fully worn) is a deliberately simple, clearly-
// labeled heuristic -- same honest-heuristic discipline as
// ImpactSolver's wing-loading threshold and its kg/m^2 constant. It is
// NOT a claim that real damage severity was consulted (WorldHistory's
// `description`/`consequences` fields are free-form strings this
// deriver doesn't parse) -- only that a real, counted number of
// matching events occurred. A richer version that reads actual damage
// magnitudes per event would need WorldHistory to carry structured
// payloads instead of free text, which is real, separate future work
// (see ROADMAP.md).
#pragma once

#include <algorithm>
#include <string>

#include "WORLD/Core/WorldHistory.h"

namespace dominus::character {

class MaterialWearDeriver {
public:
    // Counts events on `entityId` whose event_type is EXACTLY
    // "damage_event" -- a documented convention, not a substring match.
    // A substring match (`find("damage") != npos`) was tried first and
    // had a real bug: "non_damage_event" contains "damage" as a
    // substring and would have been incorrectly counted. Exact match on
    // one agreed-upon type string avoids that whole class of false
    // positive.
    static float DeriveWearState(const world::WorldHistory& history, const std::string& entityId) {
        const float kWearPerDamageEvent = 0.05f;  // 20 damage events -> fully worn; a simple, labeled heuristic

        auto events = history.EventsForEntity(entityId);
        size_t damageEventCount = 0;
        for (auto& event : events) {
            if (event.event_type == "damage_event") {
                ++damageEventCount;
            }
        }

        float wear = static_cast<float>(damageEventCount) * kWearPerDamageEvent;
        return std::min(wear, 1.0f);
    }

    // Whether ANY event exists for this entity at all -- real presence
    // check, distinct from wear magnitude (an object can have history
    // without any of it being damage-typed).
    static bool HasAnyHistory(const world::WorldHistory& history, const std::string& entityId) {
        return !history.EventsForEntity(entityId).empty();
    }
};

}  // namespace dominus::character
