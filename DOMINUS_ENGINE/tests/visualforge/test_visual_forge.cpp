// tests/visualforge/test_visual_forge.cpp
// Visual Forge: "Identity + Visual Rules + Material State + Style
// Language + World History" -> a validated CharacterBlueprint, an
// AssetSpecification, an AnimationSpecification, and a hash-addressed
// RendererPackage. Every test below either uses real fixture data
// (Brooklyn's own bound genomes) or hand-built genome structs to
// exercise the optional-input degrade paths.
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "VISUALFORGE/AnimationSpecification.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/RendererPackage.h"
#include "VISUALFORGE/VisualForgeCanonicalSerializer.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MaterialGenomeLoader;
using dominus::character::RigBinder;
using dominus::character::VisualGenomeLoader;
using dominus::character::VisualStyleGenomeLoader;
using dominus::core::DominusSerializer;
using dominus::visualforge::AnimationSpecificationForge;
using dominus::visualforge::AssetSpecificationForge;
using dominus::visualforge::CharacterBlueprint;
using dominus::visualforge::CharacterBlueprintForge;
using dominus::visualforge::RendererPackageForge;
using dominus::visualforge::VisualForgeCanonicalSerializer;
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

// --- CharacterBlueprintForge: real Brooklyn fixtures ------------------------

DOMINUS_TEST(CharacterBlueprintForge_BuildsFromRealBrooklynGenomes) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(visual.ok);
    DOMINUS_EXPECT(material.ok);
    DOMINUS_EXPECT(style.ok);

    auto blueprint =
        CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);

    DOMINUS_EXPECT(blueprint.entity_id == "brooklyn");
    DOMINUS_EXPECT(blueprint.form.body_type == "humanoid");
    DOMINUS_EXPECT(blueprint.presence.aura == "chaotic");
    DOMINUS_EXPECT(blueprint.has_material);
    DOMINUS_EXPECT(blueprint.material_id == "MAT-JACKET-001");
    DOMINUS_EXPECT(blueprint.has_visual_style);
    DOMINUS_EXPECT(blueprint.style_name == "Urban Combat");
    // Real cross-check: Brooklyn's own presence.style_id genuinely
    // matches his own visual style's style_id.
    DOMINUS_EXPECT(blueprint.style_reference_matches);
}

DOMINUS_TEST(CharacterBlueprintForge_VisualGenomeAloneStillProducesAValidBlueprint) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    DOMINUS_EXPECT(visual.ok);

    auto blueprint = CharacterBlueprintForge::Build("solo", *visual.value);
    DOMINUS_EXPECT(blueprint.entity_id == "solo");
    DOMINUS_EXPECT(!blueprint.has_material);
    DOMINUS_EXPECT(!blueprint.has_visual_style);
    DOMINUS_EXPECT(!blueprint.has_memory);
    // Nothing to contradict -- stays true by default.
    DOMINUS_EXPECT(blueprint.style_reference_matches);
}

DOMINUS_TEST(CharacterBlueprintForge_FlagsAMismatchedStyleReferenceHonestly) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");  // style_id=STYLE-URBAN-COMBAT
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(visual.ok);
    DOMINUS_EXPECT(style.ok);

    // Deliberately mismatch the referenced style's own id.
    auto mismatchedStyle = *style.value;
    mismatchedStyle.style_id = "STYLE-SOMETHING-ELSE";

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, nullptr, &mismatchedStyle);
    DOMINUS_EXPECT(!blueprint.style_reference_matches);
}

DOMINUS_TEST(CharacterBlueprintForge_WorldHistoryPopulatesRealMemorySummary) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    DOMINUS_EXPECT(visual.ok);

    WorldHistory history;
    history.Record(10.0f, "battle_won", "brooklyn", "won a fight");
    history.Record(20.0f, "battle_won", "brooklyn", "won another");

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, nullptr, nullptr, &history);
    DOMINUS_EXPECT(blueprint.has_memory);
    DOMINUS_EXPECT(blueprint.memory.event_count == 2);
    DOMINUS_EXPECT(blueprint.memory.has_history);
}

