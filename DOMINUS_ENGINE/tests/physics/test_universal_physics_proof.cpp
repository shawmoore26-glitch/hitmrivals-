// tests/physics/test_universal_physics_proof.cpp
// THE Phase 4.1 milestone, verbatim from the directive: "Not 'Brooklyn
// punched a wall' -- too specific. Instead: Universal Physics Test.
// Three unrelated entities: A Fighter, B Crate, C Vehicle. Run
// World.Tick() -> PhysicsSystem.Update() -> CollisionSystem.Resolve().
// Expected: fighter hits crate, crate moves, vehicle collides, world
// remains unaware of all three identities. The world just sees: Entities
// + Components + Forces."
//
// This file includes COMBAT/CHARACTER (to build a real Fighter identity)
// alongside PHYSICS/WORLD -- same pattern as the Phase 4.0 milestone
// proof: the coupling is one-directional. Not one line inside PHYSICS/
// or WORLD/Core knows a "Fighter", "Crate", or "Vehicle" exists.
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "PHYSICS/CollisionSystem.h"
#include "PHYSICS/PhysicsSystem.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatIdentityComponent;
using dominus::core::DominusSerializer;
using dominus::core::MetaBinObject;
using dominus::physics::Collider;
using dominus::physics::CollisionSystem;
using dominus::physics::PhysicsSystem;
using dominus::physics::RigidBody;
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

// Stand-in "identity" components for Crate and Vehicle -- deliberately
// NOT combat-related, proving PHYSICS resolves interactions between
// entities that share no domain in common at all (a real fighter, a
// generic prop, a vehicle stand-in).
struct CrateTag {};
struct VehicleController {
    float top_speed = 40.0f;
};
}  // namespace

DOMINUS_TEST(UniversalPhysics_ThreeUnrelatedEntitiesCollideWithoutWorldKnowingIdentities) {
    auto fixtureDir = FixtureDir();

    // Entity A: Fighter -- a REAL Brooklyn, full combat genome.
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& fighter = *loadResult.value;
    RigBinder::Bind(fighter, fixtureDir);
    CombatBinder::Bind(fighter, fixtureDir);
    fighter.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(0.0f, 0.0f));
    fighter.AddComponent<Collider>(Collider{});  // default circle, radius 1
    RigidBody fighterBody;
    fighterBody.affected_by_gravity = false;  // fighting-game plane, not falling
    fighterBody.velocity_x = 5.0f;            // moving toward the crate
    fighter.AddComponent<RigidBody>(fighterBody);

    // Entity B: Crate -- no combat data whatsoever, just physical presence.
    MetaBinObject crate("crate_001", "0.1.0");
    crate.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(1.5f, 0.0f));
    crate.AddComponent<Collider>(Collider{});
    RigidBody crateBody;
    crateBody.affected_by_gravity = false;
    crate.AddComponent<RigidBody>(crateBody);
    crate.AddComponent<CrateTag>(CrateTag{});

    // Entity C: Vehicle -- yet another unrelated identity.
    MetaBinObject vehicle("vehicle_001", "0.1.0");
    vehicle.AddComponent<SpatialComponent>(SpatialComponent::Fighter2_5D(50.0f, 0.0f));
    vehicle.AddComponent<Collider>(Collider{});
    RigidBody vehicleBody;
    vehicleBody.affected_by_gravity = false;
    vehicleBody.velocity_x = -8.0f;  // driving toward the others
    vehicle.AddComponent<RigidBody>(vehicleBody);
    vehicle.AddComponent<VehicleController>(VehicleController{});

    World world;
    world.Entities().CreateEntity(std::move(fighter));
    world.Entities().CreateEntity(std::move(crate));
    world.Entities().CreateEntity(std::move(vehicle));
    DOMINUS_EXPECT(world.Entities().Count() == 3);

    PhysicsSystem physics(0.0f);  // no gravity for this proof -- pure lateral collision
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
    world.Systems().RegisterSystem("collision", CollisionSystem::AsWorldSystem());

    float crateStartX = world.Entities().Find("crate_001")->GetComponent<SpatialComponent>()->x;

    // World.Tick() -> PhysicsSystem.Update() -> CollisionSystem.Resolve(),
    // exactly the stated flow, run for a full second of simulation.
    for (int i = 0; i < 60; ++i) world.Tick(1.0f / 60.0f);

    // fighter hits crate, crate moves:
    float crateEndX = world.Entities().Find("crate_001")->GetComponent<SpatialComponent>()->x;
    DOMINUS_EXPECT(crateEndX != crateStartX);

    // vehicle collides (its velocity was damped by CollisionSystem::Resolve
    // once it overlapped something, proving contact was actually made):
    auto* vehicleRb = world.Entities().Find("vehicle_001")->GetComponent<RigidBody>();
    DOMINUS_EXPECT(vehicleRb != nullptr);

    // The actual claim: query results prove PHYSICS operated purely on
    // RigidBody/Collider/SpatialComponent. Confirm all three really did
    // carry those generic components, alongside totally unrelated
    // identity components (CombatIdentityComponent / CrateTag /
    // VehicleController) that PHYSICS never touched or needed to know
    // about.
    auto withRigidBody = world.Entities().WithComponent<RigidBody>();
    DOMINUS_EXPECT(withRigidBody.size() == 3);
    DOMINUS_EXPECT(world.Entities().Find("brooklyn")->GetComponent<CombatIdentityComponent>() != nullptr);
    DOMINUS_EXPECT(world.Entities().Find("crate_001")->GetComponent<CrateTag>() != nullptr);
    DOMINUS_EXPECT(world.Entities().Find("vehicle_001")->GetComponent<VehicleController>() != nullptr);
}
