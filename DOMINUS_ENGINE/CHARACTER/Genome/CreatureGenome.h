// CHARACTER/Genome/CreatureGenome.h
// Built per an explicit directive: "Build CreatureGenome as a real
// engine schema first... deterministic and machine-readable... strict
// validation rules... separate data from generation logic." This file
// is data ONLY -- no generation, no evolution simulation, no "94%
// intelligence" claims that nothing computed. Same discipline as
// CombatIdentity and SocialGenome: every field is either a plain value,
// a free-form tag the engine doesn't interpret, or a real numeric weight
// that a FUTURE decoder could read -- never a number invented to look
// like output.
//
// Deliberately does NOT implement: evolution simulation (an actual
// genetic algorithm is real, hard, future work -- not attempted here),
// ecosystem generation, or a "combat intelligence" decoder (no
// CreatureAI exists yet to consume CombatProfile, same honest state
// CombatIdentity was in before GenomeDecoder existed).
#pragma once

#include <string>
#include <vector>

namespace dominus::character {

struct CreatureIdentity {
    std::string species_name;  // required -- the one thing every creature must have
    std::string common_name;
    std::string origin;  // free-form: "Earth", "Unknown Reality Layer", ... -- not validated
};

struct CreatureTaxonomy {
    std::string body_plan;        // free-form: "hexapod", "bipedal", "serpentine", ...
    std::string classification;   // free-form: "apex_predator", "grazer", "scavenger", ...
    int limb_count = 4;
};

struct CreatureAnatomy {
    float height_m = 1.0f;
    float weight_kg = 50.0f;
    std::string armor_type;  // free-form: "none", "scaled", "chitin", "reactive_plating", ...
    std::vector<std::string> notable_features;
    // 0 = no wings. Added specifically so "flight capability vs wing
    // area" can be a real numeric check (see
    // CreatureGenomeSemanticValidator) instead of an unchecked claim.
    float wing_area_m2 = 0.0f;
};

struct CreaturePhysiology {
    std::string metabolism;  // free-form: "warm-blooded", "cold-blooded", "synthetic", ...
    std::vector<std::string> energy_sources;
    std::string reproduction;
    float mutation_rate = 0.1f;  // 0-1, a genome trait -- not a claim of simulated mutation
    // Added so "aquatic locomotion vs respiration" can be checked for
    // real -- deliberately NOT required-if-aquatic (a dolphin is aquatic
    // and air-breathing), so the semantic validator treats a mismatch
    // here as a warning to double-check, not a hard error.
    bool requires_water_respiration = false;
};

struct CreatureSenses {
    std::vector<std::string> sense_types;
    float perception_range_m = 50.0f;
};

struct CreatureLocomotion {
    std::string primary_mode;  // free-form: "quadrupedal", "flight", "aquatic", "burrowing", ...
    float top_speed_kmh = 40.0f;
};

// The one enum in this schema, deliberately: intelligence tier is a
// small, closed, meaningful set (matches the MonsterForge source
// document's own tiers), unlike everything else here which stays
// free-form string tags. A closed enum here is honest because the tiers
// genuinely are a fixed ladder, not an open vocabulary.
enum class IntelligenceTier {
    kInstinct = 0,
    kAnimal = 1,
    kPack = 2,
    kProblemSolving = 3,
    kHumanLevel = 4,
    kSuperhuman = 5,
    kCosmic = 6,
};

struct CreatureCognition {
    IntelligenceTier tier = IntelligenceTier::kAnimal;
    float problem_solving = 0.3f;  // 0-1
    float communication = 0.2f;    // 0-1
};

struct CreatureBehavior {
    std::string temperament;  // free-form
    float aggression = 0.5f;      // 0-1
    float territoriality = 0.5f;  // 0-1
    std::string combat_role;  // free-form: "ambush", "berserker", "tactician", "swarm", ...
};

struct CreatureEcology {
    std::string trophic_role;  // free-form: "apex_predator", "herbivore", "scavenger", ...
    // "herbivore" / "carnivore" / "omnivore" / "" (unspecified, skips
    // the check) -- kept separate from trophic_role because trophic_role
    // is broader ("scavenger" isn't a diet_type) and free-form; diet_type
    // is the one small closed vocabulary the semantic validator checks
    // preferred_prey against.
    std::string diet_type;
    std::vector<std::string> preferred_prey;
    std::vector<std::string> natural_predators;
    std::string habitat;
};

// Deliberately mirrors CombatIdentity's spirit (a creature IS a combat
// entity, same as a fighter) but stays its own type -- a creature's
// "range" and "aggression" are genome traits describing capability, not
// yet decoded into DecisionWeights the way CombatIdentity is. No
// CreatureAI/CreatureGenomeDecoder exists yet to read these.
struct CreatureCombatProfile {
    float aggression = 0.5f;    // 0-1
    float speed = 0.5f;         // 0-1
    float durability = 0.5f;    // 0-1
    float combat_intelligence = 0.5f;  // 0-1
    std::string range;  // free-form: "close", "mid", "far"
};

struct CreatureGrowthLifeCycle {
    float lifespan_years = 10.0f;
    std::vector<std::string> life_stages;  // ordered: "juvenile", "adult", "elder", ...
    float maturity_age_years = 2.0f;
};

// `adaptation_potential` is a genome trait, not a claim that evolution
// was simulated -- see the file header. `lineage` is an authored,
// ordered description of ancestry, not the output of a running
// evolutionary algorithm.
struct CreatureEvolution {
    std::string evolutionary_origin;
    std::vector<std::string> lineage;
    float adaptation_potential = 0.5f;  // 0-1
};

struct CreatureGenome {
    CreatureIdentity identity;
    CreatureTaxonomy taxonomy;
    CreatureAnatomy anatomy;
    CreaturePhysiology physiology;
    CreatureSenses senses;
    CreatureLocomotion locomotion;
    CreatureCognition cognition;
    CreatureBehavior behavior;
    CreatureEcology ecology;
    CreatureCombatProfile combat;
    CreatureGrowthLifeCycle growth;
    CreatureEvolution evolution;
};

}  // namespace dominus::character
