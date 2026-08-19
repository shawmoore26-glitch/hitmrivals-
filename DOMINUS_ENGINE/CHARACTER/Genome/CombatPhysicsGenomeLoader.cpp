// CHARACTER/Genome/CombatPhysicsGenomeLoader.cpp
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"

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

std::vector<std::string> Validate(const CombatPhysicsGenome& g) {
    std::vector<std::string> errors;

    if (g.body.mass_kg <= 0.0f) errors.push_back("body.mass_kg must be > 0");
    if (g.body.height_m <= 0.0f) errors.push_back("body.height_m must be > 0");
    if (g.body.density <= 0.0f) errors.push_back("body.density must be > 0");
    if (g.body.armor < 0.0f || g.body.armor > 1.0f) {
        errors.push_back("body.armor must be within [0, 1], got " + std::to_string(g.body.armor));
    }
    if (g.body.flexibility < 0.0f || g.body.flexibility > 1.0f) {
        errors.push_back("body.flexibility must be within [0, 1], got " + std::to_string(g.body.flexibility));
    }

    if (g.energy.stamina <= 0.0f) errors.push_back("energy.stamina must be > 0");
    if (g.energy.recovery_rate < 0.0f) errors.push_back("energy.recovery_rate must be >= 0");
    if (g.energy.fatigue_rate <= 0.0f) errors.push_back("energy.fatigue_rate must be > 0");

    if (g.impact.strike_force <= 0.0f) errors.push_back("impact.strike_force must be > 0");
    if (g.impact.grapple_force <= 0.0f) errors.push_back("impact.grapple_force must be > 0");
    if (g.impact.durability <= 0.0f) errors.push_back("impact.durability must be > 0");

    return errors;
}

}  // namespace

Result<CombatPhysicsGenome> CombatPhysicsGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<CombatPhysicsGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<CombatPhysicsGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    CombatPhysicsGenome g;
    if (auto* body = root.Get("body_model")) {
        g.body.mass_kg = NumOr(*body, "mass", 70.0f);
        g.body.height_m = NumOr(*body, "height", 1.8f);
        g.body.density = NumOr(*body, "density", 1.0f);
        g.body.armor = NumOr(*body, "armor", 0.0f);
        g.body.flexibility = NumOr(*body, "flexibility", 0.5f);
    }
    if (auto* energy = root.Get("energy_model")) {
        g.energy.stamina = NumOr(*energy, "stamina", 100.0f);
        g.energy.recovery_rate = NumOr(*energy, "recovery", 1.0f);
        g.energy.fatigue_rate = NumOr(*energy, "fatigue_rate", 1.0f);
    }
    if (auto* impact = root.Get("impact_profile")) {
        g.impact.strike_force = NumOr(*impact, "strike_force", 1.0f);
        g.impact.grapple_force = NumOr(*impact, "grapple_force", 1.0f);
        g.impact.durability = NumOr(*impact, "durability", 50.0f);
    }

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined =
            "CombatPhysicsGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<CombatPhysicsGenome>::Fail(combined);
    }

    return Result<CombatPhysicsGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
