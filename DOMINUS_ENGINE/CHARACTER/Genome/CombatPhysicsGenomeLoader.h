// CHARACTER/Genome/CombatPhysicsGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/CombatPhysicsGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class CombatPhysicsGenomeLoader {
public:
    static core::Result<CombatPhysicsGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
