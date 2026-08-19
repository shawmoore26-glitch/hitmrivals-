// WORLD/Core/SpatialComponent.h
// WORLD LAW 001 — Dimension Independence: the engine never assumes a
// dimension. A HITM Rivals fighter (2.5D, side view, X/Y movement plane,
// Z as a depth layer), an open-world RPG character (3D, third-person,
// full XYZ), and a mobile strategy unit (2D, top-down) are all the same
// component type with different field values -- never three different
// position types the rest of the engine has to special-case.
#pragma once

#include <optional>

namespace dominus::world {

enum class Dimension { k2D, k2_5D, k3D };
enum class ProjectionType { kSideView, kTopDown, kThirdPerson, kFreeCamera };

struct SpatialComponent {
    Dimension dimension = Dimension::k2D;
    ProjectionType projection = ProjectionType::kTopDown;

    float x = 0.0f;
    float y = 0.0f;
    std::optional<float> z;  // absent for pure 2D; a depth layer for 2.5D; a real axis for 3D

    // Convenience factories matching WORLD LAW 001's own worked examples --
    // not because the engine needs three hardcoded presets, but because
    // "what does a 2.5D side-view fighter's spatial setup actually look
    // like" should be one call, not four fields remembered correctly by
    // every caller.
    static SpatialComponent Fighter2_5D(float x, float y, float depthLayer = 0.0f) {
        SpatialComponent s;
        s.dimension = Dimension::k2_5D;
        s.projection = ProjectionType::kSideView;
        s.x = x;
        s.y = y;
        s.z = depthLayer;
        return s;
    }
    static SpatialComponent OpenWorld3D(float x, float y, float z) {
        SpatialComponent s;
        s.dimension = Dimension::k3D;
        s.projection = ProjectionType::kThirdPerson;
        s.x = x;
        s.y = y;
        s.z = z;
        return s;
    }
    static SpatialComponent Strategy2D(float x, float y) {
        SpatialComponent s;
        s.dimension = Dimension::k2D;
        s.projection = ProjectionType::kTopDown;
        s.x = x;
        s.y = y;
        s.z = std::nullopt;
        return s;
    }
};

}  // namespace dominus::world
