// COMBAT/HitSystem/HurtboxLoader.cpp
#include "COMBAT/HitSystem/HurtboxLoader.h"

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
float FloatOr(const Value& root, const char* key, float def) {
    auto* v = root.Get(key);
    return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : def;
}
}  // namespace

Result<HurtboxSet> HurtboxLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<HurtboxSet>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<HurtboxSet>::Fail(std::string("Parse error: ") + e.what());
    }

    const Value* boxesVal = root.Get("hurtboxes");
    if (!boxesVal || !boxesVal->IsArray()) {
        return Result<HurtboxSet>::Fail("hurtbox file missing 'hurtboxes' array: " + path.string());
    }

    HurtboxSet set;
    for (const Value& b : boxesVal->AsArray()) {
        if (!b.Has("bone")) continue;
        HurtboxDef def;
        def.bone = b.Get("bone")->AsString();
        def.radius = FloatOr(b, "radius", 5.0f);
        def.offset_x = FloatOr(b, "offset_x", 0.0f);
        def.offset_y = FloatOr(b, "offset_y", 0.0f);
        set.boxes.push_back(def);
    }

    return Result<HurtboxSet>::Ok(std::move(set));
}

}  // namespace dominus::combat
