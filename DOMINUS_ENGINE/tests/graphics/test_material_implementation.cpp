// tests/graphics/test_material_implementation.cpp
// Material Implementation Phase: MaterialGenome -> GenomeRegistry ->
// MaterialContract -> a real, resolved GPU-consumable material color,
// reached through the real SceneFromEntities bridge and consumed by
// the real renderers. Tests here focus on SceneFromEntities's new
// registry-backed resolution specifically (identity, mutation,
// registration, creation, failure handling); renderer-binding proofs
// live in test_raster_device.cpp, next to RasterDevice's own existing
// material tests.
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "GRAPHICS/Renderer/MaterialContract.h"
#include "GRAPHICS/Renderer/SceneFromEntities.h"
#include "REGISTRY/GenomeCompiler.h"
#include "REGISTRY/GenomeRegistry.h"
#include "WORLD/Core/EntityRegistry.h"
#include "WORLD/Core/SpatialComponent.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MaterialGenome;
using dominus::character::MaterialGenomeComponent;
using dominus::character::MaterialGenomeLoader;
using dominus::core::MetaBinObject;
using dominus::graphics::MaterialContract;
using dominus::graphics::Scene;
using dominus::graphics::SceneFromEntities;
using dominus::registry::GenomeCompiler;
using dominus::registry::GenomeKind;
using dominus::registry::GenomeRegistry;
using dominus::world::EntityRegistry;
using dominus::world::SpatialComponent;

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

EntityRegistry BuildOneEntityWorld(const MaterialGenome& genome, std::string* outEntityId = nullptr) {
    EntityRegistry registry;
    MetaBinObject entity("material_impl_test_entity", "0.1.0");
    entity.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    entity.AddComponent<MaterialGenomeComponent>(MaterialGenomeComponent{genome});
    registry.CreateEntity(std::move(entity));
    if (outEntityId) *outEntityId = "material_impl_test_entity";
    return registry;
}
}  // namespace

// --- Backward compatibility: nullptr registry preserves old behavior --

DOMINUS_TEST(SceneFromEntities_NoRegistryPassed_MaterialResolvedStaysFalse) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    std::string entityId;
    EntityRegistry registry = BuildOneEntityWorld(*load.value, &entityId);

    Scene scene = SceneFromEntities::Build(registry, {entityId});  // no genomeRegistry -- old signature preserved
    DOMINUS_EXPECT(scene.entities.size() == 1);
    DOMINUS_EXPECT(!scene.entities[0].material_resolved);
    DOMINUS_EXPECT(scene.entities[0].material_ref == load.value->material_id);  // real ref still carried
}

// --- Creation: a real MaterialGenomeComponent produces a real,
// resolved SceneEntity ---------------------------------------------------

DOMINUS_TEST(SceneFromEntities_WithRegistry_ProducesRealResolvedMaterialColor) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    std::string entityId;
    EntityRegistry registry = BuildOneEntityWorld(*load.value, &entityId);
    GenomeRegistry genomeRegistry;

    Scene scene = SceneFromEntities::Build(registry, {entityId}, &genomeRegistry);
    DOMINUS_EXPECT(scene.entities.size() == 1);
    DOMINUS_EXPECT(scene.entities[0].material_resolved);

    // The resolved color must match the REAL chain -- compile the
    // identical genome independently and resolve it, expecting the
    // exact same real color, not a coincidental match.
    auto compiled = GenomeCompiler::CompileMaterialGenome(entityId, *load.value, std::nullopt, 1, "");
    DOMINUS_EXPECT(compiled.ok);
    auto expected = MaterialContract::Resolve(*compiled.artifact);
    DOMINUS_EXPECT(scene.entities[0].material_r == expected.r);
    DOMINUS_EXPECT(scene.entities[0].material_g == expected.g);
    DOMINUS_EXPECT(scene.entities[0].material_b == expected.b);
}

// --- Registration: SceneFromEntities actually registers into the real
// GenomeRegistry passed to it, queryable afterward -----------------------

DOMINUS_TEST(SceneFromEntities_WithRegistry_ActuallyRegistersIntoTheRealRegistry) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    std::string entityId;
    EntityRegistry registry = BuildOneEntityWorld(*load.value, &entityId);
    GenomeRegistry genomeRegistry;
    DOMINUS_EXPECT(genomeRegistry.ArtifactCount() == 0);

    SceneFromEntities::Build(registry, {entityId}, &genomeRegistry);

    DOMINUS_EXPECT(genomeRegistry.ArtifactCount() == 1);
    auto compiled = GenomeCompiler::CompileMaterialGenome(entityId, *load.value, std::nullopt, 1, "");
    const auto* found = genomeRegistry.Find(compiled.artifact->Hash(), GenomeKind::kMaterial);
    DOMINUS_EXPECT(found != nullptr);
    DOMINUS_EXPECT(found->Material().material_id == load.value->material_id);
}

