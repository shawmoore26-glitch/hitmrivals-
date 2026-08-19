// ANIMATION/SkeletonSystem/AnimationClip.h
// A clip is a set of per-bone keyframe tracks. Sampling a track at time t
// linearly interpolates between the surrounding keyframes -- easing curves,
// per-key tangents, and non-linear interpolation are a tracked upgrade for
// when hitm-animation-director's motion specs actually call for them, not
// a v0.1 requirement.
#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ANIMATION/SkeletonSystem/Transform2D.h"

namespace dominus::animation {

struct Keyframe {
    float time = 0.0f;
    Transform2D pose;
};

struct BoneTrack {
    std::string bone_name;
    std::vector<Keyframe> keyframes;  // must be sorted by time ascending
};

class AnimationClip {
public:
    std::string name;
    float duration = 0.0f;
    bool loop = true;

    void AddTrack(BoneTrack track) {
        std::sort(track.keyframes.begin(), track.keyframes.end(),
                  [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
        tracks_[track.bone_name] = std::move(track);
    }

    const BoneTrack* FindTrack(const std::string& boneName) const {
        auto it = tracks_.find(boneName);
        return it == tracks_.end() ? nullptr : &it->second;
    }

    // Samples the local-space pose for `boneName` at `time`. Returns
    // std::nullopt if the clip has no track for that bone (caller should
    // fall back to the skeleton's bind pose for that bone).
    std::optional<Transform2D> Sample(const std::string& boneName, float time) const {
        const BoneTrack* track = FindTrack(boneName);
        if (!track || track->keyframes.empty()) return std::nullopt;

        float t = time;
        if (loop && duration > 0.0f) {
            t = std::fmod(t, duration);
            if (t < 0.0f) t += duration;
        } else {
            t = std::clamp(t, 0.0f, duration);
        }

        const auto& keys = track->keyframes;
        if (keys.size() == 1 || t <= keys.front().time) return keys.front().pose;
        if (t >= keys.back().time) return keys.back().pose;

        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            const Keyframe& a = keys[i];
            const Keyframe& b = keys[i + 1];
            if (t >= a.time && t <= b.time) {
                float span = b.time - a.time;
                float alpha = span > 0.0f ? (t - a.time) / span : 0.0f;
                return Lerp(a.pose, b.pose, alpha);
            }
        }
        return keys.back().pose;  // unreachable in practice; defensive fallback
    }

private:
    std::unordered_map<std::string, BoneTrack> tracks_;
};

}  // namespace dominus::animation
