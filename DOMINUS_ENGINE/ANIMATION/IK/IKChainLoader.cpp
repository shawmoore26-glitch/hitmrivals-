// ANIMATION/IK/IKChainLoader.cpp
#include "ANIMATION/IK/IKChainLoader.h"

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
// { "root_bone": "shoulder", "mid_bone": "elbow", "end_bone": "wrist", "bend_direction": 1.0 }
Result<IKChainDef> IKChainLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<IKChainDef>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<IKChainDef>::Fail(std::string("Parse error: ") + e.what());
    }

    IKChainDef chain;
    auto* rootBone = root.Get("root_bone");
    auto* midBone = root.Get("mid_bone");
    auto* endBone = root.Get("end_bone");
    if (!rootBone || !midBone || !endBone) {
        return Result<IKChainDef>::Fail("ik chain file missing root_bone/mid_bone/end_bone: " + path.string());
    }
    chain.root_bone = rootBone->AsString();
    chain.mid_bone = midBone->AsString();
    chain.end_bone = endBone->AsString();
    if (auto* bend = root.Get("bend_direction")) chain.bend_direction = static_cast<float>(bend->AsNumber());

    return Result<IKChainDef>::Ok(std::move(chain));
}

}  // namespace dominus::animation
