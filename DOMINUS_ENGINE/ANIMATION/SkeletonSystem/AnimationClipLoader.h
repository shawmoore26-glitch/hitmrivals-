// ANIMATION/SkeletonSystem/AnimationClipLoader.h
#pragma once

#include <filesystem>

#include "ANIMATION/SkeletonSystem/AnimationClip.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::animation {

class AnimationClipLoader {
public:
    static core::Result<AnimationClip> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::animation
