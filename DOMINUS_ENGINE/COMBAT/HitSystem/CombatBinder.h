// COMBAT/HitSystem/CombatBinder.h
// LAW C002: combat identity/moves/hurtboxes load through Meta-Bin refs,
// same pattern as CHARACTER/Rig/RigBinder for skeleton/animation. Run this
// AFTER RigBinder::Bind (needs the object already loaded via
// DominusSerializer; doesn't require RigBinder's components itself, but
// callers typically bind both).
#pragma once

#include <filesystem>

#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"  // for VoidResult

namespace dominus::combat {

class CombatBinder {
public:
    static core::VoidResult Bind(core::MetaBinObject& obj, const std::filesystem::path& baseDir);
};

}  // namespace dominus::combat
