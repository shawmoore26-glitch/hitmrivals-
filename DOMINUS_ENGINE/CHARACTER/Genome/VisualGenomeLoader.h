// CHARACTER/Genome/VisualGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/VisualGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class VisualGenomeLoader {
public:
    static core::Result<VisualGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
