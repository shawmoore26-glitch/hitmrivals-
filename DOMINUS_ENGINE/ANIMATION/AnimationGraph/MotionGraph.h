// ANIMATION/AnimationGraph/MotionGraph.h
// Pure data: the state machine definition. States name a clip; transitions
// name a trigger, a blend duration, and optionally fire automatically when
// the source state's clip finishes (for non-looping clips like an attack).
// This is data only -- MotionGraphEvaluator.h is where it actually runs.
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dominus::animation {

struct MotionState {
    std::string name;
    std::string clip_name;  // key into AnimationSetComponent::clips
};

struct MotionTransition {
    std::string from_state;
    std::string to_state;
    std::string trigger;         // matched by MotionGraphEvaluator::Trigger()
    float blend_duration = 0.1f;
    bool auto_on_complete = false;  // fires without an explicit Trigger() call
                                     // once the from_state's clip finishes
};

class MotionGraph {
public:
    std::string entry_state;
    std::vector<MotionState> states;
    std::vector<MotionTransition> transitions;

    const MotionState* FindState(const std::string& name) const {
        for (auto& s : states)
            if (s.name == name) return &s;
        return nullptr;
    }

    // Returns the first transition out of `from` matching `trigger`, or
    // nullptr. Explicit triggers and auto-on-complete transitions share this
    // lookup -- auto transitions just use the reserved trigger name
    // "__complete__" (see MotionGraphEvaluator).
    const MotionTransition* FindTransition(const std::string& from, const std::string& trigger) const {
        for (auto& t : transitions)
            if (t.from_state == from && t.trigger == trigger) return &t;
        return nullptr;
    }

    const MotionTransition* FindAutoTransition(const std::string& from) const {
        for (auto& t : transitions)
            if (t.from_state == from && t.auto_on_complete) return &t;
        return nullptr;
    }
};

}  // namespace dominus::animation
