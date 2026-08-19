// CHARACTER/Genome/CreatureGenomeSemanticValidator.h
// The second compiler pass, per an explicit directive: "think like a
// compiler... Parse -> Semantic Validation." CreatureGenomeLoader's own
// Validate() checks SYNTAX -- is species_name present, are [0,1] fields
// in range. This checks SEMANTICS -- do the fields agree with each
// other. Kept as a genuinely separate stage, not folded into the loader,
// so a caller can parse+structurally-validate without paying for
// semantic reasoning, and so semantic checks can run against a
// CreatureGenome built any way (loaded, constructed, edited) not just
// one freshly parsed from JSON.
//
// Scoped to five checks that can be evaluated from REAL fields --
// three of them (wing area, water respiration, diet type) needed new
// schema fields added specifically so these checks would be genuine
// numeric/structural logic, not fuzzy string-matching heuristics
// dressed up as "biological consistency." Two of the source document's
// five example checks are NOT implemented here (see the bottom of this
// file) because they would require either speculative new structure or
// unreliable free-text matching -- flagged, not silently skipped.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/CreatureGenome.h"

namespace dominus::character {

enum class SemanticSeverity { kError, kWarning };

struct SemanticIssue {
    SemanticSeverity severity;
    std::string check;  // short identifier, e.g. "flight_wing_area"
    std::string message;
};

class CreatureGenomeSemanticValidator {
public:
    static std::vector<SemanticIssue> Validate(const CreatureGenome& g) {
        std::vector<SemanticIssue> issues;

        CheckFlightVsWingArea(g, issues);
        CheckLocomotionVsLimbCount(g, issues);
        CheckDietVsPreferredPrey(g, issues);
        CheckGrowthReachesMaturity(g, issues);
        CheckAquaticVsRespiration(g, issues);

        return issues;
    }

private:
    static bool ModeContains(const std::string& mode, const char* substr) {
        return mode.find(substr) != std::string::npos;
    }

    // "Wing area insufficient for declared flight capability" -- a real
    // check now that wing_area_m2 exists: claiming flight with zero wing
    // area is an outright contradiction; claiming it with wings too
    // small for the creature's weight is a real wing-loading problem.
    // The 1:50 ratio is a deliberately simple, clearly-labeled heuristic
    // (kg per m^2), not real aerodynamics -- it exists to catch obvious
    // mismatches (a 300kg creature with 0.5 m^2 of wing), not to model
    // flight physics precisely.
    static void CheckFlightVsWingArea(const CreatureGenome& g, std::vector<SemanticIssue>& issues) {
        if (!ModeContains(g.locomotion.primary_mode, "flight")) return;

        if (g.anatomy.wing_area_m2 <= 0.0f) {
            issues.push_back({SemanticSeverity::kError, "flight_wing_area",
                               "locomotion.primary_mode declares flight but anatomy.wing_area_m2 is 0 -- "
                               "no wings to fly with"});
            return;
        }
        const float kMaxKgPerSquareMeter = 50.0f;  // simplified wing-loading heuristic, not real aerodynamics
        if (g.anatomy.weight_kg > g.anatomy.wing_area_m2 * kMaxKgPerSquareMeter) {
            issues.push_back({SemanticSeverity::kError, "flight_wing_area",
                               "wing_area_m2 (" + std::to_string(g.anatomy.wing_area_m2) +
                                   ") is too small to support weight_kg (" + std::to_string(g.anatomy.weight_kg) +
                                   ") for declared flight -- exceeds the " + std::to_string(kMaxKgPerSquareMeter) +
                                   " kg/m^2 heuristic"});
        }
    }

