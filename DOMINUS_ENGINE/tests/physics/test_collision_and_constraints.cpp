// tests/physics/test_collision_and_constraints.cpp
#include "PHYSICS/Collider.h"
#include "PHYSICS/CollisionSystem.h"
#include "PHYSICS/ConstraintSolver.h"
#include "PHYSICS/RigidBody.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"
#include "tests/TestFramework.h"

#include <cmath>

using dominus::core::MetaBinObject;
using dominus::physics::Collider;
using dominus::physics::CollisionSystem;
using dominus::physics::ConstraintSolver;
using dominus::physics::DistanceConstraint;
using dominus::physics::RigidBody;
using dominus::world::SpatialComponent;
using dominus::world::World;

namespace {
bool NearlyEqual(float a, float b, float eps = 0.5f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(CollisionSystem_DetectsOverlappingCircles) {
    World world;
    MetaBinObject a("a", "0.1.0");
    a.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    a.AddComponent<Collider>(Collider{});
    world.Entities().CreateEntity(std::move(a));

    MetaBinObject b("b", "0.1.0");
    b.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(1.0f, 0.0f));  // radius 1 each -> overlapping
    b.AddComponent<Collider>(Collider{});
    world.Entities().CreateEntity(std::move(b));

    auto pairs = CollisionSystem::Detect(world.Entities());
    DOMINUS_EXPECT(pairs.size() == 1);
}

DOMINUS_TEST(CollisionSystem_NoDetectionWhenFarApart) {
    World world;
    MetaBinObject a("a", "0.1.0");
    a.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    a.AddComponent<Collider>(Collider{});
    world.Entities().CreateEntity(std::move(a));

    MetaBinObject b("b", "0.1.0");
    b.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(500.0f, 0.0f));
    b.AddComponent<Collider>(Collider{});
    world.Entities().CreateEntity(std::move(b));

    auto pairs = CollisionSystem::Detect(world.Entities());
    DOMINUS_EXPECT(pairs.empty());
}

DOMINUS_TEST(CollisionSystem_LayerMaskFiltersOutNonInteractingPairs) {
    World world;
    MetaBinObject a("a", "0.1.0");
    a.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    Collider colA;
    colA.layer = 1;
    colA.mask = 1;  // only interacts with layer 1
    a.AddComponent<Collider>(colA);
    world.Entities().CreateEntity(std::move(a));

    MetaBinObject b("b", "0.1.0");
    b.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.5f, 0.0f));  // overlapping position
    Collider colB;
    colB.layer = 2;  // not layer 1 -- should not interact with a's mask
    colB.mask = 2;
    b.AddComponent<Collider>(colB);
    world.Entities().CreateEntity(std::move(b));

    auto pairs = CollisionSystem::Detect(world.Entities());
    DOMINUS_EXPECT(pairs.empty());  // overlapping in space, but layers don't interact
}

DOMINUS_TEST(CollisionSystem_ResolveSeparatesOverlappingDynamicBodies) {
    World world;
    MetaBinObject a("a", "0.1.0");
    a.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    a.AddComponent<Collider>(Collider{});
    a.AddComponent<RigidBody>(RigidBody{});
    world.Entities().CreateEntity(std::move(a));

    MetaBinObject b("b", "0.1.0");
    b.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.5f, 0.0f));  // deeply overlapping
    b.AddComponent<Collider>(Collider{});
    b.AddComponent<RigidBody>(RigidBody{});
    world.Entities().CreateEntity(std::move(b));

    auto pairs = CollisionSystem::Detect(world.Entities());
    DOMINUS_EXPECT(!pairs.empty());
    CollisionSystem::Resolve(world.Entities(), pairs);

    float ax = world.Entities().Find("a")->GetComponent<SpatialComponent>()->x;
    float bx = world.Entities().Find("b")->GetComponent<SpatialComponent>()->x;
    DOMINUS_EXPECT(bx - ax > 0.5f);  // pushed further apart than their original 0.5 separation
}

DOMINUS_TEST(CollisionSystem_StaticBodyDoesNotMoveOnResolve) {
    World world;
    MetaBinObject wall("wall", "0.1.0");
    wall.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    wall.AddComponent<Collider>(Collider{});
    RigidBody staticBody;
    staticBody.is_static = true;
    wall.AddComponent<RigidBody>(staticBody);
    world.Entities().CreateEntity(std::move(wall));

    MetaBinObject box("box", "0.1.0");
    box.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.5f, 0.0f));
    box.AddComponent<Collider>(Collider{});
    box.AddComponent<RigidBody>(RigidBody{});
    world.Entities().CreateEntity(std::move(box));

    auto pairs = CollisionSystem::Detect(world.Entities());
    CollisionSystem::Resolve(world.Entities(), pairs);

    auto* wallSpatial = world.Entities().Find("wall")->GetComponent<SpatialComponent>();
    DOMINUS_EXPECT(NearlyEqual(wallSpatial->x, 0.0f, 0.001f));  // never moved
}

DOMINUS_TEST(ConstraintSolver_PullsEntitiesTowardRestLength) {
    World world;
    MetaBinObject a("a", "0.1.0");
    a.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(0.0f, 0.0f));
    world.Entities().CreateEntity(std::move(a));

    MetaBinObject b("b", "0.1.0");
    b.AddComponent<SpatialComponent>(SpatialComponent::Strategy2D(10.0f, 0.0f));  // 10 apart
    world.Entities().CreateEntity(std::move(b));

    std::vector<DistanceConstraint> constraints = {{"a", "b", 2.0f, 1.0f}};  // rest length 2
    for (int i = 0; i < 10; ++i) ConstraintSolver::Solve(world.Entities(), constraints);

    float ax = world.Entities().Find("a")->GetComponent<SpatialComponent>()->x;
    float bx = world.Entities().Find("b")->GetComponent<SpatialComponent>()->x;
    DOMINUS_EXPECT(NearlyEqual(bx - ax, 2.0f, 0.1f));
}
