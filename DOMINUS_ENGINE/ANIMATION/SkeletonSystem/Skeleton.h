// ANIMATION/SkeletonSystem/Skeleton.h
// A skeleton is a flat bone array with parent indices (topologically
// ordered -- every bone's parent has a lower index, root has parent -1).
// This is the runtime type that fulfils the SkeletonRefComponent path
// carried by CORE; nothing outside ANIMATION and CHARACTER should need to
// know its internals.
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ANIMATION/SkeletonSystem/Transform2D.h"

namespace dominus::animation {

struct Bone {
    std::string name;
    int parent_index = -1;   // -1 == root
    Transform2D bind_pose_local;
};

class Skeleton {
public:
    void AddBone(Bone bone) {
        nameToIndex_[bone.name] = static_cast<int>(bones_.size());
        bones_.push_back(std::move(bone));
    }

    const std::vector<Bone>& Bones() const { return bones_; }
    size_t BoneCount() const { return bones_.size(); }

    std::optional<int> FindBoneIndex(const std::string& name) const {
        auto it = nameToIndex_.find(name);
        if (it == nameToIndex_.end()) return std::nullopt;
        return it->second;
    }

    // World-space bind pose, computed by walking bones in index order
    // (valid because AddBone enforces parent-before-child insertion by
    // construction of every loader that builds a Skeleton).
    std::vector<Transform2D> ComputeBindPoseWorld() const {
        std::vector<Transform2D> world(bones_.size());
        for (size_t i = 0; i < bones_.size(); ++i) {
            const Bone& b = bones_[i];
            if (b.parent_index < 0) {
                world[i] = b.bind_pose_local;
            } else {
                world[i] = ComposeWorld(world[static_cast<size_t>(b.parent_index)], b.bind_pose_local);
            }
        }
        return world;
    }

private:
    std::vector<Bone> bones_;
    std::unordered_map<std::string, int> nameToIndex_;
};

}  // namespace dominus::animation
