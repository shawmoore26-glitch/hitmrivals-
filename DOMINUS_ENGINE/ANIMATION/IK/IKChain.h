// ANIMATION/IK/IKChain.h
// An IKChainDef names which three bones in a skeleton form a two-bone chain
// (root/mid/end -- e.g. shoulder/elbow/wrist) and which elbow solution to
// prefer. IKChainSolver::Apply uses TwoBoneIK plus the skeleton's bind-pose
// segment lengths to bend that chain toward a world-space target, mutating
// a Pose in place.
#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

#include "ANIMATION/IK/TwoBoneIK.h"
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"  // for Pose
#include "ANIMATION/SkeletonSystem/Skeleton.h"

namespace dominus::animation {

struct IKChainDef {
    std::string name;
    std::string root_bone;
    std::string mid_bone;
    std::string end_bone;
    float bend_direction = 1.0f;
};

struct IKApplyResult {
    bool reachable = true;
};

class IKChainSolver {
public:
    // Bends `pose` (already computed by animation/graph/layers) so the
    // chain named in `chain` reaches toward (target_x, target_y). The
    // chain's root bone keeps its existing world position from `pose` --
    // only its rotation, and its descendants' resulting transforms, change.
    static IKApplyResult Apply(const Skeleton& skeleton, const IKChainDef& chain, float target_x, float target_y,
                                Pose& pose) {
        auto rootIdx = skeleton.FindBoneIndex(chain.root_bone);
        auto midIdx = skeleton.FindBoneIndex(chain.mid_bone);
        auto endIdx = skeleton.FindBoneIndex(chain.end_bone);
        if (!rootIdx || !midIdx || !endIdx) {
            throw std::runtime_error("IKChainSolver: chain '" + chain.name + "' references unknown bone(s)");
        }

        // Segment lengths come from the skeleton's own bind pose -- the
        // chain definition never hardcodes lengths, so re-rigging a
        // character (different arm proportions) doesn't require touching
        // the IK chain file.
        auto bindWorld = skeleton.ComputeBindPoseWorld();
        float upperLen = Distance(bindWorld[*rootIdx], bindWorld[*midIdx]);
        float lowerLen = Distance(bindWorld[*midIdx], bindWorld[*endIdx]);

        Transform2D rootWorld = pose[*rootIdx];
        TwoBoneIKResult ik = TwoBoneIK::Solve(rootWorld.x, rootWorld.y, upperLen, lowerLen, target_x, target_y,
                                               chain.bend_direction);

        rootWorld.rotation_deg = ik.root_rotation_deg;
        pose[*rootIdx] = rootWorld;

        const Bone& midBone = skeleton.Bones()[*midIdx];
        Transform2D midLocalOffsetOnly = midBone.bind_pose_local;
        midLocalOffsetOnly.rotation_deg = 0.0f;  // IK overrides rotation explicitly below
        Transform2D midWorld = ComposeWorld(rootWorld, midLocalOffsetOnly);
        midWorld.rotation_deg = rootWorld.rotation_deg + ik.mid_rotation_deg;
        pose[*midIdx] = midWorld;

        const Bone& endBone = skeleton.Bones()[*endIdx];
        Transform2D endLocalOffsetOnly = endBone.bind_pose_local;
        endLocalOffsetOnly.rotation_deg = 0.0f;
        pose[*endIdx] = ComposeWorld(midWorld, endLocalOffsetOnly);

        return IKApplyResult{ik.target_reachable};
    }

private:
    static float Distance(const Transform2D& a, const Transform2D& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

}  // namespace dominus::animation
