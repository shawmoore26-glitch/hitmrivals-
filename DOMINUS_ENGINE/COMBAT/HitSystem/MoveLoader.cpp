// COMBAT/HitSystem/MoveLoader.cpp
#include "COMBAT/HitSystem/MoveLoader.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::combat {

using core::Result;
using core::json::Value;

namespace {
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
int IntOr(const Value& root, const char* key, int def) {
    auto* v = root.Get(key);
    return v && v->IsNumber() ? static_cast<int>(v->AsNumber()) : def;
}
float FloatOr(const Value& root, const char* key, float def) {
    auto* v = root.Get(key);
    return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : def;
}
std::string StrOr(const Value& root, const char* key, const std::string& def) {
    auto* v = root.Get(key);
    return v && v->IsString() ? v->AsString() : def;
}
}  // namespace

// Expected format: see docs/ARCHITECTURE_v0.1.md section 5 (Phase 3
// addendum) for the full worked example.
Result<MoveDef> MoveLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<MoveDef>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<MoveDef>::Fail(std::string("Parse error: ") + e.what());
    }

    if (!root.Has("name")) {
        return Result<MoveDef>::Fail("move file missing required 'name' field: " + path.string());
    }

    MoveDef move;
    move.name = root.Get("name")->AsString();
    move.motion_trigger = StrOr(root, "motion_trigger", move.name);
    move.power = FloatOr(root, "power", 10.0f);
    move.speed = FloatOr(root, "speed", 10.0f);

    if (const Value* frames = root.Get("frames")) {
        move.frames.startup = IntOr(*frames, "startup", 0);
        move.frames.active = IntOr(*frames, "active", 0);
        move.frames.recovery = IntOr(*frames, "recovery", 0);
        move.frames.hit_advantage = IntOr(*frames, "hit_advantage", 0);
        move.frames.block_advantage = IntOr(*frames, "block_advantage", 0);
        move.frames.cancel_window = IntOr(*frames, "cancel_window", 0);
    }

    if (const Value* intent = root.Get("intent")) {
        move.intent.purpose = StrOr(*intent, "purpose", "");
        move.intent.risk = StrOr(*intent, "risk", "medium");
        if (const Value* followups = intent->Get("followups")) {
            if (followups->IsArray()) {
                for (const Value& f : followups->AsArray()) {
                    if (f.IsString()) move.intent.followups.push_back(f.AsString());
                }
            }
        }
    }

    if (const Value* hitboxes = root.Get("hitboxes")) {
        if (hitboxes->IsArray()) {
            for (const Value& hb : hitboxes->AsArray()) {
                if (!hb.Has("bone")) continue;
                HitboxDef def;
                def.bone = hb.Get("bone")->AsString();
                def.radius = FloatOr(hb, "radius", 5.0f);
                def.offset_x = FloatOr(hb, "offset_x", 0.0f);
                def.offset_y = FloatOr(hb, "offset_y", 0.0f);
                move.hitboxes.push_back(def);
            }
        }
    }

    return Result<MoveDef>::Ok(std::move(move));
}

}  // namespace dominus::combat
