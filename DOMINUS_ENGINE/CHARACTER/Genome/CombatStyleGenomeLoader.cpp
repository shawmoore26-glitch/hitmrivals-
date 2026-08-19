// CHARACTER/Genome/CombatStyleGenomeLoader.cpp
#include "CHARACTER/Genome/CombatStyleGenomeLoader.h"

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

std::vector<std::string> StrArrayOr(const Value& root, const char* key) {
    std::vector<std::string> out;
    auto* v = root.Get(key);
    if (v && v->IsArray()) {
        for (const Value& entry : v->AsArray()) {
            if (entry.IsString()) out.push_back(entry.AsString());
        }
    }
    return out;
}

// Strict validation, same philosophy as CreatureGenomeLoader: collects
// every violation rather than stopping at the first.
std::vector<std::string> Validate(const CombatStyleGenome& g) {
    std::vector<std::string> errors;

    if (g.style_name.empty()) {
        errors.push_back("style_name is required and must be non-empty");
    }
    if (g.weaknesses.empty()) {
        errors.push_back(
            "weaknesses is required and must be non-empty -- a fighting style with zero stated weaknesses "
            "is a design smell, not a real style");
    }

    auto checkUnit = [&errors](float value, const char* field) {
        if (value < 0.0f || value > 1.0f) {
            errors.push_back(std::string(field) + " must be within [0, 1], got " + std::to_string(value));
        }
    };
    checkUnit(g.aggression, "aggression");
    checkUnit(g.defense, "defense");
    checkUnit(g.mobility, "mobility");
    checkUnit(g.pressure, "pressure");
    checkUnit(g.deception, "deception");
    checkUnit(g.endurance, "endurance");
    checkUnit(g.precision, "precision");
    checkUnit(g.adaptability, "adaptability");
    checkUnit(g.evolution_capacity, "evolution_capacity");

    return errors;
}

}  // namespace

Result<CombatStyleGenome> CombatStyleGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<CombatStyleGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<CombatStyleGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    CombatStyleGenome g;
    g.style_name = StrOr(root, "style_name");
    g.ancestry = StrArrayOr(root, "ancestry");
    g.aggression = NumOr(root, "aggression", 0.5f);
    g.defense = NumOr(root, "defense", 0.5f);
    g.mobility = NumOr(root, "mobility", 0.5f);
    g.pressure = NumOr(root, "pressure", 0.5f);
    g.deception = NumOr(root, "deception", 0.5f);
    g.endurance = NumOr(root, "endurance", 0.5f);
    g.precision = NumOr(root, "precision", 0.5f);
    g.adaptability = NumOr(root, "adaptability", 0.5f);
    g.evolution_capacity = NumOr(root, "evolution_capacity", 0.5f);
    g.range_control = StrOr(root, "range_control");
    g.rhythm = StrOr(root, "rhythm");
    g.philosophy = StrOr(root, "philosophy");
    g.weaknesses = StrArrayOr(root, "weaknesses");

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined =
            "CombatStyleGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<CombatStyleGenome>::Fail(combined);
    }

    return Result<CombatStyleGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
