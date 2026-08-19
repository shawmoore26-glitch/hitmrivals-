// CHARACTER/Genome/DecisionWeights.h
// LAW C003: Combat Genome -> Decision Weights -> Move Selection -> Reaction
// Style. This is the "Decision Weights" stage -- numeric, comparable,
// testable, decoded once from a CombatIdentity and then read by every
// downstream system (ClashSystem's skill multiplier, ReactionSystem's
// stagger/knockdown thresholds, MoveSelector's scoring) instead of each of
// them re-interpreting raw genome strings independently.
#pragma once

namespace dominus::character {

struct DecisionWeights {
    float aggression = 0.5f;       // 0=passive, 1=relentless pressure
    float risk_tolerance = 0.5f;   // 0=avoids high-risk moves, 1=embraces them
    float unpredictability = 0.5f; // 0=predictable/repeats winners, 1=varies choices
    float counter_bias = 0.5f;     // 0=poor counter-fighter, 1=expert counter-fighter
    float defense_bias = 0.5f;     // 0=fragile, 1=hard to stagger/knock down
};

}  // namespace dominus::character
