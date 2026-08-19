// COMBAT/HitSystem/MoveLoader.h
#pragma once

#include <filesystem>

#include "COMBAT/HitSystem/MoveDef.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>

namespace dominus::combat {

class MoveLoader {
public:
    static core::Result<MoveDef> LoadFromFile(const std::filesystem::path& path);
};

}  // namespace dominus::combat
