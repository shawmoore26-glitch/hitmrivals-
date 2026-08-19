// tests/world/test_world_persistence.cpp
// Society Phase 0's own proof: "Entity exists -> History recorded ->
// World saved -> Creator leaves -> Creator returns -> World continues."
// Writes to a real temp directory on disk and reads it back -- not an
// in-memory simulation of persistence.
#include "WORLD/Core/WorldPersistence.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::core::MetaBinObject;
using dominus::world::Dimension;
using dominus::world::EntitySaveRecord;
using dominus::world::ProjectionType;
using dominus::world::SourceRefComponent;
using dominus::world::SpatialComponent;
using dominus::world::World;
using dominus::world::WorldPersistence;

namespace {
std::filesystem::path TempDir() {
    auto dir = std::filesystem::temp_directory_path() / "dominus_world_state_test";
    std::filesystem::remove_all(dir);  // start clean every test run
    return dir;
}

const EntitySaveRecord* FindRecord(const std::vector<EntitySaveRecord>& records, const std::string& id) {
    for (auto& r : records) {
        if (r.entity_id == id) return &r;
    }
    return nullptr;
}
}  // namespace

DOMINUS_TEST(WorldPersistence_SaveCreatesExpectedDirectoryStructure) {
    auto dir = TempDir();
    World world;
    auto saveResult = WorldPersistence::Save(world, dir);
    DOMINUS_EXPECT(saveResult.ok);

    DOMINUS_EXPECT(std::filesystem::exists(dir / "world.json"));
    DOMINUS_EXPECT(std::filesystem::exists(dir / "entities"));
    DOMINUS_EXPECT(std::filesystem::exists(dir / "history" / "timeline.json"));
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_SavesAndReloadsElapsedTime) {
    auto dir = TempDir();
    World world;
    world.Tick(12.5f);
    world.Tick(3.5f);  // elapsed = 16.0

    WorldPersistence::Save(world, dir);
    auto loadResult = WorldPersistence::Load(dir);

    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.elapsed_seconds > 15.9f);
    DOMINUS_EXPECT(loadResult.elapsed_seconds < 16.1f);
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_SavesAndReloadsEntityWithSourceRefAndSpatial) {
    auto dir = TempDir();
    World world;

    MetaBinObject entity("brooklyn", "0.1.0");
    entity.AddComponent<SourceRefComponent>(SourceRefComponent{"brooklyn.dominus"});
    SpatialComponent spatial;
    spatial.dimension = Dimension::k2_5D;
    spatial.projection = ProjectionType::kSideView;
    spatial.x = 12.5f;
    spatial.y = 3.0f;
    spatial.z = 1.0f;
    entity.AddComponent<SpatialComponent>(spatial);
    world.Entities().CreateEntity(std::move(entity));

    WorldPersistence::Save(world, dir);
    auto loadResult = WorldPersistence::Load(dir);

    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.entities.size() == 1);
    const auto* record = FindRecord(loadResult.entities, "brooklyn");
    DOMINUS_EXPECT(record != nullptr);
    DOMINUS_EXPECT(record->source_ref == "brooklyn.dominus");
    DOMINUS_EXPECT(record->has_spatial);
    DOMINUS_EXPECT(record->spatial.dimension == Dimension::k2_5D);
    DOMINUS_EXPECT(record->spatial.projection == ProjectionType::kSideView);
    DOMINUS_EXPECT(record->spatial.x > 12.4f && record->spatial.x < 12.6f);
    DOMINUS_EXPECT(record->spatial.z.has_value());
    DOMINUS_EXPECT(*record->spatial.z > 0.9f && *record->spatial.z < 1.1f);
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_EntityWithoutSourceRefOrSpatialSavesWithEmptyDefaults) {
    // A bare "Crate"-style entity (no SourceRefComponent, no
    // SpatialComponent) must still round-trip -- source_ref="" and
    // has_spatial=false, not a crash or a skipped entity.
    auto dir = TempDir();
    World world;
    world.Entities().CreateEntity(MetaBinObject("crate_001", "0.1.0"));

    WorldPersistence::Save(world, dir);
    auto loadResult = WorldPersistence::Load(dir);

    DOMINUS_EXPECT(loadResult.ok);
    const auto* record = FindRecord(loadResult.entities, "crate_001");
    DOMINUS_EXPECT(record != nullptr);
    DOMINUS_EXPECT(record->source_ref.empty());
    DOMINUS_EXPECT(!record->has_spatial);
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_SpatialWithoutZReloadsAsNullopt) {
    // 2D entities have no z -- must round-trip as genuinely absent, not
    // zero (SpatialComponent::z is std::optional for exactly this reason).
    auto dir = TempDir();
    World world;
    MetaBinObject entity("strategy_unit", "0.1.0");
    entity.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(5.0f, 6.0f));
    world.Entities().CreateEntity(std::move(entity));

    WorldPersistence::Save(world, dir);
    auto loadResult = WorldPersistence::Load(dir);

    const auto* record = FindRecord(loadResult.entities, "strategy_unit");
    DOMINUS_EXPECT(record != nullptr);
    DOMINUS_EXPECT(record->has_spatial);
    DOMINUS_EXPECT(!record->spatial.z.has_value());
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_MultipleEntitiesAllRoundTrip) {
    auto dir = TempDir();
    World world;
    for (int i = 0; i < 5; ++i) {
        MetaBinObject e("entity_" + std::to_string(i), "0.1.0");
        e.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(static_cast<float>(i), 0.0f));
        world.Entities().CreateEntity(std::move(e));
    }

    WorldPersistence::Save(world, dir);
    auto loadResult = WorldPersistence::Load(dir);

    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.entities.size() == 5);
    for (int i = 0; i < 5; ++i) {
        DOMINUS_EXPECT(FindRecord(loadResult.entities, "entity_" + std::to_string(i)) != nullptr);
    }
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_HistoryEventsRoundTripInOrder) {
    auto dir = TempDir();
    World world;
    world.History().Record(1.0f, "entity_created", "brooklyn", "Brooklyn entered the city");
    world.History().Record(5.0f, "reputation_changed", "brooklyn", "Defeated the gang leader");
    world.History().Record(10.0f, "npc_remembered", "gang_member_04", "Remembers Brooklyn's fight");

    WorldPersistence::Save(world, dir);
    auto loadResult = WorldPersistence::Load(dir);

    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.history.size() == 3);
    DOMINUS_EXPECT(loadResult.history[0].event_type == "entity_created");
    DOMINUS_EXPECT(loadResult.history[1].description == "Defeated the gang leader");
    DOMINUS_EXPECT(loadResult.history[2].entity_id == "gang_member_04");
    // Order preserved -- tick_time strictly increasing across the log.
    DOMINUS_EXPECT(loadResult.history[0].tick_time < loadResult.history[1].tick_time);
    DOMINUS_EXPECT(loadResult.history[1].tick_time < loadResult.history[2].tick_time);
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(WorldPersistence_LoadingNonexistentDirectoryFailsGracefully) {
    auto loadResult = WorldPersistence::Load("/tmp/dominus_this_directory_should_not_exist_12345");
    DOMINUS_EXPECT(!loadResult.ok);
    DOMINUS_EXPECT(!loadResult.error.empty());
}

