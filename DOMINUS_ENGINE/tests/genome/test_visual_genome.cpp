// tests/genome/test_visual_genome.cpp
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualMemorySummary.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/VisualGenomeCompiler.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::VisualGenome;
using dominus::character::VisualGenomeLoader;
using dominus::character::VisualMemoryDeriver;
using dominus::registry::CanonicalSerializer;
using dominus::registry::VisualGenomeCompiler;
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

// --- VisualGenomeLoader: strict validation -----------------------------------

DOMINUS_TEST(VisualGenomeLoader_LoadsRealBrooklynVisualGenome) {
    auto result = VisualGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->form.silhouette == "heavy_fighter");
    DOMINUS_EXPECT(result.value->presence.aura == "chaotic");
    DOMINUS_EXPECT(result.value->skin.age_years == 29);
}

DOMINUS_TEST(VisualGenomeLoader_RejectsOutOfRangeValues) {
    // broken_visual_genome.json has roughness=4.5, subsurface=-1.0,
    // age=-5, threat_signature=12.0, emotional_visual_weight=-3.0 --
    // five independent violations, all must be caught.
    auto result = VisualGenomeLoader::LoadFromFile(FixtureDir() / "broken_visual_genome.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("roughness") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("subsurface") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("age_years") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("threat_signature") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("emotional_visual_weight") != std::string::npos);
}

DOMINUS_TEST(VisualGenomeLoader_MissingFileFailsGracefully) {
    auto result = VisualGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_visual.json");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(VisualGenomeLoader_DefaultsAreAllValid) {
    VisualGenome g;  // default-constructed, no fields explicitly set
    auto compileResult = VisualGenomeCompiler::Compile(g);
    DOMINUS_EXPECT(compileResult.ok);  // defaults are all in-range
}

// --- CanonicalSerializer + Compiler: sixth genome type through the pipeline --

DOMINUS_TEST(CanonicalSerializer_VisualGenome_SameValuesProduceSameBytes) {
    auto a = VisualGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual.json");
    auto b = VisualGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeVisualGenome(*a.value) ==
                    CanonicalSerializer::SerializeVisualGenome(*b.value));
}

DOMINUS_TEST(VisualGenomeCompiler_CompilesDeterministically) {
    auto a = VisualGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual.json");
    auto b = VisualGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual.json");
    auto compileA = VisualGenomeCompiler::Compile(*a.value);
    auto compileB = VisualGenomeCompiler::Compile(*b.value);
    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);
    DOMINUS_EXPECT(compileA.hash == compileB.hash);
}

// --- VisualMemoryDeriver: the real WorldHistory connection -------------------

DOMINUS_TEST(VisualMemoryDeriver_EntityWithNoHistoryReportsHasHistoryFalse) {
    WorldHistory history;
    auto summary = VisualMemoryDeriver::Derive(history, "brooklyn");
    DOMINUS_EXPECT(!summary.has_history);
    DOMINUS_EXPECT(summary.event_count == 0);
}

DOMINUS_TEST(VisualMemoryDeriver_RealRecordedEventsProduceARealCount) {
    // Not fabricated numbers -- exactly as many events as were actually
    // recorded, nothing more, nothing invented.
    WorldHistory history;
    history.Record(0.0f, "entity_created", "brooklyn", "Brooklyn enters the city");
    history.Record(30.0f, "battle_won", "brooklyn", "Defeated the gang leader");
    history.Record(60.0f, "battle_won", "brooklyn", "Won another fight");
    history.Record(10.0f, "entity_created", "static", "an unrelated entity's event");  // different entity_id

    auto summary = VisualMemoryDeriver::Derive(history, "brooklyn");
    DOMINUS_EXPECT(summary.has_history);
    DOMINUS_EXPECT(summary.event_count == 3);  // NOT 4 -- the "static" event correctly excluded
    DOMINUS_EXPECT(summary.first_event_time < 0.1f);
    DOMINUS_EXPECT(summary.last_event_time > 59.9f);
}

DOMINUS_TEST(VisualMemoryDeriver_DifferentEntityGetsDifferentSummary) {
    WorldHistory history;
    history.Record(0.0f, "entity_created", "brooklyn", "...");
    history.Record(5.0f, "entity_created", "flare_stalker", "...");
    history.Record(6.0f, "battle_won", "flare_stalker", "...");

    auto brooklynSummary = VisualMemoryDeriver::Derive(history, "brooklyn");
    auto stalkerSummary = VisualMemoryDeriver::Derive(history, "flare_stalker");
    DOMINUS_EXPECT(brooklynSummary.event_count == 1);
    DOMINUS_EXPECT(stalkerSummary.event_count == 2);
}

// --- RigBinder integration: VisualGenome now binds to a real entity ---------

DOMINUS_TEST(RigBinder_ResolvesVisualGenomeRefIntoBoundComponent) {
    // Brooklyn's REAL .dominus fixture now carries a visual_genome ref --
    // this is the actual flagship entity, not a synthetic test object.
    auto fixtureDir = FixtureDir();
    auto loadResult = dominus::core::DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto bindResult = dominus::character::RigBinder::Bind(*loadResult.value, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* visualComponent = loadResult.value->GetComponent<dominus::character::VisualGenomeComponent>();
    DOMINUS_EXPECT(visualComponent != nullptr);
    DOMINUS_EXPECT(visualComponent->genome.form.silhouette == "heavy_fighter");
    DOMINUS_EXPECT(visualComponent->genome.presence.aura == "chaotic");
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenVisualGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    dominus::core::MetaBinObject obj("broken_visual_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::VisualGenomeRefComponent>(
        dominus::core::VisualGenomeRefComponent{"broken_visual_genome.json"});

    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("visual genome") != std::string::npos);
}
