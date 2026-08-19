// tests/animation/test_animation_clip.cpp
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::animation::AnimationClipLoader;

namespace {
std::filesystem::path FixturePath(const std::string& name) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures") / name,
        std::filesystem::path("../tests/fixtures") / name,
        std::filesystem::path("../../tests/fixtures") / name,
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture not found: " + name);
}

bool NearlyEqual(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(AnimationClip_LoadsIdleFixture) {
    auto result = AnimationClipLoader::LoadFromFile(FixturePath("brooklyn_idle.clip.json"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->name == "idle");
    DOMINUS_EXPECT(result.value->loop == true);
    DOMINUS_EXPECT(NearlyEqual(result.value->duration, 1.0f));
}

DOMINUS_TEST(AnimationClip_SamplesExactKeyframe) {
    auto result = AnimationClipLoader::LoadFromFile(FixturePath("brooklyn_idle.clip.json"));
    auto pose = result.value->Sample("torso", 0.5f);
    DOMINUS_EXPECT(pose.has_value());
    DOMINUS_EXPECT(NearlyEqual(pose->y, 42.0f));
}

DOMINUS_TEST(AnimationClip_InterpolatesBetweenKeyframes) {
    auto result = AnimationClipLoader::LoadFromFile(FixturePath("brooklyn_idle.clip.json"));
    // Halfway between t=0 (y=40) and t=0.5 (y=42) is t=0.25 -> y=41.
    auto pose = result.value->Sample("torso", 0.25f);
    DOMINUS_EXPECT(pose.has_value());
    DOMINUS_EXPECT(NearlyEqual(pose->y, 41.0f));
}

DOMINUS_TEST(AnimationClip_LoopsPastDuration) {
    auto result = AnimationClipLoader::LoadFromFile(FixturePath("brooklyn_idle.clip.json"));
    // t=1.25 on a 1.0s looping clip wraps to t=0.25 -> same as above.
    auto pose = result.value->Sample("torso", 1.25f);
    DOMINUS_EXPECT(pose.has_value());
    DOMINUS_EXPECT(NearlyEqual(pose->y, 41.0f));
}

DOMINUS_TEST(AnimationClip_MissingTrackReturnsNullopt) {
    auto result = AnimationClipLoader::LoadFromFile(FixturePath("brooklyn_idle.clip.json"));
    auto pose = result.value->Sample("arm_r", 0.5f);  // idle clip has no arm_r track
    DOMINUS_EXPECT(!pose.has_value());
}

DOMINUS_TEST(AnimationClip_AttackClipDoesNotLoopAndClamps) {
    auto result = AnimationClipLoader::LoadFromFile(FixturePath("brooklyn_attack.clip.json"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->loop == false);

    // Past the clip's duration, a non-looping clip clamps to the final key.
    auto pose = result.value->Sample("arm_r", 999.0f);
    DOMINUS_EXPECT(pose.has_value());
    DOMINUS_EXPECT(NearlyEqual(pose->rotation_deg, 0.0f));  // last key returns arm to rest
}