DOMINUS_TEST(WorldPersistence_TheFullDirective_EntityExistsHistoryRecordedWorldSavedCreatorReturnsWorldContinues) {
    // The exact flow from the directive, as one test: an entity exists,
    // an event about it is recorded, the world is saved, and -- standing
    // in for "creator returns" -- a FRESH process-equivalent (a brand new
    // World, no shared state with the one that saved) loads it back and
    // finds the entity and its history intact.
    auto dir = TempDir();
    {
        World world;
        MetaBinObject brooklyn("brooklyn", "0.1.0");
        brooklyn.AddComponent<SourceRefComponent>(SourceRefComponent{"brooklyn.dominus"});
        brooklyn.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(10.0f, 0.0f));
        world.Entities().CreateEntity(std::move(brooklyn));
        world.History().Record(0.0f, "entity_created", "brooklyn", "Brooklyn enters the city");
        world.Tick(30.0f);
        world.History().Record(world.ElapsedSeconds(), "battle_won", "brooklyn", "Defeated the gang leader");

        auto saveResult = WorldPersistence::Save(world, dir);
        DOMINUS_EXPECT(saveResult.ok);
    }  // `world` goes out of scope entirely -- nothing carries over except the files on disk

    auto loadResult = WorldPersistence::Load(dir);
    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.entities.size() == 1);
    DOMINUS_EXPECT(loadResult.entities[0].entity_id == "brooklyn");
    DOMINUS_EXPECT(loadResult.entities[0].source_ref == "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.entities[0].spatial.x > 9.9f);
    DOMINUS_EXPECT(loadResult.history.size() == 2);
    DOMINUS_EXPECT(loadResult.history[1].description == "Defeated the gang leader");
    DOMINUS_EXPECT(loadResult.elapsed_seconds > 29.9f);

    std::filesystem::remove_all(dir);
}
