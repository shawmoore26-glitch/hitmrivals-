// tests/motion/test_animation_layers.cpp
#include "ANIMATION/AnimationGraph/AnimationLayerStack.h"
#include "ANIMATION/AnimationGraph/MotionGraphLoader.h"
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>
#include <unordered_map>

using dominus::animation::AnimationClip;
using dominus::animation::AnimationClipLoader;
using dominus::animation::AnimationLayerStack;
using dominus::animation::IClipSource;
using dominus::animation::LayerDef;
using dominus::animation::MotionGraphEvaluator;
using dominus::animation::MotionGraphLoader;
using dominus::animation::SkeletonLoader;

namespace {
std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}
bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }

struct MapClipSource : public IClipSource {
    std::unordered_map<std::string, AnimationClip> clips;
    const AnimationClip* Find(const std::string& name) const override {
        auto it = clips.find(name);
        return it == clips.end() ? nullptr : &it->second;
    }
};
}  // namespace

DOMINUS_TEST(AnimationLayerStack_UpperBodyLayerOnlyAffectsMaskedBones) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto baseGraph = MotionGraphLoader::LoadFromFile(dir / "brooklyn_motion_graph.json");
    auto upperGraph = MotionGraphLoader::LoadFromFile(dir / "brooklyn_upper_body_graph.json");

    MapClipSource clips;
    clips.clips.emplace("idle", std::move(*AnimationClipLoader::LoadFromFile(dir / "brooklyn_idle.clip.json").value));
    clips.clips.emplace("attack_jab",
                         std::move(*AnimationClipLoader::LoadFromFile(dir / "brooklyn_attack.clip.json").value));

    MotionGraphEvaluator baseEval(*skel.value, clips, *baseGraph.value);   // idle, whole body
    MotionGraphEvaluator upperEval(*skel.value, clips, *upperGraph.value); // attack_jab, arm only

    AnimationLayerStack stack;
    stack.AddLayer(LayerDef{"base", 1.0f, std::nullopt}, &baseEval);
    stack.AddLayer(LayerDef{"upper_body", 1.0f, std::unordered_set<std::string>{"arm_r"}}, &upperEval);

    // Sample at t=0.18 for both -- upperGraph's own clock advances the same
    // dt as base since Composite() drives both evaluators identically.
    // Composite() ticks 0.18s in one call via repeated small steps to reach
    // the attack's peak keyframe deterministically.
    dominus::animation::Pose pose;
    for (int i = 0; i < 18; ++i) pose = stack.Composite(*skel.value, 0.01f);

    auto torsoIdx = *skel.value->FindBoneIndex("torso");
    auto armIdx = *skel.value->FindBoneIndex("arm_r");

    // torso is NOT in the upper-body mask -> should match pure idle at t~0.18
    // (idle torso y oscillates gently; just confirm it did NOT jump to the
    // attack clip's torso lean of +4/+6deg, proving the mask excluded it).
    DOMINUS_EXPECT(pose[torsoIdx].rotation_deg < 5.9f);  // attack's torso lean is 6deg at its peak

    // arm_r IS in the mask -> should reflect the attack swing, not idle's
    // resting arm (idle has no arm_r track at all, so idle contributes bind
    // pose rotation 0 for arm_r; the layered result should be well past 0).
    DOMINUS_EXPECT(pose[armIdx].rotation_deg < -30.0f);
}

DOMINUS_TEST(AnimationLayerStack_SingleLayerIsJustThatLayersPose) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(dir / "brooklyn_motion_graph.json");

    MapClipSource clips;
    clips.clips.emplace("idle", std::move(*AnimationClipLoader::LoadFromFile(dir / "brooklyn_idle.clip.json").value));
    clips.clips.emplace("attack_jab",
                         std::move(*AnimationClipLoader::LoadFromFile(dir / "brooklyn_attack.clip.json").value));

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    AnimationLayerStack stack;
    stack.AddLayer(LayerDef{"base", 1.0f, std::nullopt}, &eval);

    auto pose = stack.Composite(*skel.value, 0.5f);
    auto torsoIdx = *skel.value->FindBoneIndex("torso");
    DOMINUS_EXPECT(NearlyEqual(pose[torsoIdx].y, 42.0f));  // matches idle's own t=0.5 keyframe
}
