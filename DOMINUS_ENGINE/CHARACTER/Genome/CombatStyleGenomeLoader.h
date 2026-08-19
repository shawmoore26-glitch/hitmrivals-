// CHARACTER/Genome/CombatStyleGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/CombatStyleGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class CombatStyleGenomeLoader {
public:
    static core::Result<CombatStyleGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
