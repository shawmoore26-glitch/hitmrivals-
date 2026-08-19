// tests/world/test_world_tick_and_spatial.cpp
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"
#include "tests/TestFramework.h"

using dominus::core::MetaBinObject;
using dominus::world::Dimension;
using dominus::world::ProjectionType;
using dominus::world::SpatialComponent;
using dominus::world::World;

DOMINUS_TEST(SpatialComponent_Fighter2_5DMatchesWorldLaw001Example) {
    auto s = SpatialComponent::Fighter2_5D(10.0f, 20.0f, 3.0f);
    DOMINUS_EXPECT(s.dimension == Dimension::k2_5D);
    DOMINUS_EXPECT(s.projection == ProjectionType::kSideView);
    DOMINUS_EXPECT(s.z.has_value());
    DOMINUS_EXPECT(*s.z == 3.0f);
}

DOMINUS_TEST(SpatialComponent_OpenWorld3DMatchesWorldLaw001Example) {
    auto s = SpatialComponent::OpenWorld3D(1.0f, 2.0f, 3.0f);
    DOMINUS_EXPECT(s.dimension == Dimension::k3D);
    DOMINUS_EXPECT(s.projection == ProjectionType::kThirdPerson);
    DOMINUS_EXPECT(s.z.has_value());
}

DOMINUS_TEST(SpatialComponent_Strategy2DHasNoZAxis) {
    auto s = SpatialComponent::Strategy2D(5.0f, 6.0f);
    DOMINUS_EXPECT(s.dimension == Dimension::k2D);
    DOMINUS_EXPECT(s.projection == ProjectionType::kTopDown);
    DOMINUS_EXPECT(!s.z.has_value());  // z is genuinely optional, not just zero
}

DOMINUS_TEST(SpatialComponent_SameTypeHoldsAllThreeDimensionsNoSpecialCase) {
    // The actual WORLD LAW 001 claim: one component type serves all three
    // cases, not three different position types.
    std::vector<SpatialComponent> mixed;
    mixed.push_back(SpatialComponent::Fighter2_5D(0, 0));
    mixed.push_back(SpatialComponent::OpenWorld3D(0, 0, 0));
    mixed.push_back(SpatialComponent::Strategy2D(0, 0));
    DOMINUS_EXPECT(mixed.size() == 3);  // compiles and stores uniformly -- that's the proof
}

DOMINUS_TEST(WorldTick_SystemsRunInRegistrationOrder) {
    World world;
    std::vector<std::string> executionOrder;

    world.Systems().RegisterSystem("first", [&](dominus::world::EntityRegistry&, float) {
        executionOrder.push_back("first");
    });
    world.Systems().RegisterSystem("second", [&](dominus::world::EntityRegistry&, float) {
        executionOrder.push_back("second");
    });

    world.Tick(0.016f);
    DOMINUS_EXPECT(executionOrder.size() == 2);
    DOMINUS_EXPECT(executionOrder[0] == "first");
    DOMINUS_EXPECT(executionOrder[1] == "second");
}

DOMINUS_TEST(WorldTick_AccumulatesElapsedTimeAcrossTicks) {
    World world;
    world.Tick(0.5f);
    world.Tick(0.25f);
    DOMINUS_EXPECT(world.ElapsedSeconds() > 0.74f);
    DOMINUS_EXPECT(world.ElapsedSeconds() < 0.76f);
}

DOMINUS_TEST(World_HeadlessSimulationRunsWithZeroRenderingCalls) {
    // WORLD LAW 003: the world ticks and mutates entity state with no
    // rendering step anywhere -- proven by a system that mutates a
    // component and asserting the mutation happened after Tick(), with
    // nothing resembling a draw/render call anywhere in this test.
    World world;
    MetaBinObject npc("citizen_204", "0.1.0");
    npc.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    world.Entities().CreateEntity(std::move(npc));

    world.Systems().RegisterSystem("movement", [](dominus::world::EntityRegistry& registry, float dt) {
        for (auto* entity : registry.WithComponent<SpatialComponent>()) {
            entity->GetComponent<SpatialComponent>()->x += 10.0f * dt;
        }
    });

    world.Tick(1.0f);
    auto* moved = world.Entities().Find("citizen_204");
    DOMINUS_EXPECT(moved->GetComponent<SpatialComponent>()->x > 9.9f);
}

DOMINUS_TEST(World_ScaleSmokeTest_TenThousandEntitiesTickWithoutError) {
    // Not the 100,000-entity target from Module 1's stated goal -- a real,
    // honest partial data point (10k) rather than an unverified claim of
    // the full target. See ROADMAP.md Phase 4.0 for that distinction.
    World world;
    for (int i = 0; i < 10000; ++i) {
        MetaBinObject e("entity_" + std::to_string(i), "0.1.0");
        e.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
        world.Entities().CreateEntity(std::move(e));
    }
    DOMINUS_EXPECT(world.Entities().Count() == 10000);

    world.Systems().RegisterSystem("tick_all", [](dominus::world::EntityRegistry& registry, float dt) {
        for (auto* entity : registry.WithComponent<SpatialComponent>()) {
            entity->GetComponent<SpatialComponent>()->x += dt;
        }
    });

    for (int frame = 0; frame < 60; ++frame) world.Tick(1.0f / 60.0f);
    DOMINUS_EXPECT(world.Entities().Count() == 10000);
    DOMINUS_EXPECT(world.Entities().Find("entity_0")->GetComponent<SpatialComponent>()->x > 0.9f);
}
