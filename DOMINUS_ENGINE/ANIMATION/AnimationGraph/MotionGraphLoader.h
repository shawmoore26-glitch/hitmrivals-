// ANIMATION/AnimationGraph/MotionGraphLoader.h
#pragma once

#include <filesystem>

#include "ANIMATION/AnimationGraph/MotionGraph.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::animation {

class MotionGraphLoader {
public:
    static core::Result<MotionGraph> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::animation
