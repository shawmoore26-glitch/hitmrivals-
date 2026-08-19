// tests/graphics/test_material_contract.cpp
// Material Resource Authority -- Phase 2: proving the deterministic
// relationship between a registered MaterialGenome and its visual
// resolution, entirely at the data level -- no VkImage, no VkSampler,
// no descriptor cache, no renderer. Uses real Brooklyn fixture data,
// matching this repository's own established convention.
#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "GRAPHICS/Renderer/MaterialContract.h"
#include "REGISTRY/GenomeCompiler.h"
#include "REGISTRY/GenomeRegistry.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CombatIdentityLoader;
using dominus::character::MaterialGenomeLoader;
using dominus::graphics::MaterialContract;
using dominus::graphics::MaterialVisualResolution;
using dominus::registry::GenomeCompiler;
using dominus::registry::GenomeKind;
using dominus::registry::GenomeRegistry;

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

// --- has_texture: an explicit, tested fact, not a silent absence ----

DOMINUS_TEST(MaterialContract_RealGenome_HasTextureIsExplicitlyFalse) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    auto compiled = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compiled.ok);

    auto resolution = MaterialContract::Resolve(*compiled.artifact);
    DOMINUS_EXPECT(resolution.has_texture == false);
}

// --- Determinism: real, registered artifact -> real, stable resolution

DOMINUS_TEST(MaterialContract_SameRegisteredArtifact_ProducesIdenticalResolution) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto compiled = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compiled.ok);

    auto resolutionA = MaterialContract::Resolve(*compiled.artifact);
    auto resolutionB = MaterialContract::Resolve(*compiled.artifact);
    DOMINUS_EXPECT(resolutionA.r == resolutionB.r && resolutionA.g == resolutionB.g && resolutionA.b == resolutionB.b);
    DOMINUS_EXPECT(resolutionA.source_hash == resolutionB.source_hash);
    DOMINUS_EXPECT(resolutionA.from_registered_artifact && resolutionB.from_registered_artifact);
}

// --- The real improvement over today's renderer path: sensitivity to
// EVERY genome field, not just material_id -----------------------------

DOMINUS_TEST(MaterialContract_SameMaterialId_DifferentProperties_ProducesDifferentResolution) {
    // Two real genomes, same identity string, different real
    // MaterialProperties (age_years, damage_history, weather_exposure
    // -- fields the CURRENT renderer's material_ref-only color seed
    // completely ignores, confirmed in the prior Visual Asset
    // Authority investigation). This contract must be sensitive to
    // ALL of them, because it derives color from the FULL registered
    // artifact hash, not just material_id.
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    auto genomeA = *load.value;
    auto genomeB = *load.value;
    genomeB.properties.age_years = genomeA.properties.age_years + 50;
    genomeB.properties.damage_history = !genomeA.properties.damage_history;
    DOMINUS_EXPECT(genomeA.material_id == genomeB.material_id);  // same identity string, on purpose

    auto compiledA = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", genomeA, std::nullopt, 1, "t1");
    auto compiledB = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", genomeB, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compiledA.artifact->Hash() != compiledB.artifact->Hash());  // real, distinct registered identity

    auto resolutionA = MaterialContract::Resolve(*compiledA.artifact);
    auto resolutionB = MaterialContract::Resolve(*compiledB.artifact);
    bool colorDiffers =
        resolutionA.r != resolutionB.r || resolutionA.g != resolutionB.g || resolutionA.b != resolutionB.b;
    DOMINUS_EXPECT(colorDiffers);
    DOMINUS_EXPECT(resolutionA.source_hash != resolutionB.source_hash);
}

// --- The named, tested fallback path ------------------------------------

DOMINUS_TEST(MaterialContract_Fallback_IsDeterministicAndDistinctFromRegisteredPath) {
    auto fallbackA = MaterialContract::ResolveFallback("gpu_test_entity_alpha");
    auto fallbackB = MaterialContract::ResolveFallback("gpu_test_entity_alpha");
    DOMINUS_EXPECT(fallbackA.r == fallbackB.r && fallbackA.g == fallbackB.g && fallbackA.b == fallbackB.b);
    DOMINUS_EXPECT(!fallbackA.from_registered_artifact);
    DOMINUS_EXPECT(fallbackA.source_hash.empty());
    DOMINUS_EXPECT(!fallbackA.has_texture);

    // A different identity seed produces a real, different fallback
    // color -- the fallback is deterministic PER SEED, not one fixed
    // universal color.
    auto fallbackDifferentSeed = MaterialContract::ResolveFallback("gpu_test_entity_beta");
    bool colorDiffers = fallbackA.r != fallbackDifferentSeed.r || fallbackA.g != fallbackDifferentSeed.g ||
                         fallbackA.b != fallbackDifferentSeed.b;
    DOMINUS_EXPECT(colorDiffers);
}

// --- Real type safety: a CombatGenome artifact is never
// misinterpreted as material data ---------------------------------------

DOMINUS_TEST(MaterialContract_WrongArtifactKind_DegradesToFallback_NeverMisreadsCombatData) {
    auto combatLoad = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    DOMINUS_EXPECT(combatLoad.ok);
    auto combatCompiled = GenomeCompiler::CompileCombatGenome("brooklyn", *combatLoad.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(combatCompiled.ok);
    DOMINUS_EXPECT(combatCompiled.artifact->Kind() == GenomeKind::kCombat);

    // Passing a real CombatGenome artifact to MaterialContract::Resolve
    // must never crash, never silently read DecisionWeights as if it
    // were MaterialGenome data -- it degrades to the real, named
    // fallback path instead.
    auto resolution = MaterialContract::Resolve(*combatCompiled.artifact);
    DOMINUS_EXPECT(!resolution.from_registered_artifact);
    DOMINUS_EXPECT(resolution.source_hash.empty());

    // And it matches the real fallback path exactly, keyed by the
    // artifact's own real entity id.
    auto expectedFallback = MaterialContract::ResolveFallback(combatCompiled.artifact->EntityId());
    DOMINUS_EXPECT(resolution.r == expectedFallback.r && resolution.g == expectedFallback.g &&
                   resolution.b == expectedFallback.b);
}

// --- The full chain: GenomeRegistry -> authoritative identity ->
// MaterialContract, proven end to end ------------------------------------

DOMINUS_TEST(MaterialContract_ResolvesFromRealRegisteredArtifact_NotJustACompiledOne) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto compiled = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compiled.ok);

    GenomeRegistry registry;
    registry.Register(*compiled.artifact);

    // Retrieved back through the REAL registry lookup mechanism, not
    // the local compiled.artifact variable -- proving the contract
    // works from the actual authoritative registry path, matching the
    // PHASE 1 -> PHASE 2 chain this milestone is meant to prove.
    const auto* retrieved = registry.Find(compiled.artifact->Hash(), GenomeKind::kMaterial);
    DOMINUS_EXPECT(retrieved != nullptr);

    auto resolution = MaterialContract::Resolve(*retrieved);
    DOMINUS_EXPECT(resolution.from_registered_artifact);
    DOMINUS_EXPECT(resolution.source_hash == compiled.artifact->Hash());
    DOMINUS_EXPECT(!resolution.has_texture);
}