// --- AssetSpecificationForge -------------------------------------------------

DOMINUS_TEST(AssetSpecificationForge_DerivesRequirementsFromRealBlueprintFields) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual_style.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);

    auto spec = AssetSpecificationForge::Build(blueprint);
    DOMINUS_EXPECT(spec.entity_id == "brooklyn");
    // mesh + skin texture + clothing material always present; + material
    // genome entry + style reference entry since both are attached here.
    DOMINUS_EXPECT(spec.requirements.size() == 5);

    bool foundMesh = false, foundMaterialGenome = false, foundStyleRef = false;
    for (const auto& r : spec.requirements) {
        if (r.category == "mesh") foundMesh = true;
        if (r.source_field == "material_genome") foundMaterialGenome = true;
        if (r.source_field == "visual_style") foundStyleRef = true;
    }
    DOMINUS_EXPECT(foundMesh);
    DOMINUS_EXPECT(foundMaterialGenome);
    DOMINUS_EXPECT(foundStyleRef);
}

DOMINUS_TEST(AssetSpecificationForge_MinimalBlueprintStillProducesTheThreeAlwaysRequiredEntries) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("solo", *visual.value);

    auto spec = AssetSpecificationForge::Build(blueprint);
    DOMINUS_EXPECT(spec.requirements.size() == 3);  // mesh, skin texture, clothing material only
}

// --- AnimationSpecificationForge: real bound MotionGraph/AnimationSet ------

DOMINUS_TEST(AnimationSpecificationForge_BuildsFromRealBoundBrooklynMotionGraph) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    auto bindResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* motionGraph = obj.GetComponent<dominus::character::MotionGraphComponent>();
    auto* animationSet = obj.GetComponent<dominus::character::AnimationSetComponent>();
    DOMINUS_EXPECT(motionGraph != nullptr);
    DOMINUS_EXPECT(animationSet != nullptr);

    auto spec = AnimationSpecificationForge::Build("brooklyn", motionGraph, animationSet);
    DOMINUS_EXPECT(spec.has_value());
    DOMINUS_EXPECT(spec->entry_state == "idle");
    DOMINUS_EXPECT(!spec->states.empty());
    DOMINUS_EXPECT(!spec->transitions.empty());

    // Every state's clip must genuinely resolve -- Brooklyn's real
    // motion graph is a complete, previously-validated fixture.
    for (const auto& state : spec->states) {
        DOMINUS_EXPECT(state.clip_found);
    }
}

DOMINUS_TEST(AnimationSpecificationForge_ReturnsNulloptWhenMotionGraphMissing) {
    auto spec = AnimationSpecificationForge::Build("nobody", nullptr, nullptr);
    DOMINUS_EXPECT(!spec.has_value());
}

// --- RendererPackageForge: hash-addressed, deterministic --------------------
// (v0.3: Build now requires a DependencyGraph and returns a
// RendererPackageResult -- see tests/visualforge/test_visual_forge_v3.cpp
// for the gate/provenance-chain behavior these tests don't cover.)

DOMINUS_TEST(RendererPackageForge_SameInputsProduceSameHashes) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = dominus::visualforge::DependencyGraphForge::Build("brooklyn", *visual.value);

    auto packageA = RendererPackageForge::Build("brooklyn", "2026-01-01T00:00:00Z", blueprint, assets, deps);
    auto packageB = RendererPackageForge::Build("brooklyn", "2026-01-01T00:00:00Z", blueprint, assets, deps);

    DOMINUS_EXPECT(packageA.ok);
    DOMINUS_EXPECT(packageB.ok);
    DOMINUS_EXPECT(packageA.package->character_blueprint_hash == packageB.package->character_blueprint_hash);
    DOMINUS_EXPECT(packageA.package->asset_specification_hash == packageB.package->asset_specification_hash);
    DOMINUS_EXPECT(packageA.package->package_hash == packageB.package->package_hash);
    DOMINUS_EXPECT(packageA.package->character_blueprint_hash.size() == 64);
}

