// tests/motion/test_procedural_hooks.cpp
#include "ANIMATION/ProceduralMotion/ProceduralHooks.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::animation::Pose;
using dominus::animation::ProceduralHookStack;
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
}  // namespace

DOMINUS_TEST(ProceduralHookStack_EmptyStackLeavesPoseUnchanged) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    Pose pose = skel.value->ComputeBindPoseWorld();
    Pose original = pose;

    ProceduralHookStack stack;
    stack.Apply(pose, *skel.value, 1.23f);

    auto torsoIdx = *skel.value->FindBoneIndex("torso");
    DOMINUS_EXPECT(NearlyEqual(pose[torsoIdx].y, original[torsoIdx].y));
}

DOMINUS_TEST(ProceduralHookStack_BreathingHookOscillatesBoneY) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto torsoIdx = *skel.value->FindBoneIndex("torso");

    ProceduralHookStack stack;
    stack.AddHook("breathing", ProceduralHookStack::MakeBreathingHook("torso", 2.0f, 0.5f));

    // At t=0, sin(0)=0 -> no offset.
    Pose poseAtZero = skel.value->ComputeBindPoseWorld();
    float baselineY = poseAtZero[torsoIdx].y;
    stack.Apply(poseAtZero, *skel.value, 0.0f);
    DOMINUS_EXPECT(NearlyEqual(poseAtZero[torsoIdx].y, baselineY));

    // At t=0.5s with frequency 0.5Hz: sin(2*pi*0.5*0.5) = sin(pi/2) = 1 ->
    // full +amplitude offset.
    Pose poseAtHalf = skel.value->ComputeBindPoseWorld();
    stack.Apply(poseAtHalf, *skel.value, 0.5f);
    DOMINUS_EXPECT(NearlyEqual(poseAtHalf[torsoIdx].y, baselineY + 2.0f, 0.05f));
}

DOMINUS_TEST(ProceduralHookStack_LookAtHookPointsBoneTowardTarget) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto headIdx = *skel.value->FindBoneIndex("head");

    ProceduralHookStack stack;
    // Target directly above the head's bind-pose position (head sits at
    // world (0, 70) per bind pose composition) -> should rotate toward
    // straight up.
    stack.AddHook("look_at", ProceduralHookStack::MakeLookAtHook("head", 0.0f, 170.0f));

    Pose pose = skel.value->ComputeBindPoseWorld();
    stack.Apply(pose, *skel.value, 0.0f);

    // Target is directly "north" of the head (dx=0, dy=+100) -> atan2(100,0)
    // = 90 degrees in this 2D convention.
    DOMINUS_EXPECT(NearlyEqual(pose[headIdx].rotation_deg, 90.0f, 0.5f));
}

DOMINUS_TEST(ProceduralHookStack_HooksApplyInRegistrationOrder) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "brooklyn.skel.json");
    auto torsoIdx = *skel.value->FindBoneIndex("torso");

    ProceduralHookStack stack;
    // First hook sets a known baseline offset, second hook adds on top --
    // if order were reversed the final value would differ.
    stack.AddHook("set_ten", [torsoIdx](Pose& pose, const auto&, float) { pose[torsoIdx].y = 10.0f; });
    stack.AddHook("add_five", [torsoIdx](Pose& pose, const auto&, float) { pose[torsoIdx].y += 5.0f; });

    Pose pose = skel.value->ComputeBindPoseWorld();
    stack.Apply(pose, *skel.value, 0.0f);
    DOMINUS_EXPECT(NearlyEqual(pose[torsoIdx].y, 15.0f));
    DOMINUS_EXPECT(stack.HookCount() == 2);
}
