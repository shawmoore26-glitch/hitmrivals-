// CHARACTER/HitmBridge/HitmIdentityImporter.cpp
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::character::hitm {

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

// Required top-level keys per identity file, evidenced by diffing the key
// sets of all three real HITM Rivals fighters' authored files (brooklyn,
// rocket, static) -- every key listed here is present, verbatim, in all
// three. Keys present in only some fighters (e.g. brooklyn's combat_genome
// "read_engine", rocket's "archetype_line"/"forbidden") are intentionally
// left optional: requiring them would reject real fighters that legitimately
// don't have that field, which is not this importer's call to make.
const std::vector<std::string>& RequiredKeys(const std::string& filename) {
    static const std::vector<std::string> kCharacterDna = {
        "character", "dna", "categorical", "behaviour", "motion", "verbs",
        "ai", "audio", "camera", "emotionalIntent", "frames", "vfx"};
    static const std::vector<std::string> kCombatGenome = {
        "_note", "ai_intent", "archetype", "combat_verbs", "defense_profile",
        "frame_dna", "impact_profile", "philosophy", "pressure_profile",
        "range_profile", "recovery_profile", "rhythm_profile", "risk_profile",
        "weight_profile"};
    static const std::vector<std::string> kDesign = {
        "_note", "character", "core_parts", "drawOrder", "features",
        "handBone", "handPoint"};
    static const std::vector<std::string> kIdentity = {
        "_note", "accent", "faction", "id", "name", "palettes", "sprite", "vfx"};
    static const std::vector<std::string> kSignature = {
        "_note", "chains", "moves", "projectiles"};
    static const std::vector<std::string> kEmpty = {};

    if (filename == "character_dna.json") return kCharacterDna;
    if (filename == "combat_genome.json") return kCombatGenome;
    if (filename == "design.json") return kDesign;
    if (filename == "identity.json") return kIdentity;
    if (filename == "signature.json") return kSignature;
    return kEmpty;
}

// Reads, parses, and validates one identity file. Throws on any failure
// (missing file, malformed JSON, missing required key) -- callers convert
// to a Result::Fail with full context.
Value LoadAndValidate(const std::filesystem::path& dir, const std::string& filename) {
    std::filesystem::path path = dir / filename;
    std::string text = ReadFile(path);  // throws with the path on failure

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        throw std::runtime_error(path.string() + ": parse error: " + e.what());
    }

    if (!root.IsObject()) {
        throw std::runtime_error(path.string() + ": expected a JSON object at the top level");
    }

    for (const auto& key : RequiredKeys(filename)) {
        if (!root.Has(key)) {
            throw std::runtime_error(path.string() + ": missing required key '" + key + "'");
        }
    }
    return root;
}

}  // namespace

Result<HitmIdentityRecord> HitmIdentityImporter::Import(const std::filesystem::path& identityDir) {
    if (!std::filesystem::exists(identityDir) || !std::filesystem::is_directory(identityDir)) {
        return Result<HitmIdentityRecord>::Fail("Identity directory does not exist: " + identityDir.string());
    }

    HitmIdentityRecord record;
    record.fighter_id = identityDir.filename().string();

    try {
        record.character_dna = LoadAndValidate(identityDir, "character_dna.json");
        record.combat_genome = LoadAndValidate(identityDir, "combat_genome.json");
        record.design = LoadAndValidate(identityDir, "design.json");
        record.identity = LoadAndValidate(identityDir, "identity.json");
        record.signature = LoadAndValidate(identityDir, "signature.json");
    } catch (const std::exception& e) {
        return Result<HitmIdentityRecord>::Fail(e.what());
    }

    // Cross-check: identity.json's "id" must match the directory this data
    // was loaded from. A mismatch here means a copy/rename mistake -- the
    // file loaded successfully but is quietly the wrong fighter's data,
    // which is exactly the kind of silent-wrongness this project refuses.
    const Value* idField = record.identity.Get("id");
    if (!idField || !idField->IsString()) {
        return Result<HitmIdentityRecord>::Fail(identityDir.string() + "/identity.json: 'id' field is not a string");
    }
    if (idField->AsString() != record.fighter_id) {
        return Result<HitmIdentityRecord>::Fail(
            "identity.json 'id' (" + idField->AsString() + ") does not match directory name (" +
            record.fighter_id + ") for " + identityDir.string());
    }

    return Result<HitmIdentityRecord>::Ok(std::move(record));
}

}  // namespace dominus::character::hitm
