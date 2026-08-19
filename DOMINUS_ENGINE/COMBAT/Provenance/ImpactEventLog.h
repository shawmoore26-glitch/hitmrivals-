// COMBAT/Provenance/ImpactEventLog.h
// Record -> Serialize -> Reload -- the other half of the provenance
// proof (Replay lives in ImpactEventCompiler::VerifyMatches). An
// append-only, in-memory log of ImpactEvents, same shape discipline as
// WORLD::WorldHistory (append, don't lose it, no querying beyond a
// plain linear filter). Serialize()/Deserialize() round-trip through
// CORE::MiniJson -- the same zero-dependency JSON type every other
// on-disk format in this engine already uses -- producing exactly the
// schema shape specified: {"event","attacker","target","context_hash",
// "result_hash","tick"} per entry.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CORE/Serialization/MiniJson.h"
#include "COMBAT/Provenance/ImpactEvent.h"

namespace dominus::combat {

class ImpactEventLog {
public:
    void Record(ImpactEvent event) { events_.push_back(std::move(event)); }

    const std::vector<ImpactEvent>& Events() const { return events_; }
    size_t Count() const { return events_.size(); }

    std::vector<ImpactEvent> EventsForEntity(const std::string& entityId) const {
        std::vector<ImpactEvent> result;
        for (const auto& e : events_) {
            if (e.attacker == entityId || e.target == entityId) result.push_back(e);
        }
        return result;
    }

    // Serialize: a plain JSON array, one object per event, exactly the
    // schema the directive specified.
    std::string Serialize() const {
        core::json::Array arr;
        for (const auto& e : events_) {
            core::json::Object obj;
            obj["event"] = core::json::Value(e.event);
            obj["attacker"] = core::json::Value(e.attacker);
            obj["target"] = core::json::Value(e.target);
            obj["context_hash"] = core::json::Value(e.context_hash);
            obj["result_hash"] = core::json::Value(e.result_hash);
            obj["tick"] = core::json::Value(static_cast<double>(e.tick));
            arr.push_back(core::json::Value(obj));
        }
        return core::json::Value(arr).Dump();
    }

    // Reload: parses Serialize()'s own output back into a fresh log --
    // proves round-trip fidelity, not just in-memory storage. Throws
    // (via MiniJson's own Parse) on malformed input, same failure mode
    // every other MiniJson-based loader in this engine already has.
    static ImpactEventLog Deserialize(const std::string& jsonText) {
        ImpactEventLog log;
        core::json::Value root = core::json::Value::Parse(jsonText);
        if (!root.IsArray()) return log;  // empty log, same graceful-degrade discipline as everywhere else

        for (const auto& item : root.AsArray()) {
            ImpactEvent e;
            if (auto* v = item.Get("event")) e.event = v->AsString();
            if (auto* v = item.Get("attacker")) e.attacker = v->AsString();
            if (auto* v = item.Get("target")) e.target = v->AsString();
            if (auto* v = item.Get("context_hash")) e.context_hash = v->AsString();
            if (auto* v = item.Get("result_hash")) e.result_hash = v->AsString();
            if (auto* v = item.Get("tick")) e.tick = static_cast<std::uint64_t>(v->AsNumber());
            log.events_.push_back(std::move(e));
        }
        return log;
    }

private:
    std::vector<ImpactEvent> events_;
};

}  // namespace dominus::combat
