// COMBAT/HitSystem/MoveDef.h
// LAW C005: every move has measurable frame timing. LAW C006: damage
// attaches to bones via hit volumes, not abstract targets. LAW C007: every
// attack has intent (purpose/risk/followups), not just a name. Frame
// counts are at a nominal 60fps and converted to seconds by callers
// (FramesToSeconds) -- keeping the source data in frames matches how
// fighting-game designers actually author and read frame data.
#pragma once

#include <string>
#include <vector>

namespace dominus::combat {

constexpr float kFramesPerSecond = 60.0f;
inline float FramesToSeconds(int frames) { return static_cast<float>(frames) / kFramesPerSecond; }

struct FrameData {
    int startup = 0;
    int active = 0;
    int recovery = 0;
    int hit_advantage = 0;
    int block_advantage = 0;
    int cancel_window = 0;  // frames after startup during which a cancel is legal

    int TotalFrames() const { return startup + active + recovery; }
};

struct MoveIntent {
    std::string purpose;
    std::string risk;
    std::vector<std::string> followups;
};

// LAW C006: a hitbox is a bone-relative circle (2D, matching the skeleton
// system's own convention), not a disembodied damage trigger.
struct HitboxDef {
    std::string bone;
    float radius = 5.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

struct MoveDef {
    std::string name;
    std::string motion_trigger;  // MotionGraphEvaluator trigger this move fires
    FrameData frames;
    MoveIntent intent;
    std::vector<HitboxDef> hitboxes;
    float power = 10.0f;  // used by ClashSystem (LAW C008) and ReactionSystem
    float speed = 10.0f;
};

}  // namespace dominus::combat
