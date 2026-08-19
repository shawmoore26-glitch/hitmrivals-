// ANIMATION/ProceduralMotion/ProceduralHooks.h
// A pipeline of pose modifiers that run AFTER the graph/layers/IK stages
// have produced a pose, each free to nudge specific bones for effects that
// don't belong in authored clips or IK targets -- breathing sway, look-at,
// secondary jiggle, screen-space stabilization, etc. Hooks receive the
// skeleton (for name->index lookups) and the elapsed time so time-based
// effects (like a sine-wave breathing offset) don't need their own clock.
#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"  // for Pose
#include "ANIMATION/SkeletonSystem/Skeleton.h"

namespace dominus::animation {

using PoseModifier = std::function<void(Pose& pose, const Skeleton& skeleton, float elapsedTime)>;

class ProceduralHookStack {
public:
    void AddHook(std::string name, PoseModifier modifier) {
        hooks_.push_back(Hook{std::move(name), std::move(modifier)});
    }

    size_t HookCount() const { return hooks_.size(); }

    // Runs every registered hook in registration order, each mutating
    // `pose` in place. Order matters -- a later hook sees an earlier hook's
    // result, same as animation layers compositing onto a running pose.
    void Apply(Pose& pose, const Skeleton& skeleton, float elapsedTime) const {
        for (const auto& hook : hooks_) {
            hook.modifier(pose, skeleton, elapsedTime);
        }
    }

    // --- Reference hook implementations -------------------------------
    // These are concrete, usable modifiers, not just an empty interface --
    // "procedural animation hooks" as a requirement means something can
    // actually be plugged in, so at least one real example ships with the
    // mechanism.

    // Sine-wave breathing offset on a named bone (typically the torso).
    static PoseModifier MakeBreathingHook(const std::string& boneName, float amplitude, float frequencyHz) {
        return [boneName, amplitude, frequencyHz](Pose& pose, const Skeleton& skeleton, float elapsedTime) {
            auto idx = skeleton.FindBoneIndex(boneName);
            if (!idx) return;
            float offset = amplitude * std::sin(2.0f * 3.14159265358979323846f * frequencyHz * elapsedTime);
            pose[*idx].y += offset;
        };
    }

    // Rotates a bone to face a world-space point, ignoring its parent's
    // current rotation contribution (a simple look-at, not a full
    // constraint solver -- e.g. head tracking).
    static PoseModifier MakeLookAtHook(const std::string& boneName, float targetX, float targetY) {
        return [boneName, targetX, targetY](Pose& pose, const Skeleton& skeleton, float /*elapsedTime*/) {
            auto idx = skeleton.FindBoneIndex(boneName);
            if (!idx) return;
            float dx = targetX - pose[*idx].x;
            float dy = targetY - pose[*idx].y;
            constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
            pose[*idx].rotation_deg = std::atan2(dy, dx) * kRadToDeg;
        };
    }

private:
    struct Hook {
        std::string name;
        PoseModifier modifier;
    };
    std::vector<Hook> hooks_;
};

}  // namespace dominus::animation
