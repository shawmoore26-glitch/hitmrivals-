// ANIMATION/SkeletonSystem/AnimationClipLoader.cpp
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"

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
//   "name": "idle", "duration": 1.0, "loop": true,
//   "tracks": [
//     { "bone": "torso", "keys": [
//         { "time": 0.0, "y": 40, "rotation": 0 },
//         { "time": 0.5, "y": 42, "rotation": 1 },
//         { "time": 1.0, "y": 40, "rotation": 0 }
//     ] }
//   ]
// }
Result<AnimationClip> AnimationClipLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<AnimationClip>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<AnimationClip>::Fail(std::string("Parse error: ") + e.what());
    }

    AnimationClip clip;
    if (auto* name = root.Get("name")) clip.name = name->AsString();
    if (auto* dur = root.Get("duration")) clip.duration = static_cast<float>(dur->AsNumber());
    if (auto* loop = root.Get("loop")) {
        if (loop->IsBool()) clip.loop = loop->AsBool();
    }

    const Value* tracksVal = root.Get("tracks");
    if (!tracksVal || !tracksVal->IsArray()) {
        return Result<AnimationClip>::Fail("clip file missing 'tracks' array: " + path.string());
    }

    for (const Value& trackVal : tracksVal->AsArray()) {
        const Value* boneVal = trackVal.Get("bone");
        const Value* keysVal = trackVal.Get("keys");
        if (!boneVal || !keysVal || !keysVal->IsArray()) {
            return Result<AnimationClip>::Fail("malformed track in " + path.string());
        }

        BoneTrack track;
        track.bone_name = boneVal->AsString();

        for (const Value& keyVal : keysVal->AsArray()) {
            Keyframe key;
            if (auto* t = keyVal.Get("time")) key.time = static_cast<float>(t->AsNumber());

            auto numOr = [&](const char* k, float def) {
                const Value* v = keyVal.Get(k);
                return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : def;
            };
            key.pose.x = numOr("x", 0.0f);
            key.pose.y = numOr("y", 0.0f);
            key.pose.rotation_deg = numOr("rotation", 0.0f);
            key.pose.scale_x = numOr("scale_x", 1.0f);
            key.pose.scale_y = numOr("scale_y", 1.0f);

            track.keyframes.push_back(key);
        }

        clip.AddTrack(std::move(track));
    }

    return Result<AnimationClip>::Ok(std::move(clip));
}

}  // namespace dominus::animation
