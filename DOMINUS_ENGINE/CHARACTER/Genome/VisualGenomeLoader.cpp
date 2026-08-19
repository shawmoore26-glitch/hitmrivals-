// CHARACTER/Genome/VisualGenomeLoader.cpp
#include "CHARACTER/Genome/VisualGenomeLoader.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::character {

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

float NumOr(const Value& root, const char* key, float def) {
    auto* v = root.Get(key);
    return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : def;
}

std::string StrOr(const Value& root, const char* key, const std::string& def = "") {
    auto* v = root.Get(key);
    return v && v->IsString() ? v->AsString() : def;
}

bool BoolOr(const Value& root, const char* key, bool def) {
    auto* v = root.Get(key);
    return v && v->IsBool() ? v->AsBool() : def;
}

std::vector<std::string> Validate(const VisualGenome& g) {
    std::vector<std::string> errors;

    auto checkUnit = [&errors](float value, const char* field) {
        if (value < 0.0f || value > 1.0f) {
            errors.push_back(std::string(field) + " must be within [0, 1], got " + std::to_string(value));
        }
    };
    checkUnit(g.skin.roughness, "skin.roughness");
    checkUnit(g.skin.subsurface, "skin.subsurface");
    checkUnit(g.presence.threat_signature, "presence.threat_signature");
    checkUnit(g.presence.emotional_visual_weight, "presence.emotional_visual_weight");

    if (g.skin.age_years <= 0) {
        errors.push_back("skin.age_years must be > 0");
    }

    return errors;
}

}  // namespace

Result<VisualGenome> VisualGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<VisualGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<VisualGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    VisualGenome g;
    if (auto* form = root.Get("form")) {
        g.form.body_type = StrOr(*form, "type");
        g.form.silhouette = StrOr(*form, "silhouette");
        g.form.proportion = StrOr(*form, "proportion");
        g.form.shape_language = StrOr(*form, "shape_language");
    }
    if (auto* surface = root.Get("surface")) {
        if (auto* skin = surface->Get("skin")) {
            g.skin.roughness = NumOr(*skin, "roughness", 0.5f);
            g.skin.subsurface = NumOr(*skin, "subsurface", 0.2f);
            g.skin.age_years = static_cast<int>(NumOr(*skin, "age", 25.0f));
            g.skin.damage_response = BoolOr(*skin, "damage_response", true);
        }
        if (auto* clothing = surface->Get("clothing")) {
            g.clothing.material = StrOr(*clothing, "material");
            g.clothing.adaptive_damage = BoolOr(*clothing, "adaptive_damage", true);
            g.clothing.weather_response = BoolOr(*clothing, "weather_response", true);
        }
    }
    if (auto* presence = root.Get("presence")) {
        g.presence.aura = StrOr(*presence, "aura");
        g.presence.threat_signature = NumOr(*presence, "threat_signature", 0.5f);
        g.presence.emotional_visual_weight = NumOr(*presence, "emotional_visual_weight", 0.5f);
        g.presence.style_id = StrOr(*presence, "style_id");
    }

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined = "VisualGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<VisualGenome>::Fail(combined);
    }

    return Result<VisualGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
