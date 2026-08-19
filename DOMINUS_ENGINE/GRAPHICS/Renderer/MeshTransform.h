// GRAPHICS/Renderer/MeshTransform.h
// Applies a real Transform2D (translation, ROTATION, scale) to a
// Mesh's local-space vertices, producing real world/screen-space
// positions -- shared, header-only, so RasterDevice (CPU) and
// VulkanFrameRenderer (GPU) apply the EXACT same math and can never
// silently diverge in how they interpret the same real transform data.
//
// Real, disclosed capability change this function introduces: BEFORE
// this file existed, neither renderer read `rotation_deg` at all --
// confirmed by a direct search of both source files before writing
// this (see GRAPHICS/README.md's geometry-milestone record). A
// rotated entity rendered identically to an unrotated one. This
// function is the fix: real rotation, applied identically everywhere.
#pragma once

#include <cmath>
#include <utility>
#include <vector>

#include "ANIMATION/SkeletonSystem/Transform2D.h"
#include "GRAPHICS/Renderer/Mesh.h"

namespace dominus::graphics {

// Matches the historical procedural rectangle's implicit convention
// (half-width = 8 * scale_x, i.e. full width = 16 * scale_x) so
// switching from the ad-hoc rectangle to a real UnitQuad mesh (extent
// 1.0) does not silently shrink or grow everything already rendered
// -- a real, deliberate continuity choice, not an accident.
inline constexpr float kMeshPixelScale = 16.0f;

// A real, minimum visible size -- an entity with scale 0 (or very
// close to it) still renders as a small but real, visible shape
// rather than silently vanishing. Applied to the scale INPUT (before
// rotation), not clamped post-transform, so it composes correctly
// with rotation.
inline constexpr float kMinEffectiveScale = 1.0f / kMeshPixelScale;

inline std::vector<std::pair<float, float>> TransformMeshVertices(const Mesh& mesh,
                                                                    const animation::Transform2D& transform) {
    std::vector<std::pair<float, float>> out;
    out.reserve(mesh.vertices.size());

    const float effectiveScaleX =
        std::abs(transform.scale_x) < kMinEffectiveScale ? kMinEffectiveScale : std::abs(transform.scale_x);
    const float effectiveScaleY =
        std::abs(transform.scale_y) < kMinEffectiveScale ? kMinEffectiveScale : std::abs(transform.scale_y);

    const float rad = transform.rotation_deg * 3.14159265358979323846f / 180.0f;
    const float cosR = std::cos(rad);
    const float sinR = std::sin(rad);

    for (const auto& v : mesh.vertices) {
        const float sx = v.x * effectiveScaleX * kMeshPixelScale;
        const float sy = v.y * effectiveScaleY * kMeshPixelScale;
        const float rx = sx * cosR - sy * sinR;
        const float ry = sx * sinR + sy * cosR;
        out.emplace_back(transform.x + rx, transform.y + ry);
    }
    return out;
}

}  // namespace dominus::graphics
