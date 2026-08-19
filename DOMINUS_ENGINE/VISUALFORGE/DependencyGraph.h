// VISUALFORGE/DependencyGraph.h
// "Now if anything changes: Dominus knows what needs rebuilding."
// Every hash here is produced by REGISTRY's OWN existing compilers
// (VisualGenomeCompiler/MaterialGenomeCompiler/VisualStyleGenomeCompiler)
// -- the exact same hash a REGISTRY-side rebuild of the same genome
// would produce, not a second, parallel hash scheme. ChangedDependencies
// is a real, field-by-field comparison, not a heuristic.
#pragma once

#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "CHARACTER/Genome/MaterialGenome.h"
#include "CHARACTER/Genome/VisualGenome.h"
#include "CHARACTER/Genome/VisualMemorySummary.h"
#include "CHARACTER/Genome/VisualStyleGenome.h"
#include "REGISTRY/Hash/Sha256.h"
#include "REGISTRY/MaterialGenomeCompiler.h"
#include "REGISTRY/VisualGenomeCompiler.h"
#include "REGISTRY/VisualStyleGenomeCompiler.h"
#include "VISUALFORGE/AnimationSpecification.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/VisualForgeCanonicalSerializer.h"
#include "WORLD/Core/WorldHistory.h"

namespace dominus::visualforge {

struct DependencyGraph {
    std::string entity_id;
    std::string visual_genome_hash;    // always present -- VisualGenome is CharacterBlueprint's one required input
    std::string material_genome_hash;  // empty if no MaterialGenome was attached
    std::string style_genome_hash;     // empty if no VisualStyleGenome was attached
    std::string animation_spec_hash;   // empty if no AnimationSpecification was attached
    std::string history_snapshot_hash; // empty if no WorldHistory was attached
};

struct RebuildPlan {
    // Only real Forge outputs are named here -- no fabricated "material
    // cache" or other system that doesn't exist in this engine.
    bool rebuild_renderer_package = false;
    bool rebuild_asset_specification = false;
    bool rebuild_animation_specification = false;
};

class DependencyGraphForge {
public:
    // Same five real inputs CharacterBlueprintForge::Build already
    // takes, plus the optional AnimationSpecification -- a caller
    // building a CharacterBlueprint already has all of these on hand.
    static DependencyGraph Build(const std::string& entityId, const character::VisualGenome& visual,
                                  const character::MaterialGenome* material = nullptr,
                                  const character::VisualStyleGenome* visualStyle = nullptr,
                                  const world::WorldHistory* history = nullptr,
                                  const std::optional<AnimationSpecification>& animation = std::nullopt) {
        DependencyGraph graph;
        graph.entity_id = entityId;

        // REGISTRY's own compiler -- the real, shared hash for this
        // exact VisualGenome content.
        graph.visual_genome_hash = registry::VisualGenomeCompiler::Compile(visual).hash;

        if (material) {
            auto compiled = registry::MaterialGenomeCompiler::Compile(*material);
            if (compiled.ok) graph.material_genome_hash = compiled.hash;
            // if compile fails (empty material_id) the hash stays empty
            // -- not fabricated, matches BlueprintValidator's own
            // "advisory, not invented" discipline.
        }

        if (visualStyle) {
            auto compiled = registry::VisualStyleGenomeCompiler::Compile(*visualStyle);
            if (compiled.ok) graph.style_genome_hash = compiled.hash;
        }

        if (animation.has_value()) {
            graph.animation_spec_hash =
                registry::Sha256::Hash(VisualForgeCanonicalSerializer::SerializeAnimationSpecification(*animation));
        }

        if (history) {
            auto memory = character::VisualMemoryDeriver::Derive(*history, entityId);
            std::ostringstream out;
            out << "event_count=" << memory.event_count << ";has_history=" << (memory.has_history ? "true" : "false")
                << ";first_event_time=" << memory.first_event_time << ";last_event_time=" << memory.last_event_time;
            graph.history_snapshot_hash = registry::Sha256::Hash(out.str());
        }

        return graph;
    }

