// ANIMATION/Retargeting/Retarget.h
// Produces a new AnimationClip whose tracks are renamed from a source
// skeleton's bone names to a target skeleton's bone names, per a
// RetargetMap. Keyframe values are copied as-is (name remapping only, no
// proportional rescaling) -- this is the "foundation" scope: it proves a
// clip authored against one skeleton can drive a differently-named one with
// the same topology, which is the actual blocker for reusing motion across
// rigs. Scaling for different bone lengths is a tracked follow-up.
#pragma once

#include "ANIMATION/Retargeting/RetargetMap.h"
#include "ANIMATION/SkeletonSystem/AnimationClip.h"

namespace dominus::animation {

class Retarget {
public:
    static AnimationClip ApplyToClip(const AnimationClip& sourceClip, const RetargetMap& map) {
        AnimationClip result;
        result.name = sourceClip.name;
        result.duration = sourceClip.duration;
        result.loop = sourceClip.loop;

        // Walk every target bone the map knows about; if the source clip has
        // a track for that bone's mapped source name, copy it under the
        // target name. Bones with no mapping, or whose mapped source has no
        // track in this clip, simply have no track in the result -- callers
        // fall back to bind pose for them, same as any other unanimated
        // bone (see AnimationPlayer::Sample).
        for (const auto& targetBone : map.AllTargetBones()) {
            auto sourceBoneName = map.SourceBoneFor(targetBone);
            if (!sourceBoneName) continue;
            const BoneTrack* sourceTrack = sourceClip.FindTrack(*sourceBoneName);
            if (!sourceTrack) continue;

            BoneTrack retargeted = *sourceTrack;
            retargeted.bone_name = targetBone;
            result.AddTrack(std::move(retargeted));
        }

        return result;
    }
};

}  // namespace dominus::animation
