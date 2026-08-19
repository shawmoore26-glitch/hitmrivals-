// ANIMATION/Retargeting/RetargetMap.h
// Foundation-level retargeting: a name-only mapping from a target
// skeleton's bone names to a source skeleton's bone names, used to copy an
// AnimationClip authored for one skeleton onto a differently-named
// skeleton with the same topology. This does NOT solve proportional
// rescaling (different bone lengths between source and target), pose
// re-projection, or topology mismatches (extra/missing bones) -- those are
// tracked upgrades once a rig actually needs them (flagged honestly in
// ROADMAP.md rather than silently assumed away).
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dominus::animation {

class RetargetMap {
public:
    void AddMapping(const std::string& target_bone, const std::string& source_bone) {
        targetToSource_[target_bone] = source_bone;
    }

    std::optional<std::string> SourceBoneFor(const std::string& targetBone) const {
        auto it = targetToSource_.find(targetBone);
        if (it == targetToSource_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<std::string> AllTargetBones() const {
        std::vector<std::string> bones;
        bones.reserve(targetToSource_.size());
        for (auto& [target, source] : targetToSource_) bones.push_back(target);
        return bones;
    }

    size_t MappingCount() const { return targetToSource_.size(); }

private:
    std::unordered_map<std::string, std::string> targetToSource_;
};

}  // namespace dominus::animation
