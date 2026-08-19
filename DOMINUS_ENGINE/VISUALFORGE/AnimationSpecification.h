// VISUALFORGE/AnimationSpecification.h
// Every entry here comes from a real, already-bound MotionGraph/
// AnimationClip -- states the character genuinely has, transitions
// that genuinely exist, clip durations/loop flags that are real
// authored values (see ANIMATION/SkeletonSystem/AnimationClip.h). This
// is a STRUCTURING of existing data, not a new animation authority --
// LAW C012 still applies (combat/animation only ever comes from the
// real MotionGraphEvaluator, never a second system).
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ANIMATION/AnimationGraph/MotionGraph.h"
#include "ANIMATION/SkeletonSystem/AnimationClip.h"
#include "CHARACTER/Rig/RigBinder.h"

namespace dominus::visualforge {

struct AnimationStateSpec {
    std::string state_name;
    std::string clip_name;
    bool clip_found = false;   // false if the state names a clip the AnimationSetComponent doesn't actually have
    float duration = 0.0f;     // 0 if clip_found is false -- not fabricated
    bool loop = false;
};

struct AnimationTransitionSpec {
    std::string from_state;
    std::string to_state;
    std::string trigger;
    float blend_duration = 0.0f;
    bool auto_on_complete = false;
};

struct AnimationSpecification {
    std::string entity_id;
    std::string entry_state;
    std::vector<AnimationStateSpec> states;
    std::vector<AnimationTransitionSpec> transitions;
};

class AnimationSpecificationForge {
public:
    // motionGraph/animationSet are both required -- a character with
    // neither bound genuinely has no animation to specify (RigBinder's
    // own MakeMotionGraphEvaluator has the same hard requirement, see
    // CHARACTER/Rig/RigBinder.h). Returns std::nullopt rather than a
    // half-populated, misleading spec.
    static std::optional<AnimationSpecification> Build(const std::string& entityId,
                                                         const character::MotionGraphComponent* motionGraph,
                                                         const character::AnimationSetComponent* animationSet) {
        if (!motionGraph || !animationSet) return std::nullopt;

        AnimationSpecification spec;
        spec.entity_id = entityId;
        spec.entry_state = motionGraph->graph.entry_state;

        for (const auto& state : motionGraph->graph.states) {
            AnimationStateSpec stateSpec;
            stateSpec.state_name = state.name;
            stateSpec.clip_name = state.clip_name;
            const auto* clip = animationSet->Find(state.clip_name);
            stateSpec.clip_found = (clip != nullptr);
            if (clip) {
                stateSpec.duration = clip->duration;
                stateSpec.loop = clip->loop;
            }
            spec.states.push_back(std::move(stateSpec));
        }

        for (const auto& transition : motionGraph->graph.transitions) {
            spec.transitions.push_back(AnimationTransitionSpec{transition.from_state, transition.to_state,
                                                                 transition.trigger, transition.blend_duration,
                                                                 transition.auto_on_complete});
        }

        return spec;
    }
};

}  // namespace dominus::visualforge
