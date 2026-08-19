// tests/visualforge/test_visual_forge_v2.cpp
// VISUALFORGE v0.2: Blueprint Validator, Dependency Graph, Versioned
// Production Snapshots.
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/DependencyGraph.h"
#include "VISUALFORGE/ProductionSnapshot.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MaterialGenomeLoader;
using dominus::character::RigBinder;
using dominus::character::VisualGenomeLoader;
using dominus::character::VisualStyleGenomeLoader;
using dominus::core::DominusSerializer;
using dominus::visualforge::BlueprintValidator;
using dominus::visualforge::CharacterBlueprint;
using dominus::visualforge::CharacterBlueprintForge;
using dominus::visualforge::DependencyGraphForge;
using dominus::visualforge::ProductionSnapshotForge;
using dominus::world::WorldHistory;

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
}  // namespace

// --- BlueprintValidator ------------------------------------------------------

DOMINUS_TEST(BlueprintValidator_RealFullBrooklynBlueprintIsValid) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);

    auto result = BlueprintValidator::Validate(blueprint);
    DOMINUS_EXPECT(result.valid);
    for (const auto& issue : result.issues) {
        DOMINUS_EXPECT(issue.severity != "error");  // no errors on a complete, correct blueprint
    }
}

DOMINUS_TEST(BlueprintValidator_MinimalBlueprintIsStillValidButWarns) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("solo", *visual.value);  // no material, no style

    auto result = BlueprintValidator::Validate(blueprint);
    DOMINUS_EXPECT(result.valid);  // missing optional data is not invalid

    bool hasWarning = false;
    for (const auto& issue : result.issues) {
        if (issue.severity == "warning") hasWarning = true;
        DOMINUS_EXPECT(issue.severity != "error");
    }
    DOMINUS_EXPECT(hasWarning);
}

DOMINUS_TEST(BlueprintValidator_DefaultConstructedBlueprintIsInvalid) {
    CharacterBlueprint empty;  // never went through CharacterBlueprintForge::Build
    auto result = BlueprintValidator::Validate(empty);
    DOMINUS_EXPECT(!result.valid);
}

DOMINUS_TEST(BlueprintValidator_MismatchedStyleReferenceIsAnError) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    auto mismatched = *style.value;
    mismatched.style_id = "STYLE-WRONG";
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, nullptr, &mismatched);

    auto result = BlueprintValidator::Validate(blueprint);
    DOMINUS_EXPECT(!result.valid);
    bool foundStyleError = false;
    for (const auto& issue : result.issues) {
        if (issue.field == "presence.style_id" && issue.severity == "error") foundStyleError = true;
    }
    DOMINUS_EXPECT(foundStyleError);
}

DOMINUS_TEST(BlueprintValidator_InconsistentMemorySummaryIsAnError) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    blueprint.has_memory = true;
    blueprint.memory.has_history = true;
    blueprint.memory.event_count = 0;  // inconsistent on purpose

    auto result = BlueprintValidator::Validate(blueprint);
    DOMINUS_EXPECT(!result.valid);
}

DOMINUS_TEST(BlueprintValidator_IsWellFormedHash_AcceptsRealSha256AndRejectsGarbage) {
    DOMINUS_EXPECT(BlueprintValidator::IsWellFormedHash(std::string(64, 'a')));
    DOMINUS_EXPECT(!BlueprintValidator::IsWellFormedHash("too_short"));
    DOMINUS_EXPECT(!BlueprintValidator::IsWellFormedHash(std::string(64, 'z')));  // not hex
    DOMINUS_EXPECT(!BlueprintValidator::IsWellFormedHash(""));
}

// --- DependencyGraph ----------------------------------------------------------

DOMINUS_TEST(DependencyGraphForge_BuildsRealHashesFromRealBrooklynGenomes) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");

    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
    DOMINUS_EXPECT(graph.entity_id == "brooklyn");
    DOMINUS_EXPECT(BlueprintValidator::IsWellFormedHash(graph.visual_genome_hash));
    DOMINUS_EXPECT(BlueprintValidator::IsWellFormedHash(graph.material_genome_hash));
    DOMINUS_EXPECT(BlueprintValidator::IsWellFormedHash(graph.style_genome_hash));
    DOMINUS_EXPECT(graph.animation_spec_hash.empty());   // not supplied
    DOMINUS_EXPECT(graph.history_snapshot_hash.empty());  // not supplied
}

DOMINUS_TEST(DependencyGraphForge_VisualHashMatchesRegistrysOwnCompiler) {
    // The dependency graph's visual_genome_hash must be THE SAME hash
    // REGISTRY::VisualGenomeCompiler would independently produce -- not
    // a second, parallel hash scheme.
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto directCompile = dominus::registry::VisualGenomeCompiler::Compile(*visual.value);
    DOMINUS_EXPECT(graph.visual_genome_hash == directCompile.hash);
}

DOMINUS_TEST(DependencyGraphForge_Validate_PassesForRealGraph) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto result = DependencyGraphForge::Validate(graph);
    DOMINUS_EXPECT(result.valid);
}

