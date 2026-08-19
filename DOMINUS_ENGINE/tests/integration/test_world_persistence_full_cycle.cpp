// tests/integration/test_world_persistence_full_cycle.cpp
// The complete Society Phase 0 loop, with a REAL fighter: Brooklyn is
// loaded, bound (skeleton/combat/motion graph), inserted into a World,
// given a history event, and saved. That World is destroyed entirely.
// A brand new World reads the save back and -- using ONLY the
// SourceRefComponent path WorldPersistence preserved -- re-runs the
// EXISTING DominusSerializer::Load + RigBinder::Bind + CombatBinder::Bind
// pipeline to reconstruct a fully bound, fully combat-capable Brooklyn
// again. This is external orchestration code (this test file), same
// pattern as every other place WORLD meets COMBAT/CHARACTER -- not
// something WorldPersistence itself does.
#include "AI/Agents/CombatAI.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "WORLD/Core/WorldPersistence.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::ai::CombatAI;
using dominus::ai::OpponentPatternTracker;
using dominus::character::GenomeDecoder;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatIdentityComponent;
using dominus::combat::MoveSetComponent;
using dominus::core::DominusSerializer;
using dominus::core::MetaBinObject;
using dominus::world::SourceRefComponent;
using dominus::world::SpatialComponent;
using dominus::world::World;
using dominus::world::WorldPersistence;

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

std::filesystem::path TempDir() {
    auto dir = std::filesystem::temp_directory_path() / "dominus_full_cycle_test";
    std::filesystem::remove_all(dir);
    return dir;
}
}  // namespace

DOMINUS_TEST(FullCycle_BrooklynSurvivesASaveAndReloadAsAFullyBoundCombatEntity) {
    auto fixtureDir = FixtureDir();
    auto stateDir = TempDir();

    // --- "Day 1": Brooklyn exists, fights, the world is saved ---
    {
        auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
        DOMINUS_EXPECT(loadResult.ok);
        auto& brooklyn = *loadResult.value;
        DOMINUS_EXPECT(RigBinder::Bind(brooklyn, fixtureDir).ok);
        DOMINUS_EXPECT(CombatBinder::Bind(brooklyn, fixtureDir).ok);

        brooklyn.AddComponent<SourceRefComponent>(SourceRefComponent{"brooklyn.dominus"});
        brooklyn.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(42.0f, 0.0f));

        World world;
        world.Entities().CreateEntity(std::move(brooklyn));
        world.History().Record(0.0f, "entity_created", "brooklyn", "Brooklyn enters the city");
        world.Tick(60.0f);
        world.History().Record(world.ElapsedSeconds(), "battle_won", "brooklyn", "Defeated the gang leader");

        auto saveResult = WorldPersistence::Save(world, stateDir);
        DOMINUS_EXPECT(saveResult.ok);
    }  // World fully destroyed here -- "creator leaves"

    // --- "Day 2": creator returns, world continues ---
    auto loadResult = WorldPersistence::Load(stateDir);
    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.entities.size() == 1);
    DOMINUS_EXPECT(loadResult.history.size() == 2);

    World reloadedWorld;
    reloadedWorld.SetElapsedSeconds(loadResult.elapsed_seconds);
    for (auto& record : loadResult.entities) {
        if (record.source_ref.empty()) continue;  // nothing to rebind from

        auto rebindLoad = DominusSerializer::Load(fixtureDir / record.source_ref);
        DOMINUS_EXPECT(rebindLoad.ok);
        auto& entity = *rebindLoad.value;
        DOMINUS_EXPECT(RigBinder::Bind(entity, fixtureDir).ok);
        DOMINUS_EXPECT(CombatBinder::Bind(entity, fixtureDir).ok);

        if (record.has_spatial) entity.AddComponent<SpatialComponent>(record.spatial);
        entity.AddComponent<SourceRefComponent>(SourceRefComponent{record.source_ref});
        reloadedWorld.Entities().CreateEntity(std::move(entity));
    }
    for (auto& event : loadResult.history) {
        reloadedWorld.History().Record(event.tick_time, event.event_type, event.entity_id, event.description);
    }

    // The world continues: elapsed time survived, history survived,
    // position survived.
    DOMINUS_EXPECT(reloadedWorld.ElapsedSeconds() > 59.9f);
    DOMINUS_EXPECT(reloadedWorld.History().Count() == 2);

    auto* reloadedBrooklyn = reloadedWorld.Entities().Find("brooklyn");
    DOMINUS_EXPECT(reloadedBrooklyn != nullptr);
    auto* spatial = reloadedBrooklyn->GetComponent<SpatialComponent>();
    DOMINUS_EXPECT(spatial != nullptr);
    DOMINUS_EXPECT(spatial->x > 41.9f);

    // Not just data -- genuinely combat-capable again: real CombatIdentity,
    // real moves, a live MotionGraphEvaluator that actually accepts a
    // real move request. This is "the world continues," proven by
    // running the exact same combat pipeline Phase 3-3.9 already proved,
    // now on a rebuilt-from-disk entity.
    auto* identity = reloadedBrooklyn->GetComponent<CombatIdentityComponent>();
    auto* moves = reloadedBrooklyn->GetComponent<MoveSetComponent>();
    DOMINUS_EXPECT(identity != nullptr);
    DOMINUS_EXPECT(moves != nullptr);
    DOMINUS_EXPECT(identity->identity.style == "psycho_drunken_martial_arts");

    auto weights = GenomeDecoder::Decode(identity->identity);
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("jab");
    tracker.RecordMove("jab");
    CombatAI ai(tracker, weights);
    // Brooklyn's genome (counter_bias ~0.9) correctly decides to counter
    // a repeated jab pattern -- same behavior Phase 3.9 already proved,
    // now on an entity rebuilt entirely from disk.
    std::string chosen = ai.DecideMoveName(*moves, {"jab"});
    DOMINUS_EXPECT(chosen == "counter");

    auto evaluator = MakeMotionGraphEvaluator(*reloadedBrooklyn);
    DOMINUS_EXPECT(evaluator != nullptr);
    CombatController controller(*evaluator, *moves);
    bool started = controller.StartMove(chosen);
    DOMINUS_EXPECT(started);  // the reloaded entity can genuinely fight again

    std::filesystem::remove_all(stateDir);
}
