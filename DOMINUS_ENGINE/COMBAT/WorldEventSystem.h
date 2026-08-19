// COMBAT/WorldEventSystem.h
// Wall Impact -> World Event -> Destruction/Particles/Audio/Camera/
// Gameplay. This is the bridge into a future World Engine (Phase 4): a
// WorldEvent is a small, renderer/audio-agnostic descriptor a downstream
// system would consume to actually spawn debris, play a sound, shake the
// camera, etc. Nothing in this engine consumes a WorldEvent yet (no
// destruction/particle/audio system exists) -- this is the seam, not the
// implementation of what's on the other side of it. Deliberately
// standalone (not baked into CombatController) so it composes with
// EnvironmentCollision::ApplyEnvironment and CinematicDirector without
// adding a third responsibility to CombatController's already-tested API.
#pragma once

#include <string>

#include "COMBAT/Environment.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"

namespace dominus::combat {

struct WorldEvent {
    std::string event_type;         // "wall_impact" | "ground_impact" | "environmental_destruction"
    std::string destruction_tag;    // what breaks, e.g. "wall_debris"
    std::string particle_tag;
    std::string audio_tag;
    std::string camera_tag;
    float x = 0.0f;
    float y = 0.0f;
};

class WorldEventSystem {
public:
    // Only produces an event for reactions EnvironmentCollision has
    // already upgraded to kWallImpact/kGroundImpact -- for any other
    // reaction type this returns a default-constructed WorldEvent with an
    // empty event_type, which callers should treat as "no world event".
    static WorldEvent FromReaction(const ReactionResult& result, float x, float y) {
        WorldEvent event;
        event.x = x;
        event.y = y;

        if (result.type == ReactionType::kWallImpact) {
            event.event_type = "wall_impact";
            event.destruction_tag = "wall_debris";
            event.particle_tag = "impact_dust";
            event.audio_tag = "wall_crash_sfx";
            event.camera_tag = "impact_cam";
        } else if (result.type == ReactionType::kGroundImpact) {
            event.event_type = "ground_impact";
            event.destruction_tag = "ground_crater";
            event.particle_tag = "dust_cloud";
            event.audio_tag = "ground_slam_sfx";
            event.camera_tag = "impact_cam";
        }
        return event;
    }

    // A defender colliding with a DestructionZone (crate, pillar, etc.) is
    // its own event path -- it doesn't come from ReactionSystem at all,
    // it comes from EnvironmentCollision::FindZone.
    static WorldEvent FromDestructionZone(const DestructionZone& zone, float x, float y) {
        WorldEvent event;
        event.event_type = "environmental_destruction";
        event.destruction_tag = zone.name + "_break";
        event.particle_tag = "debris_burst";
        event.audio_tag = "destruction_sfx";
        event.camera_tag = "destruction_cam";
        event.x = x;
        event.y = y;
        return event;
    }

    static bool IsRealEvent(const WorldEvent& event) { return !event.event_type.empty(); }
};

}  // namespace dominus::combat
