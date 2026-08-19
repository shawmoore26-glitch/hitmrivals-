// GRAPHICS/Renderer/Camera.h
// A 2D camera: position, zoom, rotation. Nothing else -- no projection
// matrix, no frustum, no 3D concept anywhere (this engine is
// Transform2D-based throughout, see ANIMATION/SkeletonSystem/
// Transform2D.h's own header comment for why).
#pragma once

#include <cmath>

#include "ANIMATION/SkeletonSystem/Transform2D.h"

namespace dominus::graphics {

struct Camera {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
    float rotation_deg = 0.0f;
};

// Transforms a world-space Transform2D into camera (screen) space:
// translate relative to the camera, rotate by the camera's own
// negative rotation, scale by zoom. Same rotate-then-scale math
// discipline ANIMATION::ComposeWorld already uses -- not a new,
// inconsistent convention.
inline animation::Transform2D ToCameraSpace(const Camera& camera, const animation::Transform2D& world) {
    float dx = world.x - camera.x;
    float dy = world.y - camera.y;

    float rad = -camera.rotation_deg * 3.14159265358979323846f / 180.0f;
    float cosR = std::cos(rad);
    float sinR = std::sin(rad);

    animation::Transform2D screen;
    screen.x = (dx * cosR - dy * sinR) * camera.zoom;
    screen.y = (dx * sinR + dy * cosR) * camera.zoom;
    screen.rotation_deg = world.rotation_deg - camera.rotation_deg;
    screen.scale_x = world.scale_x * camera.zoom;
    screen.scale_y = world.scale_y * camera.zoom;
    return screen;
}

}  // namespace dominus::graphics
