// CHARACTER/Genome/GameDesignGenome.h
// Scoped to four fields, not the source document's full 20-dimension
// list (Genre/Core Loop/Player Psychology/Economy/Difficulty/
// Progression/Risk/Reward/Exploration/Narrative/Social/AI/World/
// Crafting/Combat/Movement/Replayability/Monetization/Modding/
// Accessibility). Most of those have no engine hook yet -- there is no
// narrative system, no economy system, no monetization system, no
// modding system for a "Monetization" or "Modding" field to
// meaningfully constrain. Modeling a field with nothing to check it
// against would be exactly the "data with no real consumer" pattern
// this engine tracks honestly (same as CombatStyleGenome's weights
// before a decoder exists) -- except worse, because these fields
// wouldn't even have a plausible FUTURE consumer without first building
// the systems they're supposed to govern.
//
// `genre` and `difficulty`/`risk_reward_balance` were kept because
// GameDesignCoherenceChecker (see that file) can say something real
// and concrete about them against genomes that already exist
// (CombatStyleGenome, CombatPhysicsGenome).
#pragma once

#include <string>

namespace dominus::character {

struct GameDesignGenome {
    std::string genre;       // required, free-form: "soulslike", "arcade", ...
    std::string core_loop;   // free-form description, not interpreted by the engine

    float difficulty = 0.5f;            // 0-1
    float risk_reward_balance = 0.5f;   // 0-1 -- low = safe/steady, high = high-risk-high-reward
};

}  // namespace dominus::character
