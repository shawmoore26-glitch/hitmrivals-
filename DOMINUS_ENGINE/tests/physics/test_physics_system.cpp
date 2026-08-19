// tests/physics/test_physics_system.cpp
#include "PHYSICS/PhysicsSystem.h"
#include "PHYSICS/RigidBody.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"
#include "tests/TestFramework.h"

#include <cmath>

using dominus::core::MetaBinObject;
using dominus::physics::PhysicsSystem;
using dominus::physics::RigidBody;
using dominus::world::SpatialComponent;
using dominus::world::World;

namespace {
bool NearlyEqual(float a, float b, float eps = 0.5f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(PhysicsSystem_GravityAcceleratesFallingBody) {
    World world;
    MetaBinObject rock("rock_001", "0.1.0");
    rock.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 100.0f));
    rock.AddComponent<RigidBody>(RigidBody{});
    world.Entities().CreateEntity(std::move(rock));

    PhysicsSystem physics(-50.0f);
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());

    float startY = world.Entities().Find("rock_001")->GetComponent<SpatialComponent>()->y;
    for (int i = 0; i < 30; ++i) world.Tick(1.0f / 60.0f);  // 0.5s
    float endY = world.Entities().Find("rock_001")->GetComponent<SpatialComponent>()->y;

    DOMINUS_EXPECT(endY < startY);  // fell downward
}

DOMINUS_TEST(PhysicsSystem_StaticBodyNeverMoves) {
    World world;
    MetaBinObject wall("wall_001", "0.1.0");
    wall.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(5.0f, 5.0f));
    RigidBody staticBody;
    staticBody.is_static = true;
    wall.AddComponent<RigidBody>(staticBody);
    world.Entities().CreateEntity(std::move(wall));

    PhysicsSystem physics;
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
    for (int i = 0; i < 60; ++i) world.Tick(1.0f / 60.0f);

    auto* spatial = world.Entities().Find("wall_001")->GetComponent<SpatialComponent>();
    DOMINUS_EXPECT(NearlyEqual(spatial->x, 5.0f, 0.001f));
    DOMINUS_EXPECT(NearlyEqual(spatial->y, 5.0f, 0.001f));
}

DOMINUS_TEST(PhysicsSystem_AppliedForceMovesBodyInForceDirection) {
    World world;
    MetaBinObject box("box_001", "0.1.0");
    box.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    RigidBody body;
    body.affected_by_gravity = false;  // isolate the force effect
    body.force_x = 100.0f;
    box.AddComponent<RigidBody>(body);
    world.Entities().CreateEntity(std::move(box));

    PhysicsSystem physics;
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
    world.Tick(1.0f / 60.0f);  // one tick -- force should have been applied then cleared

    auto* rb = world.Entities().Find("box_001")->GetComponent<RigidBody>();
    DOMINUS_EXPECT(rb->velocity_x > 0.0f);
    DOMINUS_EXPECT(rb->force_x == 0.0f);  // cleared after applying
}

DOMINUS_TEST(PhysicsSystem_HeavierBodyAcceleratesLessUnderSameForce) {
    World world;
    MetaBinObject light("light", "0.1.0");
    light.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    RigidBody lightBody;
    lightBody.mass = 1.0f;
    lightBody.affected_by_gravity = false;
    lightBody.force_x = 100.0f;
    light.AddComponent<RigidBody>(lightBody);
    world.Entities().CreateEntity(std::move(light));

    MetaBinObject heavy("heavy", "0.1.0");
    heavy.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    RigidBody heavyBody;
    heavyBody.mass = 10.0f;
    heavyBody.affected_by_gravity = false;
    heavyBody.force_x = 100.0f;
    heavy.AddComponent<RigidBody>(heavyBody);
    world.Entities().CreateEntity(std::move(heavy));

    PhysicsSystem physics;
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
    world.Tick(1.0f / 60.0f);

    float lightVel = world.Entities().Find("light")->GetComponent<RigidBody>()->velocity_x;
    float heavyVel = world.Entities().Find("heavy")->GetComponent<RigidBody>()->velocity_x;
    DOMINUS_EXPECT(lightVel > heavyVel);
}
