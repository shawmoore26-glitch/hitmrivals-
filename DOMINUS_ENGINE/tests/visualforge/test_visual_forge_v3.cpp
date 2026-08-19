// tests/visualforge/test_visual_forge_v3.cpp
// VISUALFORGE v0.3: LAW -- Visual Package Integrity. A RendererPackage
// cannot exist unless its source CharacterBlueprint has passed
// BlueprintValidator. Plus: BlueprintValidationArtifact, the package's
// provenance chain, ProductionSnapshot lifecycle states, and
// DependencyGraph's rebuild-plan authority.
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "VISUALFORGE/BlueprintValidationArtifact.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/DependencyGraph.h"
#include "VISUALFORGE/ProductionSnapshot.h"
#include "VISUALFORGE/RendererPackage.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MaterialGenomeLoader;
using dominus::character::VisualGenomeLoader;
using dominus::character::VisualStyleGenomeLoader;
using dominus::visualforge::AssetSpecificationForge;
using dominus::visualforge::BlueprintValidationArtifactForge;
using dominus::visualforge::BlueprintValidator;
using dominus::visualforge::CharacterBlueprint;
using dominus::visualforge::CharacterBlueprintForge;
using dominus::visualforge::DependencyGraphForge;
using dominus::visualforge::ProductionSnapshotForge;
using dominus::visualforge::RendererPackageForge;
using dominus::visualforge::SnapshotLifecycle;
using dominus::visualforge::SnapshotState;

namespace {
std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}

CharacterBlueprint LoadRealBrooklynBlueprint() {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    return CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
}
}  // namespace

// --- BlueprintValidationArtifact ----------------------------------------------

DOMINUS_TEST(BlueprintValidationArtifactForge_RealValidBlueprintProducesAllPassChecks) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto artifact = BlueprintValidationArtifactForge::Build(blueprint, "t0");

    DOMINUS_EXPECT(artifact.blueprint_id == "brooklyn");
    DOMINUS_EXPECT(artifact.result == "PASS");
    DOMINUS_EXPECT(artifact.checks.size() == 4);
    for (const auto& check : artifact.checks) {
        DOMINUS_EXPECT(check.status == "PASS");
    }
    DOMINUS_EXPECT(artifact.artifact_hash.size() == 64);
}

DOMINUS_TEST(BlueprintValidationArtifactForge_DefaultBlueprintFailsTheVisualGenomeCheck) {
    CharacterBlueprint empty;  // never built via CharacterBlueprintForge::Build
    auto artifact = BlueprintValidationArtifactForge::Build(empty, "t0");
    DOMINUS_EXPECT(artifact.result == "FAIL");

    bool foundVisualGenomeFail = false;
    for (const auto& check : artifact.checks) {
        if (check.name == "VisualGenome" && check.status == "FAIL") foundVisualGenomeFail = true;
    }
    DOMINUS_EXPECT(foundVisualGenomeFail);
}

DOMINUS_TEST(BlueprintValidationArtifactForge_MismatchedStyleFailsTheStyleReferenceCheckOnly) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    auto mismatched = *style.value;
    mismatched.style_id = "STYLE-WRONG";
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, nullptr, &mismatched);

    auto artifact = BlueprintValidationArtifactForge::Build(blueprint, "t0");
    DOMINUS_EXPECT(artifact.result == "FAIL");

    for (const auto& check : artifact.checks) {
        if (check.name == "StyleReference") DOMINUS_EXPECT(check.status == "FAIL");
        if (check.name == "VisualGenome") DOMINUS_EXPECT(check.status == "PASS");  // unaffected by the style issue
    }
}

DOMINUS_TEST(BlueprintValidationArtifactForge_SameInputsProduceSameHash) {
    auto blueprintA = LoadRealBrooklynBlueprint();
    auto blueprintB = LoadRealBrooklynBlueprint();
    auto artifactA = BlueprintValidationArtifactForge::Build(blueprintA, "t0");
    auto artifactB = BlueprintValidationArtifactForge::Build(blueprintB, "t0");
    DOMINUS_EXPECT(artifactA.artifact_hash == artifactB.artifact_hash);
}

// --- LAW: Visual Package Integrity -- the gate --------------------------------

