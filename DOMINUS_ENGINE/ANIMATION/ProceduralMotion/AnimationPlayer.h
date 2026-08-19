// ANIMATION/ProceduralMotion/AnimationPlayer.h
// Combines a Skeleton (bind pose + hierarchy) with an AnimationClip
// (per-bone local-space keyframes) to produce a world-space Pose at a given
// time. This is the runtime the CHARACTER/Rig composition layer drives.
// "ProceduralMotion" is the home for this because sampling + hierarchy
// composition is itself a (very simple) procedural evaluation step, not
// because it does anything with root-motion or physics yet -- those are
// tracked Phase 2+ additions once a real gameplay loop needs them.
#pragma once

#include <vector>

#include "ANIMATION/SkeletonSystem/AnimationClip.h"
#include "ANIMATION/SkeletonSystem/Skeleton.h"
#include "ANIMATION/SkeletonSystem/Transform2D.h"

namespace dominus::animation {

using Pose = std::vector<Transform2D>;  // world-space, indexed like Skeleton::Bones()

class AnimationPlayer {
public:
    // Samples `clip` at `time` against `skeleton`'s bind pose (bones with no
    // track in the clip fall back to their bind-pose local transform), then
    // composes world transforms by walking the hierarchy in bone-index
    // order (parent-before-child, guaranteed by SkeletonLoader).
    static Pose Sample(const Skeleton& skeleton, const AnimationClip& clip, float time) {
        const auto& bones = skeleton.Bones();
        Pose world(bones.size());

        for (size_t i = 0; i < bones.size(); ++i) {
            const Bone& bone = bones[i];
            Transform2D local = bone.bind_pose_local;
            if (auto sampled = clip.Sample(bone.name, time)) {
                local = *sampled;
            }

            if (bone.parent_index < 0) {
                world[i] = local;
            } else {
                world[i] = ComposeWorld(world[static_cast<size_t>(bone.parent_index)], local);
            }
        }
        return world;
    }
};

}  // namespace dominus::animation
