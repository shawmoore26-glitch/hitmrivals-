// COMBAT/Provenance/ImpactCanonicalSerializer.h
// Same job as REGISTRY::CanonicalSerializer -- a fixed-field-order,
// deterministic string from a struct, so identical values always
// produce identical bytes regardless of call-site formatting. Lives in
// COMBAT rather than REGISTRY for one specific, load-bearing reason:
// the Registry Prototype phase's own law is "REGISTRY depends on
// CHARACTER/Genome only -- never COMBAT/ANIMATION/WORLD/PHYSICS",
// verified by grep before every phase since. ImpactContext/ImpactResult
// are COMBAT types (COMBAT/PhysicsCombat/ImpactSolver.h) -- serializing
// them inside REGISTRY would silently violate that law. This file is
// the correct location, not a workaround.
#pragma once

#include <sstream>
#include <string>

#include "COMBAT/PhysicsCombat/ImpactSolver.h"

namespace dominus::combat {

class ImpactCanonicalSerializer {
public:
    static std::string SerializeContext(const ImpactContext& ctx) {
        std::ostringstream out;
        out << "attacker_mass_kg=" << ctx.attacker_mass_kg << ";attacker_velocity=" << ctx.attacker_velocity
            << ";attacker_strike_force_multiplier=" << ctx.attacker_strike_force_multiplier
            << ";attacker_aggression=" << ctx.attacker_aggression
            << ";defender_durability=" << ctx.defender_durability << ";defender_armor=" << ctx.defender_armor
            << ";defender_material_wear=" << ctx.defender_material_wear
            << ";struck_body_part=" << ctx.struck_body_part
            << ";defender_blocking=" << (ctx.defender_blocking ? "true" : "false")
            << ";defender_already_staggered=" << (ctx.defender_already_staggered ? "true" : "false")
            << ";defender_defense_bias=" << ctx.defender_defense_bias
            << ";design_risk_reward_balance=" << ctx.design_risk_reward_balance
            << ";impact_dir_x=" << ctx.impact_dir_x << ";visual_style_id=" << ctx.visual_style_id;
        return out.str();
    }

    static std::string SerializeResult(const ImpactResult& result) {
        std::ostringstream out;
        out << "force=" << result.force << ";resistance=" << result.resistance
            << ";difference=" << result.difference << ";severity=" << static_cast<int>(result.severity)
            << ";damage=" << result.damage
            << ";defender_blocking=" << (result.defender_blocking ? "true" : "false")
            << ";defender_already_staggered=" << (result.defender_already_staggered ? "true" : "false")
            << ";defender_defense_bias=" << result.defender_defense_bias
            << ";impact_dir_x=" << result.impact_dir_x << ";visual_style_id=" << result.visual_style_id;
        return out.str();
    }
};

}  // namespace dominus::combat
