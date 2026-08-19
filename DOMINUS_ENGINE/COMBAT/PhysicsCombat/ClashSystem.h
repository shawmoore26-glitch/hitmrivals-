// COMBAT/PhysicsCombat/ClashSystem.h
// LAW C008: when two attacks collide, the engine weighs force/speed/
// timing/skill, not "whoever's animation started". This is foundation
// scope: a real force/speed model plus a skill modifier, no environmental
// destruction or cinematic-clash staging yet (that's a rendering/FX
// concern layered on top of this decision, not part of the decision
// itself).
#pragma once

#include <algorithm>
#include <cmath>

#include "CHARACTER/Genome/DecisionWeights.h"

namespace dominus::combat {

enum class ClashOutcome {
    kOverpowerA,  // A's attack wins, B's is interrupted
    kOverpowerB,
    kCancel,      // both attacks are cancelled, no damage either way
    kRebound,     // both attacks are knocked back / reset, close margin
};

struct ClashInput {
    float power = 0.0f;
    float speed = 0.0f;
    float skill = 1.0f;  // multiplier, e.g. derived from CombatIdentity.counter tier
};

struct ClashResult {
    ClashOutcome outcome;
    float score_a = 0.0f;
    float score_b = 0.0f;
};

class ClashSystem {
public:
    // LAW C003: the `skill` multiplier in ClashInput is meant to come from
    // genome, not a flat 1.0f everywhere. counter_bias is the natural fit
    // -- an expert counter-fighter reads a clash better than a poor one.
    // Range compressed to [0.7, 1.3] so genome nudges outcomes rather than
    // dominating raw power/speed entirely.
    static float SkillFromWeights(const character::DecisionWeights& weights) {
        return 0.7f + weights.counter_bias * 0.6f;
    }

    // Score = power * (1 + speed/100) * skill -- speed matters but doesn't
    // dominate power, matching how frame-advantage-driven fighting games
    // actually feel (a faster but weaker attack doesn't always win clean).
    static ClashResult Resolve(const ClashInput& a, const ClashInput& b) {
        float scoreA = a.power * (1.0f + a.speed / 100.0f) * a.skill;
        float scoreB = b.power * (1.0f + b.speed / 100.0f) * b.skill;

        ClashResult result;
        result.score_a = scoreA;
        result.score_b = scoreB;

        float diff = scoreA - scoreB;
        float magnitude = std::max(scoreA, scoreB);
        float relativeMargin = magnitude > 0.0f ? std::fabs(diff) / magnitude : 0.0f;

        constexpr float kRebound_Threshold = 0.08f;   // within 8% of each other -> rebound
        constexpr float kCancel_Threshold = 0.02f;    // within 2% -> full cancel (near-perfect tie)

        if (relativeMargin <= kCancel_Threshold) {
            result.outcome = ClashOutcome::kCancel;
        } else if (relativeMargin <= kRebound_Threshold) {
            result.outcome = ClashOutcome::kRebound;
        } else {
            result.outcome = diff > 0.0f ? ClashOutcome::kOverpowerA : ClashOutcome::kOverpowerB;
        }
        return result;
    }
};

}  // namespace dominus::combat
