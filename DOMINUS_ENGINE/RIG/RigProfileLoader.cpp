// RIG/RigProfileLoader.cpp
#include "RIG/RigProfileLoader.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::rig {

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
}  // namespace

// Expected format:
// {
//   "profile_id": "brooklyn_legacy_v1",
//   "entity_id": "brooklyn",
//   "mappings": [
//     { "legacy_bone": "root", "canonical_bone": "root" },
//     { "legacy_bone": "torso", "canonical_bone": "chest" }
//   ]
// }
Result<RigProfile> RigProfileLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<RigProfile>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<RigProfile>::Fail(std::string("Parse error: ") + e.what());
    }

    RigProfile profile;
    if (const Value* v = root.Get("profile_id")) profile.profile_id = v->AsString();
    if (const Value* v = root.Get("entity_id")) profile.entity_id = v->AsString();

    const Value* mappingsVal = root.Get("mappings");
    if (!mappingsVal || !mappingsVal->IsArray()) {
        return Result<RigProfile>::Fail("rig profile missing 'mappings' array: " + path.string());
    }

    for (const Value& entry : mappingsVal->AsArray()) {
        auto* legacy = entry.Get("legacy_bone");
        auto* canonical = entry.Get("canonical_bone");
        if (!legacy || !canonical) {
            return Result<RigProfile>::Fail("mappings entry missing legacy_bone/canonical_bone: " + path.string());
        }
        profile.mappings.push_back(RigBoneMapping{legacy->AsString(), canonical->AsString()});
    }

    return Result<RigProfile>::Ok(std::move(profile));
}

}  // namespace dominus::rig
