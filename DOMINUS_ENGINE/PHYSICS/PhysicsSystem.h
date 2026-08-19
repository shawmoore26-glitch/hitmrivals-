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
        for (auto* entity : registry.WithComponent<RigidBody>()) {
            auto* body = entity->GetComponent<RigidBody>();
            auto* spatial = entity->GetComponent<world::SpatialComponent>();
            if (!body || !spatial || body->is_static) continue;

            float forceX = body->force_x;
            float forceY = body->force_y;
            if (body->affected_by_gravity) forceY += gravityY_ * body->mass;

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

    // Hands this system to a WorldTick as a WorldSystemFn -- the actual
    // seam that makes PHYSICS a plugin WORLD ticks, not a built-in WORLD
    // feature, same pattern as COMBAT's registration in Phase 4.0.
    world::WorldSystemFn AsWorldSystem() const {
        return [this](world::EntityRegistry& registry, float dt) { Integrate(registry, dt); };
    }

    float GravityY() const { return gravityY_; }

private:
    float gravityY_;
};

}  // namespace dominus::physics
