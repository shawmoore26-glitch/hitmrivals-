// CHARACTER/Genome/GameDesignGenomeLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/GameDesignGenome.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class GameDesignGenomeLoader {
public:
    static core::Result<GameDesignGenome> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
