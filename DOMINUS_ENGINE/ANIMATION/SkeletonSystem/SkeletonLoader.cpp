// ANIMATION/SkeletonSystem/SkeletonLoader.cpp
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"

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
//   "bones": [
//     { "name": "root", "parent": null, "x":0, "y":0, "rotation":0, "scale_x":1, "scale_y":1 },
//     { "name": "torso", "parent": "root", "x":0, "y":40, "rotation":0, "scale_x":1, "scale_y":1 }
//   ]
// }
Result<Skeleton> SkeletonLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<Skeleton>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<Skeleton>::Fail(std::string("Parse error: ") + e.what());
    }

    const Value* bonesVal = root.Get("bones");
    if (!bonesVal || !bonesVal->IsArray()) {
        return Result<Skeleton>::Fail("skeleton file missing 'bones' array: " + path.string());
    }

    Skeleton skeleton;
    for (const Value& boneVal : bonesVal->AsArray()) {
        const Value* nameVal = boneVal.Get("name");
        if (!nameVal) return Result<Skeleton>::Fail("bone missing 'name' in " + path.string());

        Bone bone;
        bone.name = nameVal->AsString();

        // A duplicate bone name is a real, silent-corruption risk: Skeleton::
        // AddBone's own name->index map (nameToIndex_) simply overwrites on a
        // repeat name, meaning any earlier bone with that name becomes
        // permanently unaddressable by name (FindBoneIndex would resolve to
        // the LATER bone instead) while still occupying a slot in the bone
        // array -- exactly the "N entries but fewer unique bones" failure
        // mode a rig authority is supposed to make structurally impossible.
        // Rejected at load time, not discovered later as a mysterious
        // missing hurtbox/hitbox/IK-target binding.
        if (skeleton.FindBoneIndex(bone.name)) {
            return Result<Skeleton>::Fail("duplicate bone name '" + bone.name + "' in " + path.string() +
                                           " -- every bone name must be unique");
        }

        const Value* parentVal = boneVal.Get("parent");
        if (parentVal && parentVal->IsString()) {
            auto parentIdx = skeleton.FindBoneIndex(parentVal->AsString());
            if (!parentIdx) {
                return Result<Skeleton>::Fail("bone '" + bone.name + "' references unknown parent '" +
                                               parentVal->AsString() + "' -- parents must be defined earlier in the array");
            }
            bone.parent_index = *parentIdx;
        }

        auto numOr = [&](const char* key, float def) {
            const Value* v = boneVal.Get(key);
            return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : def;
        };
        bone.bind_pose_local.x = numOr("x", 0.0f);
        bone.bind_pose_local.y = numOr("y", 0.0f);
        bone.bind_pose_local.rotation_deg = numOr("rotation", 0.0f);
        bone.bind_pose_local.scale_x = numOr("scale_x", 1.0f);
        bone.bind_pose_local.scale_y = numOr("scale_y", 1.0f);

        skeleton.AddBone(std::move(bone));
    }

    return Result<Skeleton>::Ok(std::move(skeleton));
}

}  // namespace dominus::animation
