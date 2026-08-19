// CHARACTER/Genome/VisualStyleGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/VisualStyleGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class VisualStyleGenomeLoader {
public:
    static core::Result<VisualStyleGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
