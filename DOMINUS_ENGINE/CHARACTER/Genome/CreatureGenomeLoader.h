// CHARACTER/Genome/CreatureGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/CreatureGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class CreatureGenomeLoader {
public:
    static core::Result<CreatureGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
