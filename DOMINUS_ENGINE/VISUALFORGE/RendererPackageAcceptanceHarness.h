// VISUALFORGE/RendererPackageAcceptanceHarness.h
// Four structural checks (unchanged from this file's original v1) plus
// two REAL rendering checks, now that GRAPHICS exists as a minimal,
// bounded module (GRAPHICS/Renderer/FrameCompiler.h): can this
// character's blueprint compile into a real, deterministic Frame?
//
// Critical distinction, enforced in code, not just stated: compiling a
// deterministic FRAME (a logical draw-command list) is NOT the same as
// producing correct PIXELS. There is still no rasterizer anywhere in
// this engine. `structurally_sound` and `renderable` are tracked as
// TWO SEPARATE booleans on AcceptanceCertificate specifically so
// nobody can collapse "compiles into a frame" into "looks right on
// screen" -- they are different claims, verified by different checks,
// and neither implies visual correctness.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "GRAPHICS/Renderer/Camera.h"
#include "GRAPHICS/Renderer/Frame.h"
#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "GRAPHICS/Renderer/Scene.h"
#include "VISUALFORGE/AcceptanceCertificate.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/DependencyGraph.h"
#include "VISUALFORGE/RendererPackage.h"

namespace dominus::visualforge {

class RendererPackageAcceptanceHarness {
public:
    // 1. BlueprintValidity -- reuses the real, existing
    // BlueprintValidator (VisualForge v0.2). Not re-implemented here.
    static AcceptanceSection CheckBlueprintValidity(const CharacterBlueprint& blueprint) {
        auto result = BlueprintValidator::Validate(blueprint);
        int errorCount = 0;
        for (const auto& issue : result.issues) {
            if (issue.severity == "error") errorCount++;
        }
        return {"BlueprintValidity", result.valid,
                result.valid ? "no errors (" + std::to_string(result.issues.size()) + " advisory warnings)"
                              : std::to_string(errorCount) + " real error(s) found"};
    }

    // 2. DependencyIntegrity -- reuses the real, existing
    // DependencyGraphForge::Validate (v0.2/v0.3): every present hash is
    // a well-formed SHA-256 digest, visual_genome_hash is never empty.
    static AcceptanceSection CheckDependencyIntegrity(const DependencyGraph& graph) {
        auto result = DependencyGraphForge::Validate(graph);
        return {"DependencyIntegrity", result.valid,
                result.valid ? "all present dependency hashes are well-formed"
                              : std::to_string(result.issues.size()) + " malformed/missing hash(es)"};
    }

    // 3. PackageDeterminism -- builds the SAME RendererPackage TWICE
    // from the same real inputs and requires an identical package_hash.
    // Genuinely executed, not assumed from "the code looks pure."
    static AcceptanceSection CheckPackageDeterminism(const std::string& entityId, const std::string& compiledAt,
                                                       const CharacterBlueprint& blueprint,
                                                       const AssetSpecification& assets,
                                                       const DependencyGraph& dependencies,
                                                       const std::optional<AnimationSpecification>& animation) {
        auto resultA = RendererPackageForge::Build(entityId, compiledAt, blueprint, assets, dependencies, animation);
        auto resultB = RendererPackageForge::Build(entityId, compiledAt, blueprint, assets, dependencies, animation);
        if (!resultA.ok || !resultB.ok) {
            return {"PackageDeterminism", false, "package build failed -- cannot check determinism of a non-package"};
        }
        bool identical = resultA.package->package_hash == resultB.package->package_hash;
        return {"PackageDeterminism", identical,
                identical ? "two independent builds produced an identical package_hash"
                           : "two independent builds produced DIFFERENT package_hash values -- non-deterministic"};
    }

