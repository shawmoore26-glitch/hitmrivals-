// COMBAT/Provenance/ImpactEventWorldHistoryHook.h
// Connects ImpactEvent to WORLD::WorldHistory -- the exact connection
// the directive asked for ("that connects directly to the WorldHistory
// decision you previously emphasized"). WorldHistory's own schema
// (WORLD/Core/WorldHistory.h) is completely untouched: event_type,
// entity_id, description, and consequences are all pre-existing, free-
// form fields WORLD already stays deliberately ignorant of the meaning
// of (same discipline as "reputation +20"/"enemy faction created" from
// Society Phase 1). This hook is the domain-side interpreter, not a
// WorldHistory schema change.
//
// The AUTHORITATIVE provenance record is still ImpactEventLog's own
// JSON schema (ImpactEventLog.h) -- this hook exists so an impact also
// shows up in the world's general, narrative-queryable history
// (WorldHistory::EventsForEntity), not to replace ImpactEventLog as the
// replay source of truth.
//
// Dependency direction: COMBAT -> WORLD here is the same direction
// PHYSICS -> WORLD already established in Phase 4.1 ("WORLD knows
// physics exists" / combat is a domain extension running on WORLD per
// the Architectural correction before Phase 4.0) -- not the forbidden
// direction, which is WORLD depending on COMBAT.
#pragma once

#include <sstream>

#include "COMBAT/Provenance/ImpactEvent.h"
#include "WORLD/Core/WorldHistory.h"

namespace dominus::combat {

// tickTime: WorldHistory's own field is a float "tick_time", distinct
// from ImpactEvent's uint64_t simulation tick -- callers supply
// whichever real time value their World uses; this hook doesn't invent
// a conversion because none is authoritative today (no fixed tick-rate
// is declared anywhere in WORLD/Core).
inline void RecordImpactEvent(world::WorldHistory& history, const ImpactEvent& event, float tickTime) {
    std::ostringstream description;
    description << event.attacker << " struck " << event.target;

    std::vector<std::string> consequences = {
        "target=" + event.target,
        "context_hash=" + event.context_hash,
        "result_hash=" + event.result_hash,
        "tick=" + std::to_string(event.tick),
    };

    history.Record(tickTime, event.event, event.attacker, description.str(), std::move(consequences));
}

}  // namespace dominus::combat
