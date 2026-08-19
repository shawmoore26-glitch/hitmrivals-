// COMBAT/HitSystem/HurtboxLoader.h
#pragma once

#include <filesystem>

#include "COMBAT/HitSystem/Hurtbox.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::combat {

class HurtboxLoader {
public:
    static core::Result<HurtboxSet> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::combat
