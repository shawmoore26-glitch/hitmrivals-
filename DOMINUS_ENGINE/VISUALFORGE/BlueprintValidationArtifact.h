// VISUALFORGE/BlueprintValidationArtifact.h
// ValidationResult (VISUALFORGE/BlueprintValidator.h) is transient --
// computed, read, discarded. BlueprintValidationArtifact is the durable
// version: a named, timestamped, hash-addressed record that a package
// says "this was validated" about, forever, not just in the moment it
// was built. Same relationship REGISTRY::ImmutableArtifact has to a
// live CombatIdentity.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "REGISTRY/Hash/Sha256.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/CharacterBlueprint.h"

namespace dominus::visualforge {

struct ValidationCheck {
    std::string name;    // "VisualGenome" | "MaterialGenome" | "StyleReference" | "History"
    std::string status;  // "PASS" | "FAIL" -- never anything else; warnings don't fail a named check
    std::vector<std::string> messages;
};

struct BlueprintValidationArtifact {
    std::string blueprint_id;
    std::string timestamp;
    std::string result;  // "PASS" | "FAIL" -- overall, mirrors ValidationResult::valid
    std::vector<ValidationCheck> checks;
    std::string artifact_hash;
};

class BlueprintValidationArtifactForge {
public:
    // Groups BlueprintValidator's own issues into the four named checks
    // the directive's example names -- a real regrouping of real
    // issues, not a second, independent validation pass. A check is
    // "FAIL" iff at least one error-severity issue landed in its group;
    // a warning-only group still reports "PASS" (same "warnings don't
    // block" rule ValidationResult::valid already follows).
    static BlueprintValidationArtifact Build(const CharacterBlueprint& blueprint, const std::string& timestamp) {
        ValidationResult validation = BlueprintValidator::Validate(blueprint);

        BlueprintValidationArtifact artifact;
        artifact.blueprint_id = blueprint.entity_id;
        artifact.timestamp = timestamp;
        artifact.result = validation.valid ? "PASS" : "FAIL";

        artifact.checks.push_back(BuildCheck("VisualGenome", validation, {"entity_id", "form"}));
        artifact.checks.push_back(BuildCheck("MaterialGenome", validation, {"material"}));
        artifact.checks.push_back(BuildCheck("StyleReference", validation, {"visual_style", "presence.style_id"}));
        artifact.checks.push_back(BuildCheck("History", validation, {"memory"}));

        artifact.artifact_hash = registry::Sha256::Hash(Serialize(artifact));
        return artifact;
    }

    static std::string Serialize(const BlueprintValidationArtifact& artifact) {
        std::ostringstream out;
        out << "blueprint_id=" << artifact.blueprint_id << ";timestamp=" << artifact.timestamp
            << ";result=" << artifact.result;
        for (const auto& check : artifact.checks) {
            out << ";[name=" << check.name << ";status=" << check.status;
            for (const auto& m : check.messages) out << ";msg=" << m;
            out << "]";
        }
        return out.str();
    }

private:
    static ValidationCheck BuildCheck(const std::string& name, const ValidationResult& validation,
                                       const std::vector<std::string>& fields) {
        ValidationCheck check;
        check.name = name;
        check.status = "PASS";
        for (const auto& issue : validation.issues) {
            bool matches = false;
            for (const auto& f : fields) {
                if (issue.field == f) matches = true;
            }
            if (!matches) continue;
            check.messages.push_back("[" + issue.severity + "] " + issue.message);
            if (issue.severity == "error") check.status = "FAIL";
        }
        return check;
    }
};

}  // namespace dominus::visualforge
