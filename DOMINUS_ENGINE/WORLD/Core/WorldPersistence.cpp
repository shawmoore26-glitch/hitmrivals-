// WORLD/Core/WorldPersistence.cpp
#include "WORLD/Core/WorldPersistence.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::world {

using core::VoidResult;
using core::json::Array;
using core::json::Object;
using core::json::Value;

namespace {

std::string DimensionToString(Dimension d) {
    switch (d) {
        case Dimension::k2D: return "2D";
        case Dimension::k2_5D: return "2.5D";
        case Dimension::k3D: return "3D";
    }
    return "2D";
}

Dimension DimensionFromString(const std::string& s) {
    if (s == "2.5D") return Dimension::k2_5D;
    if (s == "3D") return Dimension::k3D;
    return Dimension::k2D;
}

std::string ProjectionToString(ProjectionType p) {
    switch (p) {
        case ProjectionType::kSideView: return "side_view";
        case ProjectionType::kTopDown: return "top_down";
        case ProjectionType::kThirdPerson: return "third_person";
        case ProjectionType::kFreeCamera: return "free_camera";
    }
    return "top_down";
}

ProjectionType ProjectionFromString(const std::string& s) {
    if (s == "side_view") return ProjectionType::kSideView;
    if (s == "third_person") return ProjectionType::kThirdPerson;
    if (s == "free_camera") return ProjectionType::kFreeCamera;
    return ProjectionType::kTopDown;
}

bool WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << content;
    return true;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

VoidResult WorldPersistence::Save(const World& world, const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir / "entities", ec);
    if (ec) return VoidResult::Fail("failed to create entities/ directory: " + ec.message());
    std::filesystem::create_directories(dir / "history", ec);
    if (ec) return VoidResult::Fail("failed to create history/ directory: " + ec.message());

    // world.json
    Object worldObj;
    worldObj["world_version"] = Value(std::string("0.1.0"));
    worldObj["elapsed_seconds"] = Value(static_cast<double>(world.ElapsedSeconds()));
    worldObj["entity_count"] = Value(static_cast<double>(world.Entities().Count()));
    if (!WriteFile(dir / "world.json", Value(worldObj).Dump())) {
        return VoidResult::Fail("failed to write world.json");
    }

    // entities/<id>.json
    for (const auto& id : world.Entities().AllIds()) {
        const auto* entity = world.Entities().Find(id);
        if (!entity) continue;

        Object entityObj;
        entityObj["entity_id"] = Value(id);

        if (const auto* sourceRef = entity->GetComponent<SourceRefComponent>()) {
            entityObj["source_ref"] = Value(sourceRef->dominus_path);
        } else {
            entityObj["source_ref"] = Value(std::string(""));
        }

        if (const auto* spatial = entity->GetComponent<SpatialComponent>()) {
            Object spatialObj;
            spatialObj["dimension"] = Value(DimensionToString(spatial->dimension));
            spatialObj["projection"] = Value(ProjectionToString(spatial->projection));
            spatialObj["x"] = Value(static_cast<double>(spatial->x));
            spatialObj["y"] = Value(static_cast<double>(spatial->y));
            if (spatial->z.has_value()) {
                spatialObj["z"] = Value(static_cast<double>(*spatial->z));
            }
            entityObj["spatial"] = Value(spatialObj);
        }

        if (!WriteFile(dir / "entities" / (id + ".json"), Value(entityObj).Dump())) {
            return VoidResult::Fail("failed to write entities/" + id + ".json");
        }
    }

    // history/timeline.json
    Array eventsArray;
    for (const auto& event : world.History().Events()) {
        Object eventObj;
        eventObj["tick_time"] = Value(static_cast<double>(event.tick_time));
        eventObj["event_type"] = Value(event.event_type);
        eventObj["entity_id"] = Value(event.entity_id);
        eventObj["description"] = Value(event.description);
        Array consequencesArray;
        for (const auto& c : event.consequences) consequencesArray.push_back(Value(c));
        eventObj["consequences"] = Value(consequencesArray);
        eventsArray.push_back(Value(eventObj));
    }
    Object historyObj;
    historyObj["events"] = Value(eventsArray);
    if (!WriteFile(dir / "history" / "timeline.json", Value(historyObj).Dump())) {
        return VoidResult::Fail("failed to write history/timeline.json");
    }