    // 4. ProvenanceCompleteness -- the built package's own depends_on
    // chain must trace back to the real dependency graph it was built
    // from, hash for hash. Not a new computation -- a cross-check that
    // RendererPackageForge didn't silently drop or alter anything.
    static AcceptanceSection CheckProvenanceCompleteness(const RendererPackage& package,
                                                          const DependencyGraph& sourceDependencies) {
        bool matches = package.depends_on.visual_genome_hash == sourceDependencies.visual_genome_hash &&
                       package.depends_on.material_genome_hash == sourceDependencies.material_genome_hash &&
                       package.depends_on.style_genome_hash == sourceDependencies.style_genome_hash &&
                       package.depends_on.animation_spec_hash == sourceDependencies.animation_spec_hash &&
                       package.depends_on.history_snapshot_hash == sourceDependencies.history_snapshot_hash &&
                       !package.validation_artifact_hash.empty();
        return {"ProvenanceCompleteness", matches,
                matches ? "package.depends_on matches the source DependencyGraph exactly, validation artifact present"
                        : "package.depends_on diverges from its own source DependencyGraph"};
    }

    // 5. RenderedOutput -- ALWAYS not_declared. GRAPHICS produces a
    // real deterministic FRAME (see Renderable/RenderDeterminism
    // below), but there is still no rasterizer, no GPU, no actual
    // pixel output anywhere in this engine. This section name is kept
    // distinct from "Renderable" specifically so a real future check
    // (once a rasterizer exists) has an honest, pre-reserved name to
    // land in, rather than overloading "Renderable" to silently start
    // meaning something stronger than it does today.
    static AcceptanceSection RenderedOutputNotDeclared() {
        return {"RenderedOutput", false,
                "NOT_DECLARED -- no rasterizer exists anywhere in this engine; a Frame is a logical draw-command "
                "list, not pixels. Never counted toward structurally_sound or renderable."};
    }

    // 6. Renderable -- REAL, now that GRAPHICS exists. Builds a
    // minimal one-entity Scene from the blueprint's own real material
    // reference, compiles it through the real FrameCompiler, and
    // confirms a valid, non-empty, well-formed Frame comes out. This
    // proves the character's data CAN become a frame -- it does not
    // and cannot prove the frame would look correct, because nothing
    // in this engine can rasterize it yet.
    static AcceptanceSection CheckRenderable(const CharacterBlueprint& blueprint) {
        graphics::Scene scene;
        graphics::SceneEntity entity;
        entity.entity_id = blueprint.entity_id;
        entity.world_transform = animation::Transform2D{};
        entity.material_ref = blueprint.has_material ? blueprint.material_id : "";
        scene.entities.push_back(entity);

        graphics::Camera camera;
        auto frame = graphics::FrameCompiler::Compile(scene, camera);

        bool passed = frame.commands.size() == 1 && frame.frame_hash.size() == 64;
        return {"Renderable", passed,
                passed ? "compiles into a valid, real, hash-addressed Frame (logical draw commands only, no pixels)"
                       : "failed to compile into a valid Frame"};
    }

    // 7. RenderDeterminism -- compiles the SAME scene TWICE and
    // requires an identical frame_hash. Reuses GRAPHICS's own already-
    // tested FrameCompiler determinism guarantee, applied here to this
    // specific character's real blueprint data.
    static AcceptanceSection CheckRenderDeterminism(const CharacterBlueprint& blueprint) {
        graphics::Scene scene;
        graphics::SceneEntity entity;
        entity.entity_id = blueprint.entity_id;
        entity.world_transform = animation::Transform2D{};
        entity.material_ref = blueprint.has_material ? blueprint.material_id : "";
        scene.entities.push_back(entity);

        graphics::Camera camera;
        auto frameA = graphics::FrameCompiler::Compile(scene, camera);
        auto frameB = graphics::FrameCompiler::Compile(scene, camera);

        bool passed = frameA.frame_hash == frameB.frame_hash;
        return {"RenderDeterminism", passed,
                passed ? "two independent frame compilations produced an identical frame_hash"
                       : "frame_hash differs across two independent compilations -- non-deterministic"};
    }
};

}  // namespace dominus::visualforge
