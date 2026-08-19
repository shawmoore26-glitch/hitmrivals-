// COMBAT/HitSystem/CombatComponents.h
// Components attached to a MetaBinObject by CombatBinder. Lives in COMBAT
// (not CHARACTER) because MoveDef/HurtboxSet are COMBAT-owned types --
// CHARACTER's RigBinder never depends on COMBAT headers, only the reverse.
#pragma once

#include <string>
#include <unordered_map>

#include "CHARACTER/Genome/CombatIdentity.h"
#include "COMBAT/HitSystem/Hurtbox.h"
#include "COMBAT/HitSystem/MoveDef.h"

namespace dominus::combat {

struct CombatIdentityComponent {
    character::CombatIdentity identity;
};

struct MoveSetComponent {
    std::unordered_map<std::string, MoveDef> moves;

    const MoveDef* Find(const std::string& name) const {
        auto it = moves.find(name);
        return it == moves.end() ? nullptr : &it->second;
    }
};

struct HurtboxSetComponent {
    HurtboxSet hurtboxes;
};

}  // namespace dominus::combat
