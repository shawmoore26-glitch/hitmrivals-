// CHARACTER/Genome/SocialGenome.h
// "Characters aren't quest markers. They have relationships." Same
// pattern as CombatIdentity: pure data, no behavior baked in here --
// what a "rival" relationship actually DOES (AI weighting, dialogue
// tone, faction standing) is a future extension's job, same as
// CombatIdentity itself does nothing until GenomeDecoder + CombatAI
// interpret it.
#pragma once

#include <string>
#include <vector>

namespace dominus::character {

struct PersonalityTraits {
    float trust = 0.5f;
    float aggression = 0.5f;
    float loyalty = 0.5f;
};

// `relation` is free-form ("rival", "ally", "mentor", "family", ...) --
// CHARACTER doesn't own or validate this vocabulary, same discipline as
// every other free-form tag in this engine (WORLD's event_type,
// PHYSICS's collision layers). `strength` is signed: negative values
// are meaningful (a strongly negative "rival" reads differently from a
// mildly negative one), not just magnitude.
struct Relationship {
    std::string entity_id;
    std::string relation;
    float strength = 0.0f;
};

struct SocialGenome {
    PersonalityTraits personality;
    std::vector<Relationship> relationships;
};

}  // namespace dominus::character
