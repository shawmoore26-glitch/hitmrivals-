// PHYSICS/PhysicsSystem.h
// Integrates RigidBody forces/velocity into WORLD::SpatialComponent
// position each tick. This is the one place PHYSICS is allowed to depend
// on WORLD (position lives there, per WORLD LAW 001) -- it still never
// depends on CHARACTER/COMBAT/ANIMATION. A falling rock, Brooklyn, and a
// vehicle all move through the exact same integration code; nothing here
// branches on what kind of entity it is.
#pragma once

#include "PHYSICS/RigidBody.h"
#include "WORLD/Core/EntityRegistry.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/WorldTick.h"

namespace dominus::physics {

class PhysicsSystem {
public:
    explicit PhysicsSystem(float gravityY = -50.0f) : gravityY_(gravityY) {}

    void Integrate(world::EntityRegistry& registry, float dt) const {
        IntegrateWithGravity(registry, dt, gravityY_);
    }

    // Hands this system to a WorldTick as a WorldSystemFn -- the actual
    // seam that makes PHYSICS a plugin WORLD ticks, not a built-in WORLD
    // feature, same pattern as COMBAT's registration in Phase 4.0.
    //
    // LIFETIME AUDIT (Module 5A closeout, following the HitmFighterRuntime
    // WorldTick lifetime fix): the original version of this function
    // returned `[this](EntityRegistry&, float dt) { Integrate(registry,
    // dt); }` -- a closure capturing a raw `PhysicsSystem*`. That is
    // exactly the same shape of hazard the HitmFighterRuntime bug was:
    // nothing in PhysicsSystem's public interface stops a caller from
    // moving, reassigning, or destroying the `PhysicsSystem` object after
    // registering the closure into a WorldTick, e.g.
    //   PhysicsSystem physics(gravity);
    //   world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
    //   PhysicsSystem moved = std::move(physics);  // or `physics` merely
    //                                               // goes out of scope
    //   world.Tick(dt);  // UB: reads through a dangling `this`
    // No call site in this codebase (dominus_cli.cpp, test_physics_system
    // .cpp, test_universal_physics_proof.cpp) happens to do that today --
    // every one of them keeps `physics` alive, unmoved, for the entire
    // scope the World ticks in. But "no current caller exploits it" is
    // not a lifetime proof, it is the same false confidence the
    // HitmFighterRuntime bug hid behind before a move finally triggered
    // it. PhysicsSystem is a general-purpose, publicly reusable type
    // (unlike HitmFighterRuntime::FrameState, which is private and never
    // exposed), so restricting *how future callers may use it* is not an
    // acceptable fix here either.
    //
    // The actual fix: PhysicsSystem's entire runtime state is one float
    // (`gravityY_`) -- there is no mutable, multi-field, movement-graph-
    // sized state here that would justify a heap-allocated pImpl block
    // the way HitmFighterRuntime::FrameState did. So the closure below
    // captures `gravityY_` BY VALUE and calls the shared static
    // `IntegrateWithGravity` helper directly, instead of capturing `this`
    // and calling back through the (possibly relocated or destroyed)
    // PhysicsSystem object. The returned WorldSystemFn owns a private
    // copy of the only state it needs -- it has zero pointer/reference
    // dependency on the PhysicsSystem instance that produced it, so
    // there is nothing left for a move, reassignment, or destruction of
    // that instance to invalidate. This is a stronger guarantee than
    // "provably safe under the current ownership model": it is safe
    // under *every* ownership model, because the hazard's precondition
    // (a callback reading through the original object) no longer exists.
    world::WorldSystemFn AsWorldSystem() const {
        float gravity = gravityY_;
        return [gravity](world::EntityRegistry& registry, float dt) {
            IntegrateWithGravity(registry, dt, gravity);
        };
    }

    float GravityY() const { return gravityY_; }

private:
    static void IntegrateWithGravity(world::EntityRegistry& registry, float dt, float gravityY) {
        for (auto* entity : registry.WithComponent<RigidBody>()) {
            auto* body = entity->GetComponent<RigidBody>();
            auto* spatial = entity->GetComponent<world::SpatialComponent>();
            if (!body || !spatial || body->is_static) continue;

            float forceX = body->force_x;
            float forceY = body->force_y;
            if (body->affected_by_gravity) forceY += gravityY * body->mass;

            float invMass = body->mass > 0.0f ? 1.0f / body->mass : 0.0f;
            body->velocity_x += forceX * invMass * dt;
            body->velocity_y += forceY * invMass * dt;

            spatial->x += body->velocity_x * dt;
            spatial->y += body->velocity_y * dt;

            // Forces are per-tick accumulators (matches how gravity/
            // collision response/a punch's knockback all add to this) --
            // cleared after being applied, same convention as
            // COMBAT/AnimeSpeedSystem's SpeedSimulator.
            body->force_x = 0.0f;
            body->force_y = 0.0f;
        }
    }

    float gravityY_;
};

}  // namespace dominus::physics
