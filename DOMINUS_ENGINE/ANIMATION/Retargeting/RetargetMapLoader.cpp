// ANIMATION/Retargeting/RetargetMapLoader.cpp
#include "ANIMATION/Retargeting/RetargetMapLoader.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::animation {

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
//   "bone_map": [
//     { "target_bone": "pelvis", "source_bone": "root" },
//     { "target_bone": "spine", "source_bone": "torso" }
//   ]
// }
Result<RetargetMap> RetargetMapLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<RetargetMap>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<RetargetMap>::Fail(std::string("Parse error: ") + e.what());
    }

    const Value* boneMapVal = root.Get("bone_map");
    if (!boneMapVal || !boneMapVal->IsArray()) {
        return Result<RetargetMap>::Fail("retarget map missing 'bone_map' array: " + path.string());
    }

    RetargetMap map;
    for (const Value& entry : boneMapVal->AsArray()) {
        auto* target = entry.Get("target_bone");
        auto* source = entry.Get("source_bone");
        if (!target || !source) {
            return Result<RetargetMap>::Fail("bone_map entry missing target_bone/source_bone: " + path.string());
        }
        map.AddMapping(target->AsString(), source->AsString());
    }

    return Result<RetargetMap>::Ok(std::move(map));
}

}  // namespace dominus::animation
