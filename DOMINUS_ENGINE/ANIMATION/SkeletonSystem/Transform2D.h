// ANIMATION/SkeletonSystem/Transform2D.h
// HITM CITY's established production pipeline is 2D/Spine skeletal
// animation (see the spine-fighting-game skill) -- Dominus Engine's
// ANIMATION module targets that reality for v0.1/Phase 2 rather than a 3D
// transform stack nothing currently produces content for. The 3D case
// (UE5, via unreal-combat-architect) is a distinct target added when a
// Phase 2+ milestone actually needs it -- Law 4: scalability before
// spectacle, not architecture for workloads that don't exist yet.
#pragma once

#include <cmath>

namespace dominus::animation {

struct Transform2D {
    float x = 0.0f;
    float y = 0.0f;
    float rotation_deg = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
};

inline Transform2D Lerp(const Transform2D& a, const Transform2D& b, float t) {
    return Transform2D{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.rotation_deg + (b.rotation_deg - a.rotation_deg) * t,
        a.scale_x + (b.scale_x - a.scale_x) * t,
        a.scale_y + (b.scale_y - a.scale_y) * t,
    };
}

// Composes a child's local transform under a parent's world transform.
// Deliberately simple (no shear, uniform-ish rotation compose via angle
// addition) -- sufficient for a 2D fighting-game skeleton with a shallow
// bone hierarchy; a full matrix stack is a tracked upgrade if a rig ever
// needs it, not a v0.1 requirement.
inline Transform2D ComposeWorld(const Transform2D& parentWorld, const Transform2D& local) {
    float rad = parentWorld.rotation_deg * 3.14159265358979323846f / 180.0f;
    float cosR = std::cos(rad);
    float sinR = std::sin(rad);

    float scaledX = local.x * parentWorld.scale_x;
    float scaledY = local.y * parentWorld.scale_y;

    Transform2D world;
    world.x = parentWorld.x + (scaledX * cosR - scaledY * sinR);
    world.y = parentWorld.y + (scaledX * sinR + scaledY * cosR);
    world.rotation_deg = parentWorld.rotation_deg + local.rotation_deg;
    world.scale_x = parentWorld.scale_x * local.scale_x;
    world.scale_y = parentWorld.scale_y * local.scale_y;
    return world;
}

}  // namespace dominus::animation
