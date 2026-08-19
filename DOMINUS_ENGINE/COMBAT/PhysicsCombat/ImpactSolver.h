// COMBAT/PhysicsCombat/ImpactSolver.h
// Real math, not narration: F = m*v is genuinely how this computes
// force (a standard, honest simplification -- not claiming full rigid-
// body dynamics), impact severity is a real threshold comparison against
// a defender's durability, and body-part damage multipliers are a real
// lookup table. Every number this produces is derived from its inputs;
// nothing here is invented to look like output.
//
// Phase 2 (MASTER OF COMBAT Phase 2) shipped this standalone, explicitly
// not wired into COMBAT::ReactionSystem/CombatController. Phase 4.1.5
// closes that gap with ONE new data flow (Solve(ImpactContext) ->
// ImpactResult, consumed by ReactionSystem::Apply) without touching any
// of the original static methods below -- CalculateForce/ResolveImpact/
// ApplyArmor/CalculateDamage remain byte-identical to Phase 2, still
// directly callable, still covered by the original Phase 2 tests.
// Solve() is a NEW composition built on top of them, not a replacement.
//
// ImpactSolver itself stays a pure calculation module with zero
// dependency on CORE/CHARACTER/ANIMATION -- gathering genome data and
// constructing an ImpactContext is CombatController's job (see
// COMBAT/CombatController.h), not this file's.
#pragma once

#include <string>
#include <unordered_map>

namespace dominus::combat {

enum class ImpactSeverity { kLow, kModerate, kHigh };

struct ImpactResult {
    // --- Original Phase 2 fields (MASTER OF COMBAT Phase 2) -- untouched,
    // still produced identically by ResolveImpact() below. ---
    float force = 0.0f;
    float resistance = 0.0f;
    float difference = 0.0f;
    ImpactSeverity severity = ImpactSeverity::kLow;

    // --- New Phase 4.1.5 fields -- only populated by Solve() below.
    // ResolveImpact() leaves these at their neutral defaults, same
    // "additive, zero regression" discipline as every prior phase. ---

    // Final damage after style/armor/material/body-part/design
    // multipliers are all applied -- the one number ReactionSystem::Apply
    // actually needs.
    float damage = 0.0f;

    // Echoed forward from ImpactContext, not computed here -- Solve()
    // is a pure calculator, it never decides a reaction (LAW: no
    // side effects, no reaction logic). ReactionSystem::Apply reads
    // these back out to build the exact same ReactionInput the
    // pre-existing Determine() has always taken.
    bool defender_blocking = false;
    bool defender_already_staggered = false;
    float defender_defense_bias = 0.5f;
    float impact_dir_x = 1.0f;

    // Inert presentation data -- same honesty as CinematicDirector's
    // camera/slowmo fields: real data, carried through, with no
    // renderer to consume it yet (GRAPHICS remains an empty
    // placeholder). Not computed, not interpreted -- just passed along.
    std::string visual_style_id;
};

// Phase 4.1.5's "New Runtime Contract" -- the pure input to Solve().
// Every field has a neutral default chosen so that leaving a field
// unset produces EXACTLY the same output Phase 2's manual
// ResolveImpact->ApplyArmor->CalculateDamage chain already produced --
// verified against Brooklyn's real worked example (720 force / 648
// post-armor / 648 torso damage) by
// ImpactSolver_Solve_DefaultGenomeValuesMatchPhase2Baseline.
struct ImpactContext {
    // Attacker
    float attacker_mass_kg = 70.0f;
    float attacker_velocity = 0.0f;
    float attacker_strike_force_multiplier = 1.0f;  // CombatPhysicsGenome.impact.strike_force
    float attacker_aggression = 0.5f;                // CombatStyleGenome.aggression, 0.5 = neutral

    // Defender
    float defender_durability = 50.0f;   // CombatPhysicsGenome.impact.durability
    float defender_armor = 0.0f;         // CombatPhysicsGenome.body.armor, 0-1
    float defender_material_wear = 0.0f; // MaterialGenome.properties.wear_state, 0-1 -- a worn
                                          // material protects less (a simple, clearly-labeled
                                          // heuristic, same discipline as MaterialWearDeriver's
                                          // own 0.05/event rule -- not a claim about real
                                          // material science)
    std::string struck_body_part = "torso";
    bool defender_blocking = false;
    bool defender_already_staggered = false;
    float defender_defense_bias = 0.5f;  // same neutral-baseline meaning as ReactionInput's field

    // Design tuning -- GameDesignGenome.risk_reward_balance, 0.5 = neutral.
    // "Balance values / tuning multipliers" from the Genome Integration
    // spec, applied as a single final damage multiplier -- not a claim
    // that risk/reward balance is a solved design problem, just the one
    // concrete, defensible hook available from the genome as it exists
    // today.
    float design_risk_reward_balance = 0.5f;

    // Situational
    float impact_dir_x = 1.0f;

