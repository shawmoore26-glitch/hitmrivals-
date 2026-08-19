// PHYSICS/RigidBody.h
// LAW (Phase 4.1 directive): "WORLD knows physics exists. Physics does
// not know games exist." RigidBody is pure physical state -- mass,
// velocity, an accumulated force for this tick -- attached to whatever
// entity needs it, the same way for a falling rock, a fighter, or a
// vehicle. Nothing in this file (or anywhere in PHYSICS/) knows what a
// "fighter" or "vehicle" is; that identity lives in whichever component
// an extension (CHARACTER/COMBAT/a future Racing extension) chooses to
// attach alongside this one.
#pragma once

namespace dominus::physics {

struct RigidBody {
    float mass = 1.0f;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;

    // Accumulated this tick, applied and cleared by PhysicsSystem::Integrate
    // -- callers (gravity, collision response, a punch's knockback) add to
    // this rather than setting velocity directly, so multiple force
    // sources compose correctly within one tick.
    float force_x = 0.0f;
    float force_y = 0.0f;

    bool is_static = false;          // immovable (walls, terrain) -- never integrated
    bool affected_by_gravity = true;
};

}  // namespace dominus::physics
