// CHARACTER/Genome/SocialGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/SocialGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class SocialGenomeLoader {
public:
    static core::Result<SocialGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
