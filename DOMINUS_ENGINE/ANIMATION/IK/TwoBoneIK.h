// ANIMATION/IK/TwoBoneIK.h
// Analytic (law-of-cosines) two-bone IK solver for a 2D chain: root -> mid
// -> end (e.g. shoulder -> elbow -> wrist, or hip -> knee -> ankle). Given
// the two segment lengths, a root world position, and a target world
// position, returns the local rotation each of the two bones must have so
// the chain's end reaches (or reaches toward, if unreachable) the target.
// This is deliberately the "boring but correct" analytic solution rather
// than an iterative solver (FABRIK/CCD) -- exact for two-bone chains, and a
// two-bone chain is all the current skeleton fixture has. An iterative
// solver for longer chains is a tracked upgrade for when a rig actually has
// one, not a v0.1/Phase-2.5 requirement.
#pragma once

#include <algorithm>
#include <cmath>

namespace dominus::animation {

struct TwoBoneIKResult {
    float root_rotation_deg = 0.0f;  // rotation to apply to the root/upper bone
    float mid_rotation_deg = 0.0f;   // rotation to apply to the mid/lower bone, relative to root
    bool target_reachable = true;    // false if the target was clamped to max reach
};

class TwoBoneIK {
public:
    // root_pos/target_pos are world-space. bend_direction is +1 or -1 and
    // picks which of the two elbow solutions to use (a two-bone chain has
    // two valid configurations for any reachable target -- "elbow up" vs
    // "elbow down"). Bone lengths must both be > 0.
    static TwoBoneIKResult Solve(float root_x, float root_y, float upper_len, float lower_len, float target_x,
                                  float target_y, float bend_direction = 1.0f) {
        TwoBoneIKResult result;

        float dx = target_x - root_x;
        float dy = target_y - root_y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float maxReach = upper_len + lower_len;
        float minReach = std::fabs(upper_len - lower_len);

        if (dist > maxReach) {
            dist = maxReach;
            result.target_reachable = false;
        } else if (dist < minReach) {
            dist = minReach > 0.0f ? minReach : 0.0001f;
            result.target_reachable = false;
        }
        if (dist < 0.0001f) dist = 0.0001f;

        // Angle from root to target, in world space.
        float angleToTarget = std::atan2(dy, dx);

        // Law of cosines: angle at the root between the upper bone and the
        // root-to-target line.
        float cosRootAngle = (upper_len * upper_len + dist * dist - lower_len * lower_len) / (2.0f * upper_len * dist);
        cosRootAngle = std::clamp(cosRootAngle, -1.0f, 1.0f);
        float rootAngleOffset = std::acos(cosRootAngle);

        // Interior angle at the mid joint (elbow).
        float cosMidAngle = (upper_len * upper_len + lower_len * lower_len - dist * dist) / (2.0f * upper_len * lower_len);
        cosMidAngle = std::clamp(cosMidAngle, -1.0f, 1.0f);
        float midInteriorAngle = std::acos(cosMidAngle);

        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

        float sign = bend_direction >= 0.0f ? 1.0f : -1.0f;
        result.root_rotation_deg = (angleToTarget + sign * rootAngleOffset) * kRadToDeg;
        // mid_rotation is relative to the upper bone's own direction, so it's
        // the supplement of the interior angle, signed opposite the root bend.
        result.mid_rotation_deg = -sign * (180.0f - midInteriorAngle * kRadToDeg);

        return result;
    }
};

}  // namespace dominus::animation
