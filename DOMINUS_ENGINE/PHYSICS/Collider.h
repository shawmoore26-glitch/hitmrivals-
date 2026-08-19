// PHYSICS/Collider.h
// Foundation scope: circle and box shapes, described generically enough
// to cover a punch's hitbox, a crate, or a vehicle's bounding box --
// again, no identity, just geometry. Collision layers/masks are the
// standard bitwise filter (a layer this collider IS, a mask of layers it
// COLLIDES WITH) so, e.g., projectiles-vs-terrain and fighter-vs-fighter
// can be filtered independently without PHYSICS knowing what any layer
// "means" -- that meaning lives entirely in whichever extension assigns
// the bits.
#pragma once

#include <cstdint>

namespace dominus::physics {

enum class ColliderShape { kCircle, kBox };

constexpr uint32_t kAllLayers = 0xFFFFFFFFu;
constexpr uint32_t kDefaultLayer = 1u;

struct Collider {
    ColliderShape shape = ColliderShape::kCircle;
    float radius = 1.0f;        // used when shape == kCircle
    float half_width = 1.0f;    // used when shape == kBox
    float half_height = 1.0f;   // used when shape == kBox

    uint32_t layer = kDefaultLayer;  // what this collider IS
    uint32_t mask = kAllLayers;      // what this collider COLLIDES WITH

    static bool LayersInteract(const Collider& a, const Collider& b) {
        return (a.mask & b.layer) != 0 && (b.mask & a.layer) != 0;
    }
};

}  // namespace dominus::physics
