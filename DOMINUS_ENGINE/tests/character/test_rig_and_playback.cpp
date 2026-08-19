// tests/character/test_rig_and_playback.cpp
// This is the Phase 2 milestone from ROADMAP.md, as an automated test:
// "one HITM RIVALS fighter loads as a .dominus object and plays an idle +
// one attack animation through the ANIMATION module, driven by
// CHARACTER/Rig." If this file is green, that milestone is met -- not
// claimed in prose.
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::animation::AnimationPlayer;
using dominus::character::AnimationSetComponent;
using dominus::character::RigBinder;
using dominus::character::SkeletonComponent;
using dominus::core::DominusSerializer;

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
bool NearlyEqual(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(RigBinder_LoadsAndBindsBrooklynFromDotDominus) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);

    auto& obj = *loadResult.value;
    auto bindResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    DOMINUS_EXPECT(skeleton != nullptr);
    // 7 bones since the bone-rig fix (bilateral arms + both legs).
    DOMINUS_EXPECT(skeleton->skeleton.BoneCount() == 7);

    auto* animSet = obj.GetComponent<AnimationSetComponent>();
    DOMINUS_EXPECT(animSet != nullptr);
    DOMINUS_EXPECT(animSet->Find("idle") != nullptr);
    DOMINUS_EXPECT(animSet->Find("attack_jab") != nullptr);
}

DOMINUS_TEST(Playback_IdlePlaysThroughAnimationPlayer) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);

    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    auto* animSet = obj.GetComponent<AnimationSetComponent>();
    const auto* idle = animSet->Find("idle");
    DOMINUS_EXPECT(idle != nullptr);

    auto torsoIdx = *skeleton->skeleton.FindBoneIndex("torso");

    // Sample across the loop: t=0 -> torso.y=40 (world, root at y=0).
    auto poseAtStart = AnimationPlayer::Sample(skeleton->skeleton, *idle, 0.0f);
    DOMINUS_EXPECT(NearlyEqual(poseAtStart[torsoIdx].y, 40.0f));

    // t=0.5 -> torso local y=42 -> world y=42 (root untouched).
    auto poseAtMid = AnimationPlayer::Sample(skeleton->skeleton, *idle, 0.5f);
    DOMINUS_EXPECT(NearlyEqual(poseAtMid[torsoIdx].y, 42.0f));

    // Looping: t=1.5 wraps to t=0.5 within a 1.0s clip.
    auto poseLooped = AnimationPlayer::Sample(skeleton->skeleton, *idle, 1.5f);
    DOMINUS_EXPECT(NearlyEqual(poseLooped[torsoIdx].y, 42.0f));
}

DOMINUS_TEST(Playback_AttackJabSwingsArmAndReturnsToRest) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);

    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    auto* animSet = obj.GetComponent<AnimationSetComponent>();
    const auto* attack = animSet->Find("attack_jab");
    DOMINUS_EXPECT(attack != nullptr);
    DOMINUS_EXPECT(attack->loop == false);

    auto armIdx = *skeleton->skeleton.FindBoneIndex("arm_r");

    // Rest pose at t=0.
    auto poseStart = AnimationPlayer::Sample(skeleton->skeleton, *attack, 0.0f);
    DOMINUS_EXPECT(NearlyEqual(poseStart[armIdx].rotation_deg, 0.0f));

    // Full extension at the peak keyframe (t=0.18): arm local rotation is
    // -90 deg, but torso also leans forward 6 deg at this keyframe (see
    // brooklyn_attack.clip.json), and world rotation composes as
    // parent_world + local (Transform2D::ComposeWorld). So arm world
    // rotation = torso world (0 + 6) + arm local (-90) = -84.
    auto posePeak = AnimationPlayer::Sample(skeleton->skeleton, *attack, 0.18f);
    DOMINUS_EXPECT(NearlyEqual(posePeak[armIdx].rotation_deg, -84.0f));

    // Returns to rest by the end of the clip (t=0.3).
    auto poseEnd = AnimationPlayer::Sample(skeleton->skeleton, *attack, 0.3f);
    DOMINUS_EXPECT(NearlyEqual(poseEnd[armIdx].rotation_deg, 0.0f));
}
