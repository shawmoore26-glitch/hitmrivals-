// CHARACTER/Genome/CreatureGenomeLoader.cpp
#include "CHARACTER/Genome/CreatureGenomeLoader.h"

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

int IntOr(const Value& root, const char* key, int def) {
    auto* v = root.Get(key);
    return v && v->IsNumber() ? static_cast<int>(v->AsNumber()) : def;
}

bool BoolOr(const Value& root, const char* key, bool def) {
    auto* v = root.Get(key);
    return v && v->IsBool() ? v->AsBool() : def;
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

IntelligenceTier TierFromString(const std::string& s) {
    if (s == "instinct") return IntelligenceTier::kInstinct;
    if (s == "animal") return IntelligenceTier::kAnimal;
    if (s == "pack") return IntelligenceTier::kPack;
    if (s == "problem_solving") return IntelligenceTier::kProblemSolving;
    if (s == "human_level") return IntelligenceTier::kHumanLevel;
    if (s == "superhuman") return IntelligenceTier::kSuperhuman;
    if (s == "cosmic") return IntelligenceTier::kCosmic;
    return IntelligenceTier::kAnimal;  // default -- unrecognized tier strings fall back, not reject
}

// Strict validation, per the explicit directive -- collects EVERY
// violation rather than stopping at the first, same philosophy as
// PackageValidator. Ranges are checked because they carry real meaning
// (a "0-1 weight" that's actually 47.0 is a genuine authoring error, not
// creative expression -- free-form string fields are never range-
// checked, only genuinely numeric/bounded ones).
std::vector<std::string> Validate(const CreatureGenome& g) {
    std::vector<std::string> errors;

    if (g.identity.species_name.empty()) {
        errors.push_back("identity.species_name is required and must be non-empty");
    }
    if (g.taxonomy.limb_count < 0) {
        errors.push_back("taxonomy.limb_count must be >= 0");
    }
    if (g.anatomy.height_m <= 0.0f) {
        errors.push_back("anatomy.height_m must be > 0");
    }
    if (g.anatomy.weight_kg <= 0.0f) {
        errors.push_back("anatomy.weight_kg must be > 0");
    }

    auto checkUnit = [&errors](float value, const char* field) {
        if (value < 0.0f || value > 1.0f) {
            errors.push_back(std::string(field) + " must be within [0, 1], got " + std::to_string(value));
        }
    };
    checkUnit(g.physiology.mutation_rate, "physiology.mutation_rate");
    checkUnit(g.cognition.problem_solving, "cognition.problem_solving");
    checkUnit(g.cognition.communication, "cognition.communication");
    checkUnit(g.behavior.aggression, "behavior.aggression");
    checkUnit(g.behavior.territoriality, "behavior.territoriality");
    checkUnit(g.combat.aggression, "combat.aggression");
    checkUnit(g.combat.speed, "combat.speed");
    checkUnit(g.combat.durability, "combat.durability");
    checkUnit(g.combat.combat_intelligence, "combat.combat_intelligence");
    checkUnit(g.evolution.adaptation_potential, "evolution.adaptation_potential");

    if (g.growth.lifespan_years <= 0.0f) {
        errors.push_back("growth.lifespan_years must be > 0");
    }
    if (g.growth.maturity_age_years < 0.0f) {
        errors.push_back("growth.maturity_age_years must be >= 0");
    }

    return errors;
}

}  // namespace

