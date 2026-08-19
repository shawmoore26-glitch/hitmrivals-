// COMBAT/Profiles.h
// LAW C010, expanded: a transformation changes every gameplay layer, not
// just genome/graph/moves. These are the remaining layers as real,
// swappable data. PhysicsProfile/AudioProfile/VisualProfile/CameraProfile
// are loaded and attached for real but have no downstream consumer yet
// (no physics/audio/render/camera system exists in this engine) -- that's
// flagged honestly in ROADMAP.md, not hidden. AIProfile is the one profile
// with a real consumer: AI/Agents/CombatAI reads it to bias genome weights
// (see CombatAI::ApplyProfile).
#pragma once

#include <string>

namespace dominus::combat {

struct AIProfile {
    std::string behavior_tag;  // free-form label, e.g. "feral", "disciplined"
    float difficulty = 0.5f;   // 0=passive/easy, 1=maximally aggressive/hard
};

struct PhysicsProfile {
    float mass_kg = 68.0f;
    float gravity_scale = 1.0f;
};

struct AudioProfile {
    std::string voice_bank;
    std::string hit_sound_bank;
};

struct VisualProfile {
    std::string material_set;
    std::string vfx_set;
};

struct CameraProfile {
    float fov_bias = 0.0f;
    float shake_intensity = 1.0f;
};

}  // namespace dominus::combat