DOMINUS_TEST(RendererPackageForge_DifferentBlueprintProducesDifferentHash) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprintA = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto blueprintB = blueprintA;
    blueprintB.presence.aura = "calm";  // real, meaningful change

    auto assets = AssetSpecificationForge::Build(blueprintA);
    auto deps = dominus::visualforge::DependencyGraphForge::Build("brooklyn", *visual.value);
    auto packageA = RendererPackageForge::Build("brooklyn", "t", blueprintA, assets, deps);
    auto packageB = RendererPackageForge::Build("brooklyn", "t", blueprintB, assets, deps);

    DOMINUS_EXPECT(packageA.ok);
    DOMINUS_EXPECT(packageB.ok);
    DOMINUS_EXPECT(packageA.package->character_blueprint_hash != packageB.package->character_blueprint_hash);
    DOMINUS_EXPECT(packageA.package->package_hash != packageB.package->package_hash);
}

DOMINUS_TEST(RendererPackageForge_NoAnimationSpecLeavesItsHashEmptyNotFabricated) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("solo", *visual.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = dominus::visualforge::DependencyGraphForge::Build("solo", *visual.value);

    auto result = RendererPackageForge::Build("solo", "t", blueprint, assets, deps);  // no animation arg
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(!result.package->has_animation_specification);
    DOMINUS_EXPECT(result.package->animation_specification_hash.empty());
    // The package hash still exists and is real -- absence of one
    // input doesn't break the package, it's just honestly reflected.
    DOMINUS_EXPECT(result.package->package_hash.size() == 64);
}

DOMINUS_TEST(RendererPackageForge_FullPipelineWithRealAnimationSpec) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);

    auto* visualComp = obj.GetComponent<dominus::character::VisualGenomeComponent>();
    auto* materialComp = obj.GetComponent<dominus::character::MaterialGenomeComponent>();
    auto* styleComp = obj.GetComponent<dominus::character::VisualStyleGenomeComponent>();
    auto* motionGraph = obj.GetComponent<dominus::character::MotionGraphComponent>();
    auto* animationSet = obj.GetComponent<dominus::character::AnimationSetComponent>();
    DOMINUS_EXPECT(visualComp != nullptr);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", visualComp->genome,
                                                      materialComp ? &materialComp->genome : nullptr,
                                                      styleComp ? &styleComp->genome : nullptr);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto animation = AnimationSpecificationForge::Build("brooklyn", motionGraph, animationSet);
    DOMINUS_EXPECT(animation.has_value());
    auto deps = dominus::visualforge::DependencyGraphForge::Build(
        "brooklyn", visualComp->genome, materialComp ? &materialComp->genome : nullptr,
        styleComp ? &styleComp->genome : nullptr, nullptr, animation);

    auto result = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, deps, animation);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.package->has_animation_specification);
    DOMINUS_EXPECT(!result.package->animation_specification_hash.empty());
    DOMINUS_EXPECT(result.package->animation_specification_hash.size() == 64);
}

// --- Canonical serializer determinism ---------------------------------------

DOMINUS_TEST(VisualForgeCanonicalSerializer_SameBlueprintProducesSameBytes) {
    auto fixtureDir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_visual.json");
    auto blueprintA = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto blueprintB = CharacterBlueprintForge::Build("brooklyn", *visual.value);

    DOMINUS_EXPECT(VisualForgeCanonicalSerializer::SerializeCharacterBlueprint(blueprintA) ==
                   VisualForgeCanonicalSerializer::SerializeCharacterBlueprint(blueprintB));
}