DOMINUS_TEST(DependencyGraphForge_Validate_CatchesACorruptedHashField) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value);
    graph.material_genome_hash = "not_a_real_hash";  // simulate a corrupted/truncated write
    auto result = DependencyGraphForge::Validate(graph);
    DOMINUS_EXPECT(!result.valid);
}

DOMINUS_TEST(DependencyGraphForge_ChangedDependencies_DetectsARealMaterialChange) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");

    auto graphA = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);

    auto wornMaterial = *material.value;
    wornMaterial.properties.wear_state = 0.9f;
    auto graphB = DependencyGraphForge::Build("brooklyn", *visual.value, &wornMaterial);

    auto changed = DependencyGraphForge::ChangedDependencies(graphA, graphB);
    DOMINUS_EXPECT(changed.size() == 1);
    DOMINUS_EXPECT(changed[0] == "material_genome");
}

DOMINUS_TEST(DependencyGraphForge_ChangedDependencies_EmptyWhenNothingChanged) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto graphA = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto graphB = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto changed = DependencyGraphForge::ChangedDependencies(graphA, graphB);
    DOMINUS_EXPECT(changed.empty());
}

DOMINUS_TEST(DependencyGraphForge_ChangedDependencies_DetectsAttachAndDetach) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");

    auto graphWithout = DependencyGraphForge::Build("brooklyn", *visual.value);  // no material
    auto graphWith = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);

    auto changed = DependencyGraphForge::ChangedDependencies(graphWithout, graphWith);
    DOMINUS_EXPECT(changed.size() == 1);
    DOMINUS_EXPECT(changed[0] == "material_genome");
}

// --- ProductionSnapshot -------------------------------------------------------

DOMINUS_TEST(ProductionSnapshotForge_CreateInitial_StartsAtVersionOne) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);

    auto snapshot = ProductionSnapshotForge::CreateInitial("BROOKLYN_VISUAL_BUILD_001", "brooklyn", "t", graph,
                                                             validation, "HITM City v1");
    DOMINUS_EXPECT(snapshot.snapshot_id == "BROOKLYN_VISUAL_BUILD_001");
    DOMINUS_EXPECT(snapshot.visual_version == 1);
    DOMINUS_EXPECT(snapshot.material_version == 1);
    DOMINUS_EXPECT(snapshot.animation_version == 1);
    DOMINUS_EXPECT(snapshot.style_label == "HITM City v1");
    DOMINUS_EXPECT(snapshot.snapshot_hash.size() == 64);
}

DOMINUS_TEST(ProductionSnapshotForge_CreateNext_BumpsOnlyTheChangedDependencyVersion) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value);
    auto graphV1 = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);
    auto validation = BlueprintValidator::Validate(blueprint);

    auto snapshotV1 =
        ProductionSnapshotForge::CreateInitial("BROOKLYN_VISUAL_BUILD_001", "brooklyn", "t1", graphV1, validation);

    // Only material changes.
    auto wornMaterial = *material.value;
    wornMaterial.properties.wear_state = 0.75f;
    auto graphV2 = DependencyGraphForge::Build("brooklyn", *visual.value, &wornMaterial);

    auto snapshotV2 = ProductionSnapshotForge::CreateNext("BROOKLYN_VISUAL_BUILD_002", snapshotV1, "t2", graphV2,
                                                            validation);

    DOMINUS_EXPECT(snapshotV2.visual_version == 1);      // unchanged
    DOMINUS_EXPECT(snapshotV2.material_version == 2);    // bumped -- the real change
    DOMINUS_EXPECT(snapshotV2.animation_version == 1);   // unchanged
    DOMINUS_EXPECT(snapshotV2.snapshot_hash != snapshotV1.snapshot_hash);
}

DOMINUS_TEST(ProductionSnapshotForge_CreateNext_NoChangeMeansNoVersionBump) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);

    auto snapshotV1 =
        ProductionSnapshotForge::CreateInitial("BUILD_001", "brooklyn", "t1", graph, validation);
    // Rebuild the identical graph -- nothing actually changed.
    auto graphAgain = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto snapshotV2 = ProductionSnapshotForge::CreateNext("BUILD_002", snapshotV1, "t2", graphAgain, validation);

    DOMINUS_EXPECT(snapshotV2.visual_version == snapshotV1.visual_version);
    DOMINUS_EXPECT(snapshotV2.material_version == snapshotV1.material_version);
    DOMINUS_EXPECT(snapshotV2.animation_version == snapshotV1.animation_version);
}

DOMINUS_TEST(ProductionSnapshotForge_CreateNext_PreservesStyleLabelWhenNotOverridden) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto graph = DependencyGraphForge::Build("brooklyn", *visual.value);
    auto validation = BlueprintValidator::Validate(blueprint);

    auto snapshotV1 =
        ProductionSnapshotForge::CreateInitial("BUILD_001", "brooklyn", "t1", graph, validation, "HITM City v1");
    auto snapshotV2 = ProductionSnapshotForge::CreateNext("BUILD_002", snapshotV1, "t2", graph, validation);
    DOMINUS_EXPECT(snapshotV2.style_label == "HITM City v1");  // carried forward, not blanked
}
