// COMBAT/ReactionSystem/ReactionSystem.h
// LAW C001: reaction state is downstream of a physics result (hit power +
// defender state), never chosen by an animator picking "the hit clip".
// Reactions map to motion-graph trigger names so CombatController (see
// COMBAT/ReactionSystem/CombatController.h) can drive them through the
// existing MotionGraphEvaluator per LAW C012 -- no separate animation
// system.
//
// Phase 4.1.5 adds Apply(const ImpactResult&) as the single new entry
// point for the ImpactContext -> ImpactSolver::Solve() -> ImpactResult
// pipeline. Apply does not duplicate Determine's logic -- it builds the
// exact same ReactionInput Determine has always taken (echoed back out
// of the ImpactResult) and calls Determine directly, so every existing
// caller of Determine (and every existing test of it) is completely
// unaffected.
#pragma once

#include <string>

#include "COMBAT/PhysicsCombat/ImpactSolver.h"

namespace dominus::combat {

enum class ReactionType {
    kNone,
    kStagger,
    kKnockback,
    kLaunch,
    kWallImpact,
    kGroundImpact,
    kKnockdown,
};

struct ReactionInput {
    float hit_power = 0.0f;
    bool defender_blocking = false;
    bool defender_already_staggered = false;  // juggle/repeat-hit escalation
    // LAW C003: reaction STYLE varies by genome, not just hit power. 0.5 is
    // neutral -- Phase 3's original flat 15/35 power thresholds -- so any
    // caller that doesn't set this (existing Phase 3 code/tests) gets
    // identical behavior to before. Higher defense_bias raises the
    // stagger/knockdown thresholds (tankier fighter); lower lowers them.
    float defense_bias = 0.5f;
};

struct ReactionResult {
    ReactionType type = ReactionType::kNone;
    float force_x = 0.0f;
    float force_y = 0.0f;
    std::string motion_trigger;  // name to fire on the defender's MotionGraphEvaluator
};

class ReactionSystem {
public:
    // Foundation-scope thresholds -- power bands map to reaction severity.
    // A real tuning pass (per-character weight/stagger-resistance from
    // CombatIdentity) is a tracked follow-up once real move data exists to
    // tune against, not invented here.
    static ReactionResult Determine(const ReactionInput& input, float impactDirX) {
        ReactionResult result;
        float dir = impactDirX >= 0.0f ? 1.0f : -1.0f;

        if (input.defender_blocking) {
            result.type = ReactionType::kNone;
            result.motion_trigger = "block_impact";
            return result;
        }

        if (input.defender_already_staggered) {
            // Escalate a repeat hit on an already-staggered target into a
            // launch -- juggle-state behavior (Devil May Cry / FighterZ
            // style combo extension, LAW C004's "stylish action" bucket).
            result.type = ReactionType::kLaunch;
            result.force_x = dir * input.hit_power * 0.4f;
            result.force_y = input.hit_power * 1.2f;
            result.motion_trigger = "launch";
            return result;
        }

        // Thresholds shift with defense_bias around the neutral (0.5)
        // Phase 3 baseline of 15/35 -- at 0.5 these reduce to exactly
        // 15/35, so existing Phase 3 callers see identical behavior.
        float thresholdShift = (input.defense_bias - 0.5f) * 20.0f;  // +-10 at the extremes
        float staggerThreshold = 15.0f + thresholdShift;
        float knockdownThreshold = 35.0f + thresholdShift;

        if (input.hit_power < staggerThreshold) {
            result.type = ReactionType::kStagger;
            result.force_x = dir * input.hit_power * 0.2f;
            result.motion_trigger = "stagger";
        } else if (input.hit_power < knockdownThreshold) {
            result.type = ReactionType::kKnockback;
            result.force_x = dir * input.hit_power * 0.5f;
            result.motion_trigger = "knockback";
        } else {
            result.type = ReactionType::kKnockdown;
            result.force_x = dir * input.hit_power * 0.3f;
            result.force_y = input.hit_power * 0.6f;
            result.motion_trigger = "knockdown";
        }
        return result;
    }

    // Phase 4.1.5: the only place that turns an ImpactResult into a
    // reaction. Genuinely "everything downstream happens here" in the
    // sense that matters for this phase's contract -- REACTION
    // DETERMINATION is fully decided in this class, never in
    // ImpactSolver (which stays pure) and never in CombatController
    // (which only delegates). What Apply does NOT do, honestly flagged
    // rather than faked: mutate a Health component (none exists
    // anywhere in this engine -- see the roadmap entry for why one
    // wasn't invented here), queue a concrete animation, or emit a
    // particle/combat-event -- ReactionResult.motion_trigger is real,
    // computed data that CombatController::ApplyPrecomputedReaction
    // (the SAME existing seam Environment/Cinematic reactions already
    // flow through) is what actually fires it through the motion graph.
    static ReactionResult Apply(const ImpactResult& impact) {
        ReactionInput input;
        input.hit_power = impact.damage;
        input.defender_blocking = impact.defender_blocking;
        input.defender_already_staggered = impact.defender_already_staggered;
        input.defense_bias = impact.defender_defense_bias;
        return Determine(input, impact.impact_dir_x);
    }
};

}  // namespace dominus::combat