    // Presentation -- VisualStyleGenome.style_id, passed through inert
    // (see ImpactResult::visual_style_id above).
    std::string visual_style_id;
};

class ImpactSolver {
public:
    // F = m*v -- the standard simplified force model, not full rigid-body
    // dynamics. Honest about what it is: a real, useful approximation,
    // not a physics engine.
    static float CalculateForce(float mass, float velocity) { return mass * velocity; }

    // Severity thresholds (>100 difference = high, >0 = moderate, else
    // low) are a deliberately simple heuristic, same honest labeling as
    // CreatureGenomeSemanticValidator's wing-loading threshold -- not a
    // claim of precisely-tuned game balance.
    static ImpactResult ResolveImpact(float attackerMass, float attackVelocity, float defenderDurability) {
        ImpactResult result;
        result.force = CalculateForce(attackerMass, attackVelocity);
        result.resistance = defenderDurability;
        result.difference = result.force - defenderDurability;

        if (result.difference > 100.0f) {
            result.severity = ImpactSeverity::kHigh;
        } else if (result.difference > 0.0f) {
            result.severity = ImpactSeverity::kModerate;
        } else {
            result.severity = ImpactSeverity::kLow;
        }
        return result;
    }

    // Armor reduces force BEFORE the body-part multiplier is applied --
    // armor is a property of the whole body (CombatPhysicsGenome::body),
    // multipliers are per-struck-location.
    static float ApplyArmor(float force, float armorFraction) {
        float clamped = armorFraction < 0.0f ? 0.0f : (armorFraction > 1.0f ? 1.0f : armorFraction);
        return force * (1.0f - clamped);
    }

    // Real lookup table, not invented per-call. Unknown body parts get a
    // neutral 1.0 multiplier rather than being rejected -- this is a
    // damage calculator, not a validator; an unrecognized location
    // shouldn't silently zero out or inflate damage.
    static float CalculateDamage(float postArmorForce, const std::string& bodyPart) {
        static const std::unordered_map<std::string, float> kMultipliers = {
            {"head", 2.0f}, {"torso", 1.0f}, {"arm", 0.6f}, {"leg", 0.8f}};
        auto it = kMultipliers.find(bodyPart);
        float multiplier = it != kMultipliers.end() ? it->second : 1.0f;
        return postArmorForce * multiplier;
    }

    // Phase 4.1.5's one new data flow: CombatController -> ImpactContext
    // -> Solve() -> ImpactResult -> ReactionSystem::Apply(). Pure
    // calculation only -- composes the exact same building blocks above
    // (CalculateForce/ApplyArmor/CalculateDamage), reads no ECS state,
    // writes no ECS state, triggers no animation, changes no health.
    // Every genome-driven multiplier is built so its NEUTRAL default
    // reduces to an exact 1.0x, which is what
    // ImpactSolver_Solve_DefaultGenomeValuesMatchPhase2Baseline actually
    // verifies -- not asserted, checked against the same real numbers
    // (720 / 648 / 648) the Phase 2 roadmap entry printed.
    static ImpactResult Solve(const ImpactContext& ctx) {
        ImpactResult result;
        result.force = CalculateForce(ctx.attacker_mass_kg, ctx.attacker_velocity);
        result.resistance = ctx.defender_durability;
        result.difference = result.force - ctx.defender_durability;
        result.severity = result.difference > 100.0f   ? ImpactSeverity::kHigh
                           : result.difference > 0.0f   ? ImpactSeverity::kModerate
                                                         : ImpactSeverity::kLow;

        // Style: how much the attacker's own commitment/aggression
        // carries into the strike. aggression=0.5 (the ImpactContext
        // default) -> exactly 1.0x.
        float styleMultiplier = 0.75f + ctx.attacker_aggression * 0.5f;
        float styledForce = result.force * ctx.attacker_strike_force_multiplier * styleMultiplier;

        // Material: worn material protects less. wear_state=0.0 (the
        // ImpactContext default, "pristine") -> effectiveArmor ==
        // defender_armor exactly, unchanged from Phase 2's own formula.
        float effectiveArmor = ctx.defender_armor * (1.0f - ctx.defender_material_wear * 0.5f);
        float postArmor = ApplyArmor(styledForce, effectiveArmor);

        float bodyDamage = CalculateDamage(postArmor, ctx.struck_body_part);

        // Design: a single final tuning multiplier.
        // risk_reward_balance=0.5 (the ImpactContext default) -> 1.0x.
        float designMultiplier = 0.75f + ctx.design_risk_reward_balance * 0.5f;
        result.damage = bodyDamage * designMultiplier;

        // Echoed forward, never interpreted here -- ReactionSystem::Apply
        // is the only thing that reads these to decide a reaction.
        result.defender_blocking = ctx.defender_blocking;
        result.defender_already_staggered = ctx.defender_already_staggered;
        result.defender_defense_bias = ctx.defender_defense_bias;
        result.impact_dir_x = ctx.impact_dir_x;
        result.visual_style_id = ctx.visual_style_id;

        return result;
    }
};

}  // namespace dominus::combat
