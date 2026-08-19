// ANIMATION/IK/IKChainLoader.h
#pragma once

#include <filesystem>

#include "ANIMATION/IK/IKChain.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::animation {

class IKChainLoader {
public:
    static core::Result<IKChainDef> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::animation