DOMINUS_TEST(RendererPackageForge_RefusesAnInvalidBlueprint_NoPackageAtAll) {
    CharacterBlueprint invalid;  // never built via the Forge
    auto assets = AssetSpecificationForge::Build(invalid);
    auto deps = DependencyGraphForge::Build("nobody", dominus::character::VisualGenome{});

    auto result = RendererPackageForge::Build("nobody", "t", invalid, assets, deps);

    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.package.has_value());  // LAW: no package, not a package with a warning
    DOMINUS_EXPECT(result.validation_artifact.result == "FAIL");
}

DOMINUS_TEST(RendererPackageForge_AcceptsARealValidBlueprint_AndProducesAPackage) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);

    auto result = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, deps);

    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.package.has_value());
    DOMINUS_EXPECT(result.validation_artifact.result == "PASS");
}

DOMINUS_TEST(RendererPackageForge_MinimalButValidBlueprintStillPassesTheGate) {
    // Missing material/style is a WARNING, not an error -- the gate
    // must still let this through (matches BlueprintValidator's own
    // "advisory, not invalid" rule from v0.2).
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("solo", *visual.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = DependencyGraphForge::Build("solo", *visual.value);

    auto result = RendererPackageForge::Build("solo", "t", blueprint, assets, deps);
    DOMINUS_EXPECT(result.ok);
}

// --- Provenance chain -----------------------------------------------------

DOMINUS_TEST(RendererPackage_CarriesFullProvenanceChain) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);

    auto result = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, deps);
    DOMINUS_EXPECT(result.ok);

    // created_from
    DOMINUS_EXPECT(result.package->character_blueprint_hash.size() == 64);
    // depends_on
    DOMINUS_EXPECT(result.package->depends_on.visual_genome_hash == deps.visual_genome_hash);
    DOMINUS_EXPECT(result.package->depends_on.material_genome_hash == deps.material_genome_hash);
    DOMINUS_EXPECT(result.package->depends_on.style_genome_hash == deps.style_genome_hash);
    // gate proof
    DOMINUS_EXPECT(result.package->validation_artifact_hash == result.validation_artifact.artifact_hash);
    DOMINUS_EXPECT(!result.package->validation_artifact_hash.empty());
}

DOMINUS_TEST(RendererPackage_PackageHashChangesWhenAnyDependencyChanges) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");

    auto depsA = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);
    auto resultA = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, depsA);

    auto wornMaterial = *material.value;
    wornMaterial.properties.wear_state = 0.9f;
    auto depsB = DependencyGraphForge::Build("brooklyn", *visual.value, &wornMaterial);
    auto resultB = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, depsB);

    DOMINUS_EXPECT(resultA.ok);
    DOMINUS_EXPECT(resultB.ok);
    DOMINUS_EXPECT(resultA.package->package_hash != resultB.package->package_hash);
}

// --- Snapshot lifecycle -------------------------------------------------------

DOMINUS_TEST(SnapshotLifecycle_NewSnapshotStartsAtCreated) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);
    auto snap = ProductionSnapshotForge::CreateInitial("BUILD_001", "brooklyn", "t", deps, validation);
    DOMINUS_EXPECT(snap.state == SnapshotState::kCreated);
}

DOMINUS_TEST(SnapshotLifecycle_FullValidPathAdvancesThroughApproved) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);
    DOMINUS_EXPECT(validation.valid);

    auto snap = ProductionSnapshotForge::CreateInitial("BUILD_001", "brooklyn", "t", deps, validation);
    std::string hashAfterCreate = snap.snapshot_hash;

    DOMINUS_EXPECT(SnapshotLifecycle::AdvanceToValidated(snap));
    DOMINUS_EXPECT(snap.state == SnapshotState::kValidated);
    DOMINUS_EXPECT(snap.snapshot_hash != hashAfterCreate);  // promotion is a real, hashed change

    DOMINUS_EXPECT(SnapshotLifecycle::Approve(snap, "shawn"));
    DOMINUS_EXPECT(snap.state == SnapshotState::kApproved);
    DOMINUS_EXPECT(snap.approved_by == "shawn");
    // Approved -> Accepted -> Active is covered by
    // tests/visualforge/test_visualforge_acceptance.cpp, which has the
    // real AcceptanceCertificate this transition requires -- see that
    // file for why Active has no reachable path at all.
}

