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

// ---------------------------------------------------------------------
// Lifetime-safety regression tests (Module 5A closeout).
//
// AsWorldSystem() used to return `[this](...) { Integrate(...); }` --
// the exact `this`-capturing hazard shape the HitmFighterRuntime
// WorldTick lifetime bug was. Nothing in PhysicsSystem's public
// interface stopped a caller from moving, reassigning, or destroying
// the PhysicsSystem after registering the closure, which would have
// left the registered WorldSystemFn reading through a dangling
// pointer the next time the World ticked. The fix makes AsWorldSystem()
// capture `gravityY_` BY VALUE and call a static helper -- the closure
// now owns the only state it needs and has zero dependency on the
// PhysicsSystem object's address, so there is nothing left for a move,
// reassignment, or destruction to invalidate. These tests exercise
// exactly the sequences that would have crashed (or silently
// misbehaved) under the old `[this]`-capturing version; run under
// AddressSanitizer they are the credible proof, not just "didn't
// crash".
// ---------------------------------------------------------------------

DOMINUS_TEST(PhysicsSystem_AsWorldSystem_ClosureOutlivesDestroyedSourceObject) {
    World world;
    MetaBinObject rock("rock_001", "0.1.0");
    rock.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 100.0f));
    rock.AddComponent<RigidBody>(RigidBody{});
    world.Entities().CreateEntity(std::move(rock));

    {
        PhysicsSystem physics(-50.0f);
        world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
        // `physics` is destroyed here, at the end of this scope --
        // BEFORE the world is ever ticked. If AsWorldSystem() still
        // captured `this`, every subsequent Tick() would read through a
        // dangling PhysicsSystem*.
    }

    float startY = world.Entities().Find("rock_001")->GetComponent<SpatialComponent>()->y;
    for (int i = 0; i < 30; ++i) world.Tick(1.0f / 60.0f);
    float endY = world.Entities().Find("rock_001")->GetComponent<SpatialComponent>()->y;

    // Real gravity still applied -- not just "didn't crash": the
    // captured gravityY_ value is what actually integrated the fall,
    // proving the closure carries its own state rather than reading
    // through the now-destroyed PhysicsSystem.
    DOMINUS_EXPECT(endY < startY);
}

DOMINUS_TEST(PhysicsSystem_AsWorldSystem_ClosureUnaffectedByMoveThenReuseOfSourceSlot) {
    World world;
    MetaBinObject rock("rock_001", "0.1.0");
    rock.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 100.0f));
    rock.AddComponent<RigidBody>(RigidBody{});
    world.Entities().CreateEntity(std::move(rock));

    PhysicsSystem physics(-50.0f);
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());

    // Move the source object away, then overwrite the original variable
    // with a DIFFERENT, easily distinguished gravity. If AsWorldSystem()
    // still captured `this`, the world would now integrate with
    // whatever ends up living at &physics after this reassignment
    // (0.0f -- no fall at all), not the -50.0f that was actually
    // captured at AsWorldSystem() call time.
    PhysicsSystem relocated(std::move(physics));
    physics = PhysicsSystem(0.0f);
    (void)relocated;

    float startY = world.Entities().Find("rock_001")->GetComponent<SpatialComponent>()->y;
    for (int i = 0; i < 30; ++i) world.Tick(1.0f / 60.0f);
    float endY = world.Entities().Find("rock_001")->GetComponent<SpatialComponent>()->y;

    // Still falls under the original -50.0f -- the closure's own
    // captured copy, unaffected by the move and the subsequent
    // in-place reassignment of the variable that produced it.
    DOMINUS_EXPECT(endY < startY);
}

DOMINUS_TEST(PhysicsSystem_AsWorldSystem_EachClosureKeepsItsOwnGravityIndependently) {
    World worldA;
    World worldB;
    MetaBinObject rockA("rockA", "0.1.0");
    rockA.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 100.0f));
    rockA.AddComponent<RigidBody>(RigidBody{});
    worldA.Entities().CreateEntity(std::move(rockA));

    MetaBinObject rockB("rockB", "0.1.0");
    rockB.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 100.0f));
    rockB.AddComponent<RigidBody>(RigidBody{});
    worldB.Entities().CreateEntity(std::move(rockB));

    {
        PhysicsSystem strongGravity(-100.0f);
        PhysicsSystem weakGravity(-5.0f);
        worldA.Systems().RegisterSystem("physics", strongGravity.AsWorldSystem());
        worldB.Systems().RegisterSystem("physics", weakGravity.AsWorldSystem());
        // Both source objects destroyed here, before either world ticks.
    }

    for (int i = 0; i < 30; ++i) {
        worldA.Tick(1.0f / 60.0f);
        worldB.Tick(1.0f / 60.0f);
    }

    float fallA = 100.0f - worldA.Entities().Find("rockA")->GetComponent<SpatialComponent>()->y;
    float fallB = 100.0f - worldB.Entities().Find("rockB")->GetComponent<SpatialComponent>()->y;

    // Each world's rock fell under its OWN captured gravity value, not
    // a value aliased from (or overwritten by) the other now-destroyed
    // PhysicsSystem -- proves the two closures don't share any state.
    DOMINUS_EXPECT(fallA > fallB);
}