    return VoidResult::Ok();
}

WorldLoadResult WorldPersistence::Load(const std::filesystem::path& dir) {
    WorldLoadResult result;

    std::string worldText;
    try {
        worldText = ReadFile(dir / "world.json");
    } catch (const std::exception& e) {
        result.error = std::string("failed to read world.json: ") + e.what();
        return result;
    }

    Value worldJson;
    try {
        worldJson = Value::Parse(worldText);
    } catch (const std::exception& e) {
        result.error = std::string("failed to parse world.json: ") + e.what();
        return result;
    }
    if (const auto* elapsed = worldJson.Get("elapsed_seconds")) {
        if (elapsed->IsNumber()) result.elapsed_seconds = static_cast<float>(elapsed->AsNumber());
    }

    // entities/*.json
    std::error_code ec;
    if (std::filesystem::exists(dir / "entities", ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir / "entities")) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;

            std::string text;
            try {
                text = ReadFile(entry.path());
            } catch (...) {
                continue;  // skip an unreadable entry rather than fail the whole load
            }

            Value entityJson;
            try {
                entityJson = Value::Parse(text);
            } catch (...) {
                continue;  // skip a malformed entry rather than fail the whole load
            }

            EntitySaveRecord record;
            if (const auto* id = entityJson.Get("entity_id")) {
                if (id->IsString()) record.entity_id = id->AsString();
            }
            if (record.entity_id.empty()) continue;

            if (const auto* sourceRef = entityJson.Get("source_ref")) {
                if (sourceRef->IsString()) record.source_ref = sourceRef->AsString();
            }

            if (const auto* spatial = entityJson.Get("spatial")) {
                record.has_spatial = true;
                if (const auto* dim = spatial->Get("dimension")) {
                    if (dim->IsString()) record.spatial.dimension = DimensionFromString(dim->AsString());
                }
                if (const auto* proj = spatial->Get("projection")) {
                    if (proj->IsString()) record.spatial.projection = ProjectionFromString(proj->AsString());
                }
                if (const auto* x = spatial->Get("x")) {
                    if (x->IsNumber()) record.spatial.x = static_cast<float>(x->AsNumber());
                }
                if (const auto* y = spatial->Get("y")) {
                    if (y->IsNumber()) record.spatial.y = static_cast<float>(y->AsNumber());
                }
                if (const auto* z = spatial->Get("z")) {
                    if (z->IsNumber()) record.spatial.z = static_cast<float>(z->AsNumber());
                }
            }

            result.entities.push_back(std::move(record));
        }
    }

    // history/timeline.json -- optional; a save with zero recorded events
    // is still a valid save.
    try {
        std::string historyText = ReadFile(dir / "history" / "timeline.json");
        Value historyJson = Value::Parse(historyText);
        if (const auto* events = historyJson.Get("events")) {
            if (events->IsArray()) {
                for (const Value& eventVal : events->AsArray()) {
                    HistoryEvent event;
                    if (const auto* t = eventVal.Get("tick_time")) {
                        if (t->IsNumber()) event.tick_time = static_cast<float>(t->AsNumber());
                    }
                    if (const auto* et = eventVal.Get("event_type")) {
                        if (et->IsString()) event.event_type = et->AsString();
                    }
                    if (const auto* eid = eventVal.Get("entity_id")) {
                        if (eid->IsString()) event.entity_id = eid->AsString();
                    }
                    if (const auto* desc = eventVal.Get("description")) {
                        if (desc->IsString()) event.description = desc->AsString();
                    }
                    if (const auto* consequences = eventVal.Get("consequences")) {
                        if (consequences->IsArray()) {
                            for (const Value& c : consequences->AsArray()) {
                                if (c.IsString()) event.consequences.push_back(c.AsString());
                            }
                        }
                    }
                    result.history.push_back(std::move(event));
                }
            }
        }
    } catch (...) {
        // No history file -- fine, treat as zero events.
    }

    result.ok = true;
    return result;
}

}  // namespace dominus::world
