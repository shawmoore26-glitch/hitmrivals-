// tests/motion/test_motion_graph.cpp
#include "ANIMATION/AnimationGraph/MotionGraphEvaluator.h"
#include "ANIMATION/AnimationGraph/MotionGraphLoader.h"
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>
#include <unordered_map>

using dominus::animation::AnimationClip;
using dominus::animation::AnimationClipLoader;
using dominus::animation::IClipSource;
using dominus::animation::MotionGraph;
using dominus::animation::MotionGraphEvaluator;
using dominus::animation::MotionGraphLoader;
using dominus::animation::Skeleton;
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

// Simple test double implementing IClipSource directly (no dependency on
// CHARACTER's AnimationSetComponent -- this test exercises ANIMATION in
// isolation, per the module boundary).
struct MapClipSource : public IClipSource {
    std::unordered_map<std::string, AnimationClip> clips;
    const AnimationClip* Find(const std::string& name) const override {
        auto it = clips.find(name);
        return it == clips.end() ? nullptr : &it->second;
    }
};

MapClipSource LoadBrooklynClips() {
    auto dir = FixtureDir();
    MapClipSource source;
    auto idle = AnimationClipLoader::LoadFromFile(dir / "brooklyn_idle.clip.json");
    auto attack = AnimationClipLoader::LoadFromFile(dir / "brooklyn_attack.clip.json");
    source.clips.emplace("idle", std::move(*idle.value));
    source.clips.emplace("attack_jab", std::move(*attack.value));
    return source;
}

}  // namespace

DOMINUS_TEST(MotionGraph_LoadsFixture) {
    auto result = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->entry_state == "idle");
    DOMINUS_EXPECT(result.value->states.size() == 13);       // Phase 3.9: full move roster + reactions
    DOMINUS_EXPECT(result.value->transitions.size() == 47);
}

DOMINUS_TEST(MotionGraphEvaluator_StartsInEntryState) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    auto clips = LoadBrooklynClips();

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    DOMINUS_EXPECT(eval.CurrentState() == "idle");
    DOMINUS_EXPECT(!eval.IsTransitioning());
}

DOMINUS_TEST(MotionGraphEvaluator_TriggerStartsBlendTransition) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    auto clips = LoadBrooklynClips();

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    eval.Update(0.2f);  // settle into idle for a moment

    bool triggered = eval.Trigger("attack");
    DOMINUS_EXPECT(triggered);
    DOMINUS_EXPECT(eval.IsTransitioning());
    // Still reports the outgoing state name until the blend completes.
    DOMINUS_EXPECT(eval.CurrentState() == "idle");
}

DOMINUS_TEST(MotionGraphEvaluator_UnknownTriggerIsNoOp) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    auto clips = LoadBrooklynClips();

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    bool triggered = eval.Trigger("does_not_exist");
    DOMINUS_EXPECT(!triggered);
    DOMINUS_EXPECT(!eval.IsTransitioning());
}

DOMINUS_TEST(MotionGraphEvaluator_BlendCompletesAndCommitsToTargetState) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    auto clips = LoadBrooklynClips();

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    eval.Trigger("attack");
    DOMINUS_EXPECT(eval.IsTransitioning());

    // blend_duration is 0.1s for idle->attack; step past it.
    eval.Update(0.05f);
    DOMINUS_EXPECT(eval.IsTransitioning());
    eval.Update(0.06f);  // total 0.11s, past the 0.1s blend
    DOMINUS_EXPECT(!eval.IsTransitioning());
    DOMINUS_EXPECT(eval.CurrentState() == "attack");
}

DOMINUS_TEST(MotionGraphEvaluator_AutoTransitionFiresWhenNonLoopingClipFinishes) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    auto clips = LoadBrooklynClips();

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    eval.Trigger("attack");
    eval.Update(0.11f);  // commit into "attack" state
    DOMINUS_EXPECT(eval.CurrentState() == "attack");

    // attack_jab's duration is 0.3s and is non-looping; driving well past
    // that should auto-fire the "complete" transition back to idle.
    for (int i = 0; i < 20; ++i) eval.Update(0.05f);  // 1.0s total, well past 0.3s + 0.15s blend
    DOMINUS_EXPECT(eval.CurrentState() == "idle");
    DOMINUS_EXPECT(!eval.IsTransitioning());
}

DOMINUS_TEST(MotionGraphEvaluator_BlendedPoseIsBetweenOutgoingAndIncoming) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto graph = MotionGraphLoader::LoadFromFile(FixtureDir() / "brooklyn_motion_graph.json");
    auto clips = LoadBrooklynClips();
    auto armIdx = *skel.value->FindBoneIndex("arm_r");

    MotionGraphEvaluator eval(*skel.value, clips, *graph.value);
    eval.Update(0.0f);                       // idle pose, arm rest (rotation 0)
    eval.Trigger("attack");                  // begin 0.1s blend into attack_jab
    auto poseHalfway = eval.Update(0.05f);    // halfway through the blend

    // Idle has arm at rotation 0 throughout; attack_jab at its own t=0.05 is
    // partway into its swing (between key t=0.0 rot=0 and t=0.1 rot=-70).
    // The blended result should sit strictly between the idle pose (0) and
    // the attack's own partial swing -- i.e. not equal to either endpoint,
    // and not overshooting past the attack clip's own value at that time.
    DOMINUS_EXPECT(poseHalfway[armIdx].rotation_deg < 0.0f);   // moved toward the swing
    DOMINUS_EXPECT(poseHalfway[armIdx].rotation_deg > -70.0f); // hasn't reached full attack pose
}
