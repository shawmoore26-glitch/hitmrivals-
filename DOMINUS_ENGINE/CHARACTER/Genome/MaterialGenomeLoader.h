// CHARACTER/Genome/MaterialGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/MaterialGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class MaterialGenomeLoader {
public:
    static core::Result<MaterialGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