DOMINUS_TEST(SnapshotLifecycle_CannotAdvanceToValidatedWhenValidationFailed) {
    CharacterBlueprint invalid;
    auto deps = DependencyGraphForge::Build("nobody", dominus::character::VisualGenome{});
    auto validation = BlueprintValidator::Validate(invalid);
    DOMINUS_EXPECT(!validation.valid);

    auto snap = ProductionSnapshotForge::CreateInitial("BUILD_BAD", "nobody", "t", deps, validation);
    DOMINUS_EXPECT(!SnapshotLifecycle::AdvanceToValidated(snap));
    DOMINUS_EXPECT(snap.state == SnapshotState::kCreated);  // unchanged
}

DOMINUS_TEST(SnapshotLifecycle_CannotSkipStates) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);
    auto snap = ProductionSnapshotForge::CreateInitial("BUILD_001", "brooklyn", "t", deps, validation);

    DOMINUS_EXPECT(!SnapshotLifecycle::Approve(snap, "shawn"));  // still Created, can't skip Validated
    DOMINUS_EXPECT(snap.state == SnapshotState::kCreated);
    dominus::visualforge::AcceptanceCertificate fakeCert;
    fakeCert.structurally_sound = true;
    fakeCert.renderable = true;
    fakeCert.certificate_hash = "hash";
    DOMINUS_EXPECT(!SnapshotLifecycle::AdvanceToAccepted(snap, fakeCert));  // still Created, can't skip to Accepted
    DOMINUS_EXPECT(snap.state == SnapshotState::kCreated);
}

DOMINUS_TEST(SnapshotLifecycle_ApproveRequiresARealApprover) {
    auto blueprint = LoadRealBrooklynBlueprint();
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);
    auto snap = ProductionSnapshotForge::CreateInitial("BUILD_001", "brooklyn", "t", deps, validation);
    SnapshotLifecycle::AdvanceToValidated(snap);

    DOMINUS_EXPECT(!SnapshotLifecycle::Approve(snap, ""));  // anonymous approval rejected
    DOMINUS_EXPECT(snap.state == SnapshotState::kValidated);
}

// --- Dependency Graph Authority: rebuild plan ---------------------------------

DOMINUS_TEST(DependencyGraphForge_PlanRebuild_MaterialChangeRebuildsPackageAndAssetsNotAnimation) {
    auto changed = std::vector<std::string>{"material_genome"};
    auto plan = DependencyGraphForge::PlanRebuild(changed);
    DOMINUS_EXPECT(plan.rebuild_renderer_package);
    DOMINUS_EXPECT(plan.rebuild_asset_specification);
    DOMINUS_EXPECT(!plan.rebuild_animation_specification);
}

DOMINUS_TEST(DependencyGraphForge_PlanRebuild_AnimationChangeRebuildsPackageAndAnimationNotAssets) {
    auto changed = std::vector<std::string>{"animation_spec"};
    auto plan = DependencyGraphForge::PlanRebuild(changed);
    DOMINUS_EXPECT(plan.rebuild_renderer_package);
    DOMINUS_EXPECT(!plan.rebuild_asset_specification);
    DOMINUS_EXPECT(plan.rebuild_animation_specification);
}

DOMINUS_TEST(DependencyGraphForge_PlanRebuild_NoChangesRebuildsNothing) {
    auto plan = DependencyGraphForge::PlanRebuild({});
    DOMINUS_EXPECT(!plan.rebuild_renderer_package);
    DOMINUS_EXPECT(!plan.rebuild_asset_specification);
    DOMINUS_EXPECT(!plan.rebuild_animation_specification);
}

DOMINUS_TEST(DependencyGraphForge_PlanRebuild_HistorySnapshotAloneRebuildsPackageOnly) {
    // History informs CharacterBlueprint.memory but nothing
    // AssetSpecification/AnimationSpecification actually reads.
    auto changed = std::vector<std::string>{"history_snapshot"};
    auto plan = DependencyGraphForge::PlanRebuild(changed);
    DOMINUS_EXPECT(plan.rebuild_renderer_package);
    DOMINUS_EXPECT(!plan.rebuild_asset_specification);
    DOMINUS_EXPECT(!plan.rebuild_animation_specification);
}
