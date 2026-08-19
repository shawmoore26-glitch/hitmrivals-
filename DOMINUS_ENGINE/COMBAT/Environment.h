// COMBAT/Environment.h
// LAW module 5 (Environmental Combat): walls, floors, destruction zones.
// Foundation scope: an axis-aligned arena boundary standing in for
// "walls"/"floor" (no arbitrary geometry yet), plus circular destruction
// zones. ApplyEnvironment is what actually closes the kWallImpact/
// kGroundImpact gap CombatController's switch statement flagged as
// unhandled in Phase 3 -- it takes a plain ReactionSystem result and
// upgrades it if the predicted trajectory would cross a boundary.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "COMBAT/ReactionSystem/ReactionSystem.h"

namespace dominus::combat {

struct EnvironmentBounds {
    float min_x = -1000.0f;
    float max_x = 1000.0f;
    float min_y = 0.0f;  // floor
    float max_y = 1000.0f;
};

struct DestructionZone {
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 10.0f;
};

class EnvironmentCollision {
public:
    static bool CrossesWallX(const EnvironmentBounds& bounds, float predictedX) {
        return predictedX <= bounds.min_x || predictedX >= bounds.max_x;
    }
    static bool CrossesFloorY(const EnvironmentBounds& bounds, float predictedY) {
        return predictedY <= bounds.min_y;
    }

    static std::optional<DestructionZone> FindZone(const std::vector<DestructionZone>& zones, float x, float y) {
        for (const auto& zone : zones) {
            float dx = x - zone.x;
            float dy = y - zone.y;
            if (dx * dx + dy * dy <= zone.radius * zone.radius) return zone;
        }
        return std::nullopt;
    }

    // Predicts where `input`'s force would carry the defender from
    // (currentX, currentY) over `dt` seconds (simple impulse projection,
    // consistent with COMBAT/AnimeSpeedSystem's integrator), and upgrades
    // the reaction type/trigger if that crosses a wall or floor. Passing
    // through unchanged (same type/trigger) when nothing is crossed --
    // this never invents a wall/ground impact that isn't there.
    static ReactionResult ApplyEnvironment(const ReactionResult& base, const EnvironmentBounds& bounds,
                                            float currentX, float currentY, float dt = 0.5f) {
        float predictedX = currentX + base.force_x * dt;
        float predictedY = currentY + base.force_y * dt;

        ReactionResult upgraded = base;
        if (CrossesFloorY(bounds, predictedY)) {
            upgraded.type = ReactionType::kGroundImpact;
            upgraded.motion_trigger = "ground_impact";
        } else if (CrossesWallX(bounds, predictedX)) {
            upgraded.type = ReactionType::kWallImpact;
            upgraded.motion_trigger = "wall_impact";
        }
        return upgraded;
    }
};

}  // namespace dominus::combat