// --- Deterministic identity: same genome data -> same resolved color,
// across independent Build() calls ---------------------------------------

DOMINUS_TEST(SceneFromEntities_SameGenomeData_ProducesDeterministicResolvedColor) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    std::string entityId;
    EntityRegistry registryA = BuildOneEntityWorld(*load.value, &entityId);
    EntityRegistry registryB = BuildOneEntityWorld(*load.value, &entityId);
    GenomeRegistry genomeRegistryA;
    GenomeRegistry genomeRegistryB;

    Scene sceneA = SceneFromEntities::Build(registryA, {entityId}, &genomeRegistryA);
    Scene sceneB = SceneFromEntities::Build(registryB, {entityId}, &genomeRegistryB);

    DOMINUS_EXPECT(sceneA.entities[0].material_r == sceneB.entities[0].material_r);
    DOMINUS_EXPECT(sceneA.entities[0].material_g == sceneB.entities[0].material_g);
    DOMINUS_EXPECT(sceneA.entities[0].material_b == sceneB.entities[0].material_b);
}

// --- Mutation: changing a real MaterialProperties field and rebuilding
// produces a real, different resolved color -------------------------------

DOMINUS_TEST(SceneFromEntities_MutatedMaterialProperty_ProducesDifferentResolvedColor) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    std::string entityId;
    EntityRegistry registry = BuildOneEntityWorld(*load.value, &entityId);
    GenomeRegistry genomeRegistry;

    Scene sceneBefore = SceneFromEntities::Build(registry, {entityId}, &genomeRegistry);

    MetaBinObject* entity = registry.Find(entityId);
    DOMINUS_EXPECT(entity != nullptr);
    auto* materialComponent = entity->GetComponent<MaterialGenomeComponent>();
    DOMINUS_EXPECT(materialComponent != nullptr);
    materialComponent->genome.properties.wear_state = 1.0f;

    Scene sceneAfter = SceneFromEntities::Build(registry, {entityId}, &genomeRegistry);

    bool colorDiffers = sceneBefore.entities[0].material_r != sceneAfter.entities[0].material_r ||
                         sceneBefore.entities[0].material_g != sceneAfter.entities[0].material_g ||
                         sceneBefore.entities[0].material_b != sceneAfter.entities[0].material_b;
    DOMINUS_EXPECT(colorDiffers);
    // Both real, distinct artifacts now live in the same registry.
    DOMINUS_EXPECT(genomeRegistry.ArtifactCount() == 2);
}

// --- Failure handling: a genuinely invalid genome degrades to the
// real, named fallback -- never a crash, never a fabricated color --------

DOMINUS_TEST(SceneFromEntities_InvalidMaterialGenome_DegradesToRealFallback_NeverCrashes) {
    MaterialGenome invalidGenome;
    invalidGenome.material_id = "";  // real, confirmed compile failure (MaterialGenomeCompiler requires non-empty)
    std::string entityId;
    EntityRegistry registry = BuildOneEntityWorld(invalidGenome, &entityId);
    GenomeRegistry genomeRegistry;

    Scene scene = SceneFromEntities::Build(registry, {entityId}, &genomeRegistry);
    DOMINUS_EXPECT(scene.entities.size() == 1);
    DOMINUS_EXPECT(scene.entities[0].material_resolved);  // still real, resolved data -- just via the fallback path

    auto expectedFallback = MaterialContract::ResolveFallback(entityId);
    DOMINUS_EXPECT(scene.entities[0].material_r == expectedFallback.r);
    DOMINUS_EXPECT(scene.entities[0].material_g == expectedFallback.g);
    DOMINUS_EXPECT(scene.entities[0].material_b == expectedFallback.b);
    // A real compile failure must never register a broken artifact.
    DOMINUS_EXPECT(genomeRegistry.ArtifactCount() == 0);
}

// --- No MaterialGenomeComponent at all: real, named fallback, matching
// the identical color the pre-existing renderer fallback already used ----

DOMINUS_TEST(SceneFromEntities_NoMaterialComponent_UsesRealFallback_MatchingPreExistingBehavior) {
    EntityRegistry registry;
    MetaBinObject entity("no_material_entity", "0.1.0");
    entity.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    registry.CreateEntity(std::move(entity));
    GenomeRegistry genomeRegistry;

    Scene scene = SceneFromEntities::Build(registry, {"no_material_entity"}, &genomeRegistry);
    DOMINUS_EXPECT(scene.entities.size() == 1);
    DOMINUS_EXPECT(scene.entities[0].material_resolved);

    auto expectedFallback = MaterialContract::ResolveFallback("no_material_entity");
    DOMINUS_EXPECT(scene.entities[0].material_r == expectedFallback.r);
    DOMINUS_EXPECT(scene.entities[0].material_g == expectedFallback.g);
    DOMINUS_EXPECT(scene.entities[0].material_b == expectedFallback.b);
    DOMINUS_EXPECT(genomeRegistry.ArtifactCount() == 0);
}
