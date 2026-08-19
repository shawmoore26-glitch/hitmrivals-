// ANIMATION/SkeletonSystem/SkeletonLoader.h
// Loads a .skel.json file (see docs/ARCHITECTURE_v0.1.md Phase 2 addendum
// for the format) into a runtime Skeleton. Reuses CORE's MiniJson rather
// than inventing a second parser -- ANIMATION is allowed to depend on
// CORE, just not the reverse.
#pragma once

#include <filesystem>

#include "ANIMATION/SkeletonSystem/Skeleton.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::animation {

class SkeletonLoader {
public:
    static core::Result<Skeleton> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::animation
