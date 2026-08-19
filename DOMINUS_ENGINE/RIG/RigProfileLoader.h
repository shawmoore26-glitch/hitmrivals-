// RIG/RigProfileLoader.h
#pragma once

#include <filesystem>

#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>
#include "RIG/RigProfile.h"

namespace dominus::rig {

class RigProfileLoader {
public:
    static core::Result<RigProfile> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::rig
