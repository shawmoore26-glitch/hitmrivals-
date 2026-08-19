// ANIMATION/Retargeting/RetargetMapLoader.h
#pragma once

#include <filesystem>

#include "ANIMATION/Retargeting/RetargetMap.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::animation {

class RetargetMapLoader {
public:
    static core::Result<RetargetMap> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::animation