Result<CreatureGenome> CreatureGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<CreatureGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<CreatureGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    CreatureGenome g;

    if (auto* identity = root.Get("identity")) {
        g.identity.species_name = StrOr(*identity, "species_name");
        g.identity.common_name = StrOr(*identity, "common_name");
        g.identity.origin = StrOr(*identity, "origin");
    }

    if (auto* taxonomy = root.Get("taxonomy")) {
        g.taxonomy.body_plan = StrOr(*taxonomy, "body_plan");
        g.taxonomy.classification = StrOr(*taxonomy, "classification");
        g.taxonomy.limb_count = IntOr(*taxonomy, "limb_count", 4);
    }

    if (auto* anatomy = root.Get("anatomy")) {
        g.anatomy.height_m = NumOr(*anatomy, "height_m", 1.0f);
        g.anatomy.weight_kg = NumOr(*anatomy, "weight_kg", 50.0f);
        g.anatomy.armor_type = StrOr(*anatomy, "armor_type");
        g.anatomy.notable_features = StrArrayOr(*anatomy, "notable_features");
        g.anatomy.wing_area_m2 = NumOr(*anatomy, "wing_area_m2", 0.0f);
    }

    if (auto* physiology = root.Get("physiology")) {
        g.physiology.metabolism = StrOr(*physiology, "metabolism");
        g.physiology.energy_sources = StrArrayOr(*physiology, "energy_sources");
        g.physiology.reproduction = StrOr(*physiology, "reproduction");
        g.physiology.mutation_rate = NumOr(*physiology, "mutation_rate", 0.1f);
        g.physiology.requires_water_respiration = BoolOr(*physiology, "requires_water_respiration", false);
    }

    if (auto* senses = root.Get("senses")) {
        g.senses.sense_types = StrArrayOr(*senses, "sense_types");
        g.senses.perception_range_m = NumOr(*senses, "perception_range_m", 50.0f);
    }

    if (auto* locomotion = root.Get("locomotion")) {
        g.locomotion.primary_mode = StrOr(*locomotion, "primary_mode");
        g.locomotion.top_speed_kmh = NumOr(*locomotion, "top_speed_kmh", 40.0f);
    }

    if (auto* cognition = root.Get("cognition")) {
        g.cognition.tier = TierFromString(StrOr(*cognition, "tier", "animal"));
        g.cognition.problem_solving = NumOr(*cognition, "problem_solving", 0.3f);
        g.cognition.communication = NumOr(*cognition, "communication", 0.2f);
    }

    if (auto* behavior = root.Get("behavior")) {
        g.behavior.temperament = StrOr(*behavior, "temperament");
        g.behavior.aggression = NumOr(*behavior, "aggression", 0.5f);
        g.behavior.territoriality = NumOr(*behavior, "territoriality", 0.5f);
        g.behavior.combat_role = StrOr(*behavior, "combat_role");
    }

    if (auto* ecology = root.Get("ecology")) {
        g.ecology.trophic_role = StrOr(*ecology, "trophic_role");
        g.ecology.diet_type = StrOr(*ecology, "diet_type");
        g.ecology.preferred_prey = StrArrayOr(*ecology, "preferred_prey");
        g.ecology.natural_predators = StrArrayOr(*ecology, "natural_predators");
        g.ecology.habitat = StrOr(*ecology, "habitat");
    }

    if (auto* combat = root.Get("combat_profile")) {
        g.combat.aggression = NumOr(*combat, "aggression", 0.5f);
        g.combat.speed = NumOr(*combat, "speed", 0.5f);
        g.combat.durability = NumOr(*combat, "durability", 0.5f);
        g.combat.combat_intelligence = NumOr(*combat, "combat_intelligence", 0.5f);
        g.combat.range = StrOr(*combat, "range");
    }

    if (auto* growth = root.Get("growth")) {
        g.growth.lifespan_years = NumOr(*growth, "lifespan_years", 10.0f);
        g.growth.life_stages = StrArrayOr(*growth, "life_stages");
        g.growth.maturity_age_years = NumOr(*growth, "maturity_age_years", 2.0f);
    }

    if (auto* evolution = root.Get("evolution")) {
        g.evolution.evolutionary_origin = StrOr(*evolution, "evolutionary_origin");
        g.evolution.lineage = StrArrayOr(*evolution, "lineage");
        g.evolution.adaptation_potential = NumOr(*evolution, "adaptation_potential", 0.5f);
    }

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined = "CreatureGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<CreatureGenome>::Fail(combined);
    }

    return Result<CreatureGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
