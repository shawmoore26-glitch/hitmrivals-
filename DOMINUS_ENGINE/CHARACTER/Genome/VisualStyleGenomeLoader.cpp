// CHARACTER/Genome/VisualStyleGenomeLoader.cpp
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"

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

std::vector<std::string> Validate(const VisualStyleGenome& g) {
    std::vector<std::string> errors;
    if (g.style_id.empty()) {
        errors.push_back("style_id is required and must be non-empty");
    }
    if (g.name.empty()) {
        errors.push_back("name is required and must be non-empty");
    }
    return errors;
}

}  // namespace

Result<VisualStyleGenome> VisualStyleGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<VisualStyleGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<VisualStyleGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    VisualStyleGenome g;
    g.style_id = StrOr(root, "style_id");
    g.name = StrOr(root, "name");
    if (auto* rules = root.Get("visual_rules")) {
        g.visual_rules.line_quality = StrOr(*rules, "line_quality");
        g.visual_rules.color_behavior = StrOr(*rules, "color_behavior");
        g.visual_rules.shape_behavior = StrOr(*rules, "shape_behavior");
        g.visual_rules.motion_behavior = StrOr(*rules, "motion_behavior");
    }
    g.influences = StrArrayOr(root, "influences");

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined =
            "VisualStyleGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<VisualStyleGenome>::Fail(combined);
    }

    return Result<VisualStyleGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