    // "Combat abilities requiring anatomy that isn't present" -- the
    // cleanest real version of this check available without inventing
    // an ability-tagging system: a declared locomotion mode implies a
    // minimum limb count (quadrupedal needs 4+, bipedal needs 2+), and
    // locomotion IS what most close-range combat capability depends on
    // in this schema.
    static void CheckLocomotionVsLimbCount(const CreatureGenome& g, std::vector<SemanticIssue>& issues) {
        if (ModeContains(g.locomotion.primary_mode, "quadrupedal") && g.taxonomy.limb_count < 4) {
            issues.push_back({SemanticSeverity::kError, "locomotion_limb_count",
                               "locomotion.primary_mode is quadrupedal but taxonomy.limb_count is " +
                                   std::to_string(g.taxonomy.limb_count) + " (need >= 4)"});
        }
        if (ModeContains(g.locomotion.primary_mode, "bipedal") && g.taxonomy.limb_count < 2) {
            issues.push_back({SemanticSeverity::kError, "locomotion_limb_count",
                               "locomotion.primary_mode is bipedal but taxonomy.limb_count is " +
                                   std::to_string(g.taxonomy.limb_count) + " (need >= 2)"});
        }
    }

    // "Carnivore digestive system paired with herbivore-only diet" -- the
    // real, checkable version: diet_type and preferred_prey must agree.
    // diet_type == "" is treated as unspecified and skips the check
    // entirely, matching this schema's free-form-by-default philosophy.
    static void CheckDietVsPreferredPrey(const CreatureGenome& g, std::vector<SemanticIssue>& issues) {
        if (g.ecology.diet_type == "herbivore" && !g.ecology.preferred_prey.empty()) {
            issues.push_back({SemanticSeverity::kError, "diet_vs_prey",
                               "ecology.diet_type is herbivore but preferred_prey is non-empty"});
        }
        if (g.ecology.diet_type == "carnivore" && g.ecology.preferred_prey.empty()) {
            issues.push_back({SemanticSeverity::kError, "diet_vs_prey",
                               "ecology.diet_type is carnivore but preferred_prey is empty"});
        }
    }

    // "Growth stages that never reach reproductive maturity" -- clean and
    // numeric with fields that already existed: a creature that matures
    // after its own lifespan ends never reproduces.
    static void CheckGrowthReachesMaturity(const CreatureGenome& g, std::vector<SemanticIssue>& issues) {
        if (g.growth.maturity_age_years >= g.growth.lifespan_years) {
            issues.push_back({SemanticSeverity::kError, "growth_maturity",
                               "growth.maturity_age_years (" + std::to_string(g.growth.maturity_age_years) +
                                   ") >= lifespan_years (" + std::to_string(g.growth.lifespan_years) +
                                   ") -- this creature never reaches reproductive maturity"});
        }
    }

    // "Declared aquatic locomotion without water respiration" -- kept as
    // a WARNING, not an error: real biology has aquatic-adapted air
    // breathers (dolphins, sea turtles), so a mismatch here is worth
    // flagging for a human to confirm, not an automatic rejection.
    static void CheckAquaticVsRespiration(const CreatureGenome& g, std::vector<SemanticIssue>& issues) {
        if (ModeContains(g.locomotion.primary_mode, "aquatic") && !g.physiology.requires_water_respiration) {
            issues.push_back({SemanticSeverity::kWarning, "aquatic_respiration",
                               "locomotion.primary_mode is aquatic but "
                               "physiology.requires_water_respiration is false -- confirm this is intentional "
                               "(air-breathing aquatic life is real, e.g. dolphins, but worth double-checking)"});
        }
    }
};

// NOT implemented, flagged rather than silently skipped:
// - "Wing area insufficient for declared flight capability" -- DONE above.
// - "Carnivore digestive system paired with herbivore-only diet" -- DONE
//   above via diet_type vs preferred_prey (a reasonable proxy; a true
//   "digestive system" field doesn't exist and wasn't invented).
// - "Declared aquatic locomotion without water respiration" -- DONE
//   above, as a warning.
// - "Growth stages that never reach reproductive maturity" -- DONE above.
// - "Combat abilities requiring anatomy that isn't present" -- partially
//   covered via locomotion-vs-limb-count above. A fuller version (e.g.
//   "combat_role: ambush requires camouflage-supporting anatomy") would
//   need either a structured ability-tagging system or fuzzy matching
//   against notable_features' free text -- not attempted, because a
//   heuristic that greps notable_features for keywords is exactly the
//   kind of unreliable check this file's whole design tries to avoid.

}  // namespace dominus::character
