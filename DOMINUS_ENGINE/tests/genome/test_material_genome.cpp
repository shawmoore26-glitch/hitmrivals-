// tests/genome/test_material_genome.cpp
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/MaterialWearDeriver.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/MaterialGenomeCompiler.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::character::MaterialGenome;
using dominus::character::MaterialGenomeLoader;
using dominus::character::MaterialWearDeriver;
using dominus::registry::CanonicalSerializer;
using dominus::registry::MaterialGenomeCompiler;
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
bool NearlyEqual(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }
}  // namespace

// --- MaterialGenomeLoader: strict validation ---------------------------------

DOMINUS_TEST(MaterialGenomeLoader_LoadsRealJacketFixture) {
    auto result = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->material_id == "MAT-JACKET-001");
    DOMINUS_EXPECT(result.value->identity.type == "urban_leather");
    DOMINUS_EXPECT(result.value->properties.age_years == 7);
    DOMINUS_EXPECT(result.value->properties.weather_exposure);
}

DOMINUS_TEST(MaterialGenomeLoader_RejectsMissingIdAndInvalidValues) {
    auto result = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "broken_material_genome.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("material_id") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("age_years") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("wear_state") != std::string::npos);
}

DOMINUS_TEST(MaterialGenomeLoader_MissingFileFailsGracefully) {
    auto result = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_material.json");
    DOMINUS_EXPECT(!result.ok);
}

// --- CanonicalSerializer + Compiler: seventh genome type through the pipeline -

DOMINUS_TEST(CanonicalSerializer_MaterialGenome_SameValuesProduceSameBytes) {
    auto a = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto b = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeMaterialGenome(*a.value) ==
                    CanonicalSerializer::SerializeMaterialGenome(*b.value));
}

DOMINUS_TEST(MaterialGenomeCompiler_CompilesDeterministically) {
    auto a = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto b = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto compileA = MaterialGenomeCompiler::Compile(*a.value);
    auto compileB = MaterialGenomeCompiler::Compile(*b.value);
    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);
    DOMINUS_EXPECT(compileA.hash == compileB.hash);
}

DOMINUS_TEST(MaterialGenomeCompiler_RejectsEmptyMaterialId) {
    MaterialGenome invalid;
    auto result = MaterialGenomeCompiler::Compile(invalid);
    DOMINUS_EXPECT(!result.ok);
}

// --- MaterialWearDeriver: the real WorldHistory connection, second proof -----

DOMINUS_TEST(MaterialWearDeriver_NoHistoryMeansZeroWear) {
    WorldHistory history;
    float wear = MaterialWearDeriver::DeriveWearState(history, "MAT-JACKET-001");
    DOMINUS_EXPECT(NearlyEqual(wear, 0.0f));
    DOMINUS_EXPECT(!MaterialWearDeriver::HasAnyHistory(history, "MAT-JACKET-001"));
}

DOMINUS_TEST(MaterialWearDeriver_RealDamageEventsProduceRealWear) {
    // The document's own worked example, made real: "Sword is damaged
    // because: EVENT Battle_482, RESULT Durability -15%." Here: the
    // jacket takes damage across several real recorded battles.
    WorldHistory history;
    history.Record(0.0f, "entity_created", "MAT-JACKET-001", "Jacket crafted");
    history.Record(30.0f, "damage_event", "MAT-JACKET-001", "Battle_482 -- torn sleeve");
    history.Record(60.0f, "damage_event", "MAT-JACKET-001", "Battle_501 -- scorched collar");
    history.Record(90.0f, "non_damage_event", "MAT-JACKET-001", "Cleaned and pressed");

    float wear = MaterialWearDeriver::DeriveWearState(history, "MAT-JACKET-001");
    // Exactly 2 damage-typed events * 0.05 = 0.10 -- not 3 (the cleaning
    // event correctly excluded), not fabricated.
    DOMINUS_EXPECT(NearlyEqual(wear, 0.10f));
    DOMINUS_EXPECT(MaterialWearDeriver::HasAnyHistory(history, "MAT-JACKET-001"));
}

DOMINUS_TEST(MaterialWearDeriver_WearCapsAtOne) {
    WorldHistory history;
    for (int i = 0; i < 50; ++i) {
        history.Record(static_cast<float>(i), "damage_event", "old_sword", "another hit");
    }
    float wear = MaterialWearDeriver::DeriveWearState(history, "old_sword");
    DOMINUS_EXPECT(NearlyEqual(wear, 1.0f));  // 50 * 0.05 = 2.5, capped to 1.0
}

DOMINUS_TEST(MaterialWearDeriver_DifferentMaterialsGetIndependentWear) {
    WorldHistory history;
    history.Record(0.0f, "damage_event", "jacket", "hit 1");
    history.Record(1.0f, "damage_event", "jacket", "hit 2");
    history.Record(2.0f, "damage_event", "sword", "hit 1");

    float jacketWear = MaterialWearDeriver::DeriveWearState(history, "jacket");
    float swordWear = MaterialWearDeriver::DeriveWearState(history, "sword");
    DOMINUS_EXPECT(NearlyEqual(jacketWear, 0.10f));
    DOMINUS_EXPECT(NearlyEqual(swordWear, 0.05f));
}

// --- RigBinder integration: MaterialGenome now binds to a real entity -------

DOMINUS_TEST(RigBinder_ResolvesMaterialGenomeRefIntoBoundComponent) {
    auto fixtureDir = FixtureDir();
    auto loadResult = dominus::core::DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto bindResult = dominus::character::RigBinder::Bind(*loadResult.value, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* materialComponent = loadResult.value->GetComponent<dominus::character::MaterialGenomeComponent>();
    DOMINUS_EXPECT(materialComponent != nullptr);
    DOMINUS_EXPECT(materialComponent->genome.material_id == "MAT-JACKET-001");
    DOMINUS_EXPECT(materialComponent->genome.identity.type == "urban_leather");
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenMaterialGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    dominus::core::MetaBinObject obj("broken_material_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::MaterialGenomeRefComponent>(
        dominus::core::MaterialGenomeRefComponent{"broken_material_genome.json"});

    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("material genome") != std::string::npos);
}
