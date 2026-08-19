// tests/world/test_hitm_rivals_as_world_entity.cpp
// THE Phase 4.0 milestone: "Create a blank universe that can run a 2D
// fighter... without changing the core engine." This file is the proof.
// It includes WORLD/Core, CHARACTER, and COMBAT headers TOGETHER -- but
// the coupling only goes one direction: this test file (an "extension",
// per the revised architecture) wires COMBAT/CHARACTER logic into a
// WorldSystemFn lambda and hands it to WorldTick. Not one line inside
// WORLD/Core/*.h was touched to make this work -- verified structurally
// by the earlier grep in this phase's build log (WORLD/Core has zero
// COMBAT/CHARACTER/ANIMATION includes) and functionally right here.
#include "AI/Agents/CombatAI.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <unordered_map>

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
using dominus::world::SpatialComponent;
using dominus::world::World;

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

DOMINUS_TEST(HitmRivals_BrooklynLoadsAsAWorldEntityWithSpatialComponent) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& brooklyn = *loadResult.value;

    DOMINUS_EXPECT(RigBinder::Bind(brooklyn, fixtureDir).ok);
    DOMINUS_EXPECT(CombatBinder::Bind(brooklyn, fixtureDir).ok);

    // WORLD LAW 001: Brooklyn is 2.5D, side-view -- exactly the example in
    // the law itself, expressed through the generic SpatialComponent, not
    // a fighting-game-specific position type.
    brooklyn.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(0.0f, 0.0f));

    World world;
    world.Entities().CreateEntity(std::move(brooklyn));

    auto* entity = world.Entities().Find("brooklyn");
    DOMINUS_EXPECT(entity != nullptr);
    auto* spatial = entity->GetComponent<SpatialComponent>();
    DOMINUS_EXPECT(spatial != nullptr);
    DOMINUS_EXPECT(spatial->dimension == dominus::world::Dimension::k2_5D);
    DOMINUS_EXPECT(entity->GetComponent<CombatIdentityComponent>() != nullptr);  // combat data survived the move
}

DOMINUS_TEST(HitmRivals_CombatRunsAsAWorldTickSystemNotABuiltInWorldFeature) {
    // The actual architectural claim: COMBAT is a plugin the WORLD ticks,
    // not a thing WORLD knows how to do. We build the system as a lambda
    // HERE, in test code that already includes COMBAT/CHARACTER -- World
    // itself never does.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& brooklyn = *loadResult.value;
    RigBinder::Bind(brooklyn, fixtureDir);
    CombatBinder::Bind(brooklyn, fixtureDir);
    brooklyn.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(0.0f, 0.0f));

    World world;
    world.Entities().CreateEntity(std::move(brooklyn));

    // Long-lived evaluators/controllers keyed by entity id -- a real World
    // would own this lifecycle more elegantly (Phase 4.1+ concern); for
    // this proof, static storage keyed by id is sufficient and honest.
    static std::unordered_map<std::string, std::unique_ptr<dominus::animation::MotionGraphEvaluator>> evaluators;
    static std::unordered_map<std::string, bool> triggeredOnce;

    world.Systems().RegisterSystem("combat_extension", [](dominus::world::EntityRegistry& registry, float dt) {
        for (auto* entity : registry.WithComponent<CombatIdentityComponent>()) {
            if (evaluators.find(entity->Id()) == evaluators.end()) {
                evaluators[entity->Id()] = MakeMotionGraphEvaluator(*entity);
            }
            auto& evaluator = evaluators[entity->Id()];
            if (!evaluator) continue;

            if (!triggeredOnce[entity->Id()]) {
                evaluator->Trigger("attack");
                triggeredOnce[entity->Id()] = true;
            }
            evaluator->Update(dt);
        }
    });

    DOMINUS_EXPECT(world.Entities().Find("brooklyn")->GetComponent<CombatIdentityComponent>() != nullptr);

    for (int i = 0; i < 50; ++i) world.Tick(0.02f);  // 1.0s of headless simulation

    auto* evaluator = evaluators["brooklyn"].get();
    DOMINUS_EXPECT(evaluator != nullptr);
    // 0.4s is well past attack's 0.1s blend + its own duration -> should
    // have auto-completed back to idle, entirely through WorldTick, with
    // zero rendering and zero WORLD-module knowledge of what "attack" means.
    DOMINUS_EXPECT(evaluator->CurrentState() == "idle");

    evaluators.clear();
    triggeredOnce.clear();
}

DOMINUS_TEST(HitmRivals_MultipleDimensionsCoexistInSameWorld) {
    // The literal claim: "a world that can support 2D, 2.5D, 3D... same
    // engine." Three entities, three dimensions, one World, one registry.
    World world;

    MetaBinObject fighter("fighter_2_5d", "0.1.0");
    fighter.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(0, 0));
    world.Entities().CreateEntity(std::move(fighter));

    MetaBinObject rpgCharacter("rpg_hero_3d", "0.1.0");
    rpgCharacter.AddComponent<SpatialComponent>(SpatialComponent::OpenWorld3D(0, 0, 0));
    world.Entities().CreateEntity(std::move(rpgCharacter));

    MetaBinObject strategyUnit("strategy_unit_2d", "0.1.0");
    strategyUnit.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0, 0));
    world.Entities().CreateEntity(std::move(strategyUnit));

    DOMINUS_EXPECT(world.Entities().Count() == 3);
    auto allSpatial = world.Entities().WithComponent<SpatialComponent>();
    DOMINUS_EXPECT(allSpatial.size() == 3);

    DOMINUS_EXPECT(world.Entities().Find("fighter_2_5d")->GetComponent<SpatialComponent>()->dimension ==
                   dominus::world::Dimension::k2_5D);
    DOMINUS_EXPECT(world.Entities().Find("rpg_hero_3d")->GetComponent<SpatialComponent>()->dimension ==
                   dominus::world::Dimension::k3D);
    DOMINUS_EXPECT(world.Entities().Find("strategy_unit_2d")->GetComponent<SpatialComponent>()->dimension ==
                   dominus::world::Dimension::k2D);
}
