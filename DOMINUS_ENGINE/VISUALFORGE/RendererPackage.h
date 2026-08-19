// VISUALFORGE/RendererPackage.h
// v0.3 -- LAW: Visual Package Integrity. A RendererPackage cannot
// exist unless its source CharacterBlueprint has passed
// BlueprintValidator. Not "package with a warning" -- no package.
// Build() now validates internally, first, before anything else runs,
// and returns a RendererPackageResult the caller cannot misuse into
// skipping the check -- there is no code path that hands back a
// RendererPackage without result.ok having been true. Same pattern
// REGISTRY::GenomeCompiler/MaterialGenomeCompiler already established
// (Validator -> Serializer -> Hash -> Artifact, refuse before any of
// the later stages run), applied here as an explicit law rather than
// an implicit convention.
//
// Provenance: a RendererPackage now carries its full ancestry --
// created_from (character_blueprint_hash), depends_on (the same
// per-genome hashes DependencyGraph already computes), and the
// validation_artifact_hash that proves it passed the gate. "Every
// visual artifact has ancestry" -- checkable, not asserted.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "REGISTRY/Hash/Sha256.h"
#include "VISUALFORGE/AnimationSpecification.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/BlueprintValidationArtifact.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/DependencyGraph.h"
#include "VISUALFORGE/VisualForgeCanonicalSerializer.h"

namespace dominus::visualforge {

struct RendererPackage {
    std::string entity_id;
    std::string compiled_at;

    // created_from
    std::string character_blueprint_hash;
    std::string asset_specification_hash;
    std::string animation_specification_hash;  // empty if no AnimationSpecification was available
    bool has_animation_specification = false;

    // depends_on -- the same real per-genome hashes DependencyGraph
    // already computes, embedded here rather than re-derived.
    DependencyGraph depends_on;

    // proof this package passed the gate -- never absent, since a
    // package that failed the gate is never constructed at all.
    std::string validation_artifact_hash;

    // A hash of everything above, in fixed order -- the package's own
    // identity. Changing ANY dependency changes this.
    std::string package_hash;
};

struct RendererPackageResult {
    bool ok = false;
    std::optional<RendererPackage> package;
    // Always populated, pass or fail -- "the package remembers this
    // was validated" applies even to a refusal: a caller can inspect
    // exactly why the gate refused, not just that it did.
    BlueprintValidationArtifact validation_artifact;
};

class RendererPackageForge {
public:
    // dependencies is caller-supplied (built via DependencyGraphForge::
    // Build, same as v0.2 already established) rather than re-derived
    // here -- RendererPackageForge's job is the gate + the bundle, not
    // re-deriving genome hashes it doesn't have the original genome
    // objects for (only the flattened CharacterBlueprint).
    static RendererPackageResult Build(const std::string& entityId, const std::string& compiledAt,
                                        const CharacterBlueprint& blueprint, const AssetSpecification& assets,
                                        const DependencyGraph& dependencies,
                                        const std::optional<AnimationSpecification>& animation = std::nullopt) {
        RendererPackageResult result;

        // The gate. First. Before anything else in this function runs.
        result.validation_artifact = BlueprintValidationArtifactForge::Build(blueprint, compiledAt);
        if (result.validation_artifact.result != "PASS") {
            result.ok = false;
            result.package = std::nullopt;  // LAW: no package, not a package with a warning
            return result;
        }

        RendererPackage package;
        package.entity_id = entityId;
        package.compiled_at = compiledAt;

        package.character_blueprint_hash =
            registry::Sha256::Hash(VisualForgeCanonicalSerializer::SerializeCharacterBlueprint(blueprint));
        package.asset_specification_hash =
            registry::Sha256::Hash(VisualForgeCanonicalSerializer::SerializeAssetSpecification(assets));

        if (animation.has_value()) {
            package.has_animation_specification = true;
            package.animation_specification_hash =
                registry::Sha256::Hash(VisualForgeCanonicalSerializer::SerializeAnimationSpecification(*animation));
        }

        package.depends_on = dependencies;
        package.validation_artifact_hash = result.validation_artifact.artifact_hash;

        std::string combined = "character_blueprint=" + package.character_blueprint_hash +
                                ";asset_specification=" + package.asset_specification_hash +
                                ";animation_specification=" + package.animation_specification_hash +
                                ";visual_genome=" + package.depends_on.visual_genome_hash +
                                ";material_genome=" + package.depends_on.material_genome_hash +
                                ";style_genome=" + package.depends_on.style_genome_hash +
                                ";animation_spec_dep=" + package.depends_on.animation_spec_hash +
                                ";history_snapshot=" + package.depends_on.history_snapshot_hash +
                                ";validation_artifact=" + package.validation_artifact_hash;
        package.package_hash = registry::Sha256::Hash(combined);

        result.ok = true;
        result.package = std::move(package);
        return result;
    }
};

}  // namespace dominus::visualforge
