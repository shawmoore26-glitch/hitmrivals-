// VISUALFORGE/BlueprintValidator.h
// CharacterBlueprint -> Validation -> Valid/Invalid, run before a
// RendererPackage gets built. Two severities, not one: "error" blocks
// (Valid=false), "warning" doesn't -- an entity genuinely having no
// MaterialGenome attached isn't a defect (Phase 4's own "Visual Genome
// alone still produces a valid blueprint" case), so that's a warning,
// not an error. A style_id that's set on both sides and disagrees IS a
// real data-integrity defect -- that's an error.
#pragma once

#include <cctype>
#include <string>
#include <vector>

#include "VISUALFORGE/CharacterBlueprint.h"

namespace dominus::visualforge {

struct ValidationIssue {
    std::string severity;  // "error" | "warning"
    std::string field;
    std::string message;
};

struct ValidationResult {
    bool valid = true;  // false iff at least one "error"-severity issue exists
    std::vector<ValidationIssue> issues;
};

class BlueprintValidator {
public:
    // A real SHA-256 hex digest is exactly 64 lowercase hex characters
    // -- reused by DependencyGraph's own "Registry hashes valid" check
    // (VISUALFORGE/DependencyGraph.h) so there's one definition of
    // "well-formed hash," not two.
    static bool IsWellFormedHash(const std::string& hash) {
        if (hash.size() != 64) return false;
        for (char c : hash) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    static ValidationResult Validate(const CharacterBlueprint& blueprint) {
        ValidationResult result;

        // "VisualGenome exists" -- CharacterBlueprintForge::Build always
        // requires one structurally, so the honest signal that this
        // blueprint never actually went through the forge (e.g. someone
        // hand-built or deserialized a default-constructed one) is empty
        // identity fields across the board.
        if (blueprint.entity_id.empty()) {
            result.issues.push_back(
                {"error", "entity_id", "no entity_id -- was this CharacterBlueprint ever built via CharacterBlueprintForge::Build?"});
        }
        if (blueprint.form.body_type.empty() && blueprint.form.silhouette.empty() &&
            blueprint.form.proportion.empty() && blueprint.form.shape_language.empty()) {
            result.issues.push_back(
                {"error", "form", "VisualGenome identity fields are all empty -- no real VisualGenome data present"});
        }

        // "MaterialGenome exists" -- advisory, not a defect. An entity
        // with genuinely no material ref (a bare-skin creature) is a
        // real, valid case Phase 4's own tests already cover.
        if (!blueprint.has_material) {
            result.issues.push_back(
                {"warning", "material", "no MaterialGenome attached -- material state is unknown, not invalid"});
        }

        // "Style reference valid" -- CharacterBlueprintForge already
        // computed this cross-check; the validator just surfaces it as
        // a real error, since a set-but-disagreeing style_id IS a
        // genuine data-integrity defect (not a missing-optional-data
        // case like material above).
        if (!blueprint.has_visual_style) {
            result.issues.push_back(
                {"warning", "visual_style", "no VisualStyleGenome attached -- style language is unknown, not invalid"});
        } else if (!blueprint.style_reference_matches) {
            result.issues.push_back({"error", "presence.style_id",
                                      "VisualGenome.presence.style_id ('" + blueprint.presence.style_id +
                                          "') does not match the attached VisualStyleGenome.style_id ('" +
                                          blueprint.style_id + "')"});
        }

        // "History references valid" -- a real internal-consistency
        // check on the VisualMemorySummary, not a claim about whether
        // the underlying WorldHistory itself is correct (this engine
        // has no way to verify that independently).
        if (blueprint.has_memory) {
            if (blueprint.memory.has_history && blueprint.memory.event_count == 0) {
                result.issues.push_back(
                    {"error", "memory", "memory.has_history is true but event_count is 0 -- inconsistent VisualMemorySummary"});
            }
            if (!blueprint.memory.has_history && blueprint.memory.event_count > 0) {
                result.issues.push_back(
                    {"error", "memory", "memory.event_count > 0 but has_history is false -- inconsistent VisualMemorySummary"});
            }
            if (blueprint.memory.has_history && blueprint.memory.last_event_time < blueprint.memory.first_event_time) {
                result.issues.push_back(
                    {"error", "memory", "memory.last_event_time is before first_event_time -- inconsistent VisualMemorySummary"});
            }
        }

        for (const auto& issue : result.issues) {
            if (issue.severity == "error") result.valid = false;
        }
        return result;
    }
};

}  // namespace dominus::visualforge