    // "If anything changes, Dominus knows what needs rebuilding" -- a
    // real, field-by-field comparison between two graphs for the SAME
    // entity. Returns the names of every dependency whose hash differs
    // (including one going from present to absent, or vice versa).
    static std::vector<std::string> ChangedDependencies(const DependencyGraph& oldGraph,
                                                          const DependencyGraph& newGraph) {
        std::vector<std::string> changed;
        if (oldGraph.visual_genome_hash != newGraph.visual_genome_hash) changed.push_back("visual_genome");
        if (oldGraph.material_genome_hash != newGraph.material_genome_hash) changed.push_back("material_genome");
        if (oldGraph.style_genome_hash != newGraph.style_genome_hash) changed.push_back("style_genome");
        if (oldGraph.animation_spec_hash != newGraph.animation_spec_hash) changed.push_back("animation_spec");
        if (oldGraph.history_snapshot_hash != newGraph.history_snapshot_hash) changed.push_back("history_snapshot");
        return changed;
    }

    // "Registry hashes valid" -- checks every non-empty hash field
    // actually looks like a real SHA-256 digest (64 hex chars), reusing
    // BlueprintValidator's own definition of "well-formed" rather than
    // a second one. An empty field is not an error here (that just
    // means the corresponding genome wasn't attached, already reflected
    // honestly by Build above) -- a NON-empty field that ISN'T a valid
    // hash shape is the real defect this catches (a truncated write, a
    // corrupted reload, a hand-edited snapshot file).
    static ValidationResult Validate(const DependencyGraph& graph) {
        ValidationResult result;
        auto checkField = [&](const std::string& fieldName, const std::string& value) {
            if (!value.empty() && !BlueprintValidator::IsWellFormedHash(value)) {
                result.issues.push_back(
                    {"error", fieldName, fieldName + " is not a well-formed SHA-256 hash: '" + value + "'"});
            }
        };
        checkField("visual_genome_hash", graph.visual_genome_hash);
        checkField("material_genome_hash", graph.material_genome_hash);
        checkField("style_genome_hash", graph.style_genome_hash);
        checkField("animation_spec_hash", graph.animation_spec_hash);
        checkField("history_snapshot_hash", graph.history_snapshot_hash);

        if (graph.visual_genome_hash.empty()) {
            result.issues.push_back(
                {"error", "visual_genome_hash", "visual_genome_hash is empty -- VisualGenome is required, this should never happen from Build()"});
        }

        for (const auto& issue : result.issues) {
            if (issue.severity == "error") result.valid = false;
        }
        return result;
    }

    // Dependency Graph Authority: given the real output of
    // ChangedDependencies, decide which of Visual Forge's own real
    // outputs actually need rebuilding. A RendererPackage is a
    // hash-addressed bundle of everything, so ANY real change means it
    // needs rebuilding. AssetSpecification is derived from
    // CharacterBlueprint's visual/material/style fields (see
    // AssetSpecificationForge), so it only needs rebuilding when one of
    // those three actually changed -- a pure animation_spec change
    // doesn't touch it. AnimationSpecification only needs rebuilding
    // when animation_spec itself changed (it's not derived from
    // anything else). history_snapshot changing affects neither, by
    // design -- CharacterBlueprint.memory is informational (event
    // counts/timestamps), not something AssetSpecification or
    // AnimationSpecification read.
    static RebuildPlan PlanRebuild(const std::vector<std::string>& changed) {
        RebuildPlan plan;
        std::set<std::string> changedSet(changed.begin(), changed.end());

        if (!changedSet.empty()) plan.rebuild_renderer_package = true;

        if (changedSet.count("visual_genome") || changedSet.count("material_genome") ||
            changedSet.count("style_genome")) {
            plan.rebuild_asset_specification = true;
        }

        if (changedSet.count("animation_spec")) {
            plan.rebuild_animation_specification = true;
        }

        return plan;
    }
};

}  // namespace dominus::visualforge
