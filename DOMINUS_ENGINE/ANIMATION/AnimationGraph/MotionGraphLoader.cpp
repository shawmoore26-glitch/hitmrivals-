// ANIMATION/AnimationGraph/MotionGraphLoader.cpp
#include "ANIMATION/AnimationGraph/MotionGraphLoader.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::animation {

using core::Result;
using core::json::Value;

namespace {
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
}  // namespace

// Expected format:
// {
//   "entry_state": "idle",
//   "states": [ { "name": "idle", "clip": "idle" }, { "name": "attack", "clip": "attack_jab" } ],
//   "transitions": [
//     { "from": "idle", "to": "attack", "trigger": "attack", "blend_duration": 0.1 },
//     { "from": "attack", "to": "idle", "trigger": "complete", "blend_duration": 0.15, "auto": true }
//   ]
// }
Result<MotionGraph> MotionGraphLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<MotionGraph>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<MotionGraph>::Fail(std::string("Parse error: ") + e.what());
    }

    MotionGraph graph;
    if (auto* entry = root.Get("entry_state")) graph.entry_state = entry->AsString();

    const Value* statesVal = root.Get("states");
    if (!statesVal || !statesVal->IsArray()) {
        return Result<MotionGraph>::Fail("motion graph missing 'states' array: " + path.string());
    }
    for (const Value& s : statesVal->AsArray()) {
        const Value* name = s.Get("name");
        const Value* clip = s.Get("clip");
        if (!name || !clip) return Result<MotionGraph>::Fail("state missing name/clip in " + path.string());
        graph.states.push_back(MotionState{name->AsString(), clip->AsString()});
    }

    if (const Value* transitionsVal = root.Get("transitions")) {
        if (transitionsVal->IsArray()) {
            for (const Value& t : transitionsVal->AsArray()) {
                MotionTransition mt;
                if (auto* v = t.Get("from")) mt.from_state = v->AsString();
                if (auto* v = t.Get("to")) mt.to_state = v->AsString();
                if (auto* v = t.Get("trigger")) mt.trigger = v->AsString();
                if (auto* v = t.Get("blend_duration")) mt.blend_duration = static_cast<float>(v->AsNumber());
                if (auto* v = t.Get("auto")) {
                    if (v->IsBool()) mt.auto_on_complete = v->AsBool();
                }
                if (mt.from_state.empty() || mt.to_state.empty()) {
                    return Result<MotionGraph>::Fail("transition missing from/to in " + path.string());
                }
                graph.transitions.push_back(std::move(mt));
            }
        }
    }

    if (graph.entry_state.empty() && !graph.states.empty()) {
        graph.entry_state = graph.states.front().name;
    }
    if (!graph.FindState(graph.entry_state)) {
        return Result<MotionGraph>::Fail("entry_state '" + graph.entry_state + "' not found among states in " +
                                          path.string());
    }

    return Result<MotionGraph>::Ok(std::move(graph));
}

}  // namespace dominus::animation
