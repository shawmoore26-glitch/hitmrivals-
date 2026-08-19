// CHARACTER/Genome/CombatIdentityLoader.h
#pragma once

#include <filesystem>

#include "CHARACTER/Genome/CombatIdentity.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::character {

class CombatIdentityLoader {
public:
    static core::Result<CombatIdentity> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::character
