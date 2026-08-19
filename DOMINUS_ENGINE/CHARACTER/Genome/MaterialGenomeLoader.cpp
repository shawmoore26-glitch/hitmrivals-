// CHARACTER/Genome/MaterialGenomeLoader.cpp
#include "CHARACTER/Genome/MaterialGenomeLoader.h"

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

std::vector<std::string> Validate(const MaterialGenome& g) {
    std::vector<std::string> errors;

    if (g.material_id.empty()) {
        errors.push_back("material_id is required and must be non-empty");
    }
    if (g.properties.age_years < 0) {
        errors.push_back("properties.age_years must be >= 0");
    }
    if (g.properties.wear_state < 0.0f || g.properties.wear_state > 1.0f) {
        errors.push_back("properties.wear_state must be within [0, 1], got " +
                          std::to_string(g.properties.wear_state));
    }

    return errors;
}

}  // namespace

Result<MaterialGenome> MaterialGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<MaterialGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<MaterialGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    MaterialGenome g;
    g.material_id = StrOr(root, "material_id");
    if (auto* identity = root.Get("identity")) {
        g.identity.type = StrOr(*identity, "type");
    }
    if (auto* properties = root.Get("properties")) {
        g.properties.age_years = static_cast<int>(NumOr(*properties, "age", 0.0f));
        g.properties.wear_state = NumOr(*properties, "wear_state", 0.0f);
        g.properties.damage_history = BoolOr(*properties, "damage_history", false);
        g.properties.weather_exposure = BoolOr(*properties, "weather_exposure", false);
    }

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined = "MaterialGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<MaterialGenome>::Fail(combined);
    }

    return Result<MaterialGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
