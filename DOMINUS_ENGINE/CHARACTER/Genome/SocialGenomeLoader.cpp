// CHARACTER/Genome/SocialGenomeLoader.cpp
#include "CHARACTER/Genome/SocialGenomeLoader.h"

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
}  // namespace

Result<SocialGenome> SocialGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<SocialGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<SocialGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    SocialGenome genome;
    if (auto* personality = root.Get("personality")) {
        genome.personality.trust = NumOr(*personality, "trust", 0.5f);
        genome.personality.aggression = NumOr(*personality, "aggression", 0.5f);
        genome.personality.loyalty = NumOr(*personality, "loyalty", 0.5f);
    }

    if (auto* relationships = root.Get("relationships")) {
        if (relationships->IsArray()) {
            for (const Value& entry : relationships->AsArray()) {
                Relationship rel;
                if (auto* entity = entry.Get("entity")) {
                    if (entity->IsString()) rel.entity_id = entity->AsString();
                }
                if (auto* relation = entry.Get("relation")) {
                    if (relation->IsString()) rel.relation = relation->AsString();
                }
                rel.strength = NumOr(entry, "strength", 0.0f);
                if (!rel.entity_id.empty()) genome.relationships.push_back(std::move(rel));
            }
        }
    }

    return Result<SocialGenome>::Ok(std::move(genome));
}

}  // namespace dominus::character
