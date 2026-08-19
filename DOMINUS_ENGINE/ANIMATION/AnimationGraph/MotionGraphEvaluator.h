// ANIMATION/AnimationGraph/MotionGraphEvaluator.h
// The runtime side of MotionGraph: tracks current state, current clip time,
// and an optional in-flight blend transition, producing a world-space Pose
// each Update(dt). This is the "Animation Graph system" + "state machine" +
// "blend transitions" requirements as one cohesive evaluator rather than
// three disconnected pieces -- a blend transition IS a graph edge being
// walked, and the state machine IS the graph's control layer, so splitting
// them into unrelated classes would violate Law 1 (no disconnected
// systems).
#pragma once

#include <stdexcept>
#include <string>

#include "ANIMATION/AnimationGraph/MotionGraph.h"
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"
#include "ANIMATION/SkeletonSystem/AnimationClip.h"
#include "ANIMATION/SkeletonSystem/Skeleton.h"

namespace dominus::animation {

// Minimal interface the evaluator needs from wherever clips are stored --
// avoids a hard dependency on CHARACTER's AnimationSetComponent so ANIMATION
// never depends on CHARACTER (module boundary: CHARACTER reads ANIMATION,
// never the reverse).
class IClipSource {
public:
    virtual ~IClipSource() = default;
    virtual const AnimationClip* Find(const std::string& name) const = 0;
};

class MotionGraphEvaluator {
public:
    MotionGraphEvaluator(const Skeleton& skeleton, const IClipSource& clips, const MotionGraph& graph)
        : skeleton_(skeleton), clips_(clips), graph_(graph) {
        currentState_ = graph.entry_state;
        const MotionState* state = graph.FindState(currentState_);
        if (!state) throw std::runtime_error("MotionGraphEvaluator: invalid entry_state");
        currentClip_ = clips_.Find(state->clip_name);
        if (!currentClip_) {
            throw std::runtime_error("MotionGraphEvaluator: entry state's clip '" + state->clip_name +
                                      "' not found in clip source");
        }
    }

    const std::string& CurrentState() const { return currentState_; }
    bool IsTransitioning() const { return transitioning_; }

    // Requests a transition out of the current state via `trigger`. No-op
    // (returns false) if no matching transition exists or a transition is
    // already in flight -- callers should check the return value if they
    // need to know whether the request took effect.
    bool Trigger(const std::string& trigger) {
        if (transitioning_) return false;
        const MotionTransition* t = graph_.FindTransition(currentState_, trigger);
        if (!t) return false;
        BeginTransition(*t);
        return true;
    }

    Pose Update(float dt) {
        clipTime_ += dt;

        if (transitioning_) {
            transitionElapsed_ += dt;
            targetClipTime_ += dt;

            Pose targetPose = AnimationPlayer::Sample(skeleton_, *targetClip_, targetClipTime_);
            float alpha = transitionDuration_ > 0.0f ? transitionElapsed_ / transitionDuration_ : 1.0f;
            if (alpha >= 1.0f) {
                // Transition complete: commit to the target state.
                currentState_ = pendingState_;
                currentClip_ = targetClip_;
                clipTime_ = targetClipTime_;
                transitioning_ = false;
                return targetPose;
            }

            Pose blended(blendFromPose_.size());
            for (size_t i = 0; i < blended.size(); ++i) {
                blended[i] = Lerp(blendFromPose_[i], targetPose[i], alpha);
            }
            return blended;
        }

        // Not transitioning: check for an auto-on-complete transition once a
        // non-looping clip has finished.
        if (!currentClip_->loop && clipTime_ >= currentClip_->duration) {
            if (const MotionTransition* autoT = graph_.FindAutoTransition(currentState_)) {
                BeginTransition(*autoT);
                // Fall through: recurse the frame's remaining logic by
                // re-entering Update with dt=0 so this frame still returns a
                // valid pose (the freshly-started transition at alpha=0).
                return Update(0.0f);
            }
        }

        return AnimationPlayer::Sample(skeleton_, *currentClip_, clipTime_);
    }

private:
    void BeginTransition(const MotionTransition& t) {
        const MotionState* targetState = graph_.FindState(t.to_state);
        if (!targetState) throw std::runtime_error("MotionGraphEvaluator: transition target state not found: " + t.to_state);
        const AnimationClip* targetClip = clips_.Find(targetState->clip_name);
        if (!targetClip) {
            throw std::runtime_error("MotionGraphEvaluator: transition target clip not found: " + targetState->clip_name);
        }

        blendFromPose_ = AnimationPlayer::Sample(skeleton_, *currentClip_, clipTime_);
        pendingState_ = t.to_state;
        targetClip_ = targetClip;
        targetClipTime_ = 0.0f;
        transitionDuration_ = t.blend_duration;
        transitionElapsed_ = 0.0f;
        transitioning_ = true;
    }

    const Skeleton& skeleton_;
    const IClipSource& clips_;
    const MotionGraph& graph_;

    std::string currentState_;
    const AnimationClip* currentClip_ = nullptr;
    float clipTime_ = 0.0f;

    bool transitioning_ = false;
    std::string pendingState_;
    const AnimationClip* targetClip_ = nullptr;
    float targetClipTime_ = 0.0f;
    float transitionDuration_ = 0.0f;
    float transitionElapsed_ = 0.0f;
    Pose blendFromPose_;
};

}  // namespace dominus::animation
