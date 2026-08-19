// COMBAT/CinematicDirector.h
// Combat Event System: Gameplay -> Special Event -> Cinematic Camera ->
// Physics Simulation -> Return Gameplay. This is the state machine for
// that flow -- event definitions (camera trigger name + slowmo scale +
// duration) live in a small lookup table, foundation scope. Actual camera
// rendering and physics staging are downstream concerns for a renderer
// this engine doesn't have yet; this system's job is deciding WHEN a
// cinematic moment happens and WHAT to tell the camera/timescale, which is
// exactly the seam a renderer plugs into later.
#pragma once

#include <string>

namespace dominus::combat {

enum class CombatEventType {
    kFinisher,
    kClash,
    kTransformation,
    kWallImpact,
    kEnvironmentalDestruction,
};

struct CombatEventDef {
    CombatEventType type;
    std::string camera_trigger;
    float slow_motion_scale = 1.0f;  // 1.0 = normal speed, lower = slower
    float duration_seconds = 0.0f;
};

class CinematicDirector {
public:
    // Starts an event, overriding any currently-active one (a finisher
    // interrupting an in-progress clash is a legitimate real scenario, not
    // an error case).
    CombatEventDef Trigger(CombatEventType type) {
        current_ = LookupDef(type);
        remaining_ = current_.duration_seconds;
        active_ = true;
        return current_;
    }

    void Update(float dt) {
        if (!active_) return;
        remaining_ -= dt;
        if (remaining_ <= 0.0f) {
            active_ = false;
            remaining_ = 0.0f;
        }
    }

    bool IsActive() const { return active_; }
    const CombatEventDef* Current() const { return active_ ? &current_ : nullptr; }
    float RemainingSeconds() const { return remaining_; }

    static CombatEventDef LookupDef(CombatEventType type) {
        switch (type) {
            case CombatEventType::kFinisher:
                return {CombatEventType::kFinisher, "finisher_cam", 0.2f, 2.0f};
            case CombatEventType::kClash:
                return {CombatEventType::kClash, "clash_cam", 0.5f, 0.8f};
            case CombatEventType::kTransformation:
                return {CombatEventType::kTransformation, "transform_cam", 0.1f, 3.0f};
            case CombatEventType::kWallImpact:
                return {CombatEventType::kWallImpact, "impact_cam", 0.6f, 0.5f};
            case CombatEventType::kEnvironmentalDestruction:
                return {CombatEventType::kEnvironmentalDestruction, "destruction_cam", 0.7f, 0.6f};
        }
        return {CombatEventType::kFinisher, "finisher_cam", 0.2f, 2.0f};
    }

private:
    bool active_ = false;
    float remaining_ = 0.0f;
    CombatEventDef current_{};
};

}  // namespace dominus::combat
