// tests/motion/test_retargeting.cpp
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"
#include "ANIMATION/Retargeting/Retarget.h"
#include "ANIMATION/Retargeting/RetargetMapLoader.h"
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::animation::AnimationClipLoader;
using dominus::animation::AnimationPlayer;
using dominus::animation::Retarget;
using dominus::animation::RetargetMapLoader;
using dominus::animation::SkeletonLoader;
using dominus::character::RetargetMapComponent;
using dominus::character::SkeletonComponent;
using dominus::character::RigBinder;
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
bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(RetargetMap_LoadsGenericToBrooklynFixture) {
    auto result = RetargetMapLoader::LoadFromFile(FixtureDir() / "generic_to_brooklyn_retarget.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->MappingCount() == 4);
    auto sourceForSpine = result.value->SourceBoneFor("spine");
    DOMINUS_EXPECT(sourceForSpine.has_value());
    DOMINUS_EXPECT(*sourceForSpine == "torso");
}

DOMINUS_TEST(Retarget_RenamesTracksOntoTargetSkeletonBoneNames) {
    auto dir = FixtureDir();
    auto idleClip = AnimationClipLoader::LoadFromFile(dir / "brooklyn_idle.clip.json");
    auto map = RetargetMapLoader::LoadFromFile(dir / "generic_to_brooklyn_retarget.json");
    DOMINUS_EXPECT(idleClip.ok);
    DOMINUS_EXPECT(map.ok);

    auto retargeted = Retarget::ApplyToClip(*idleClip.value, *map.value);

    // Original clip has tracks for "torso" and "head" (brooklyn bone names).
    // Retargeted clip should have the SAME keyframe data under "spine" and
    // "cranium" (generic biped bone names) instead.
    DOMINUS_EXPECT(retargeted.FindTrack("torso") == nullptr);   // old name gone
    DOMINUS_EXPECT(retargeted.FindTrack("spine") != nullptr);   // new name present

    auto originalTorsoPose = idleClip.value->Sample("torso", 0.5f);
    auto retargetedSpinePose = retargeted.Sample("spine", 0.5f);
    DOMINUS_EXPECT(originalTorsoPose.has_value());
    DOMINUS_EXPECT(retargetedSpinePose.has_value());
    DOMINUS_EXPECT(NearlyEqual(originalTorsoPose->y, retargetedSpinePose->y));
}

DOMINUS_TEST(Retarget_UnmappedOrUntrackedBonesAreOmitted) {
    auto dir = FixtureDir();
    auto idleClip = AnimationClipLoader::LoadFromFile(dir / "brooklyn_idle.clip.json");
    auto map = RetargetMapLoader::LoadFromFile(dir / "generic_to_brooklyn_retarget.json");
    auto retargeted = Retarget::ApplyToClip(*idleClip.value, *map.value);

    // idle clip has no "arm_r" track at all, so mapped "limb_r" should also
    // have no track in the retargeted result -- nothing to copy.
    DOMINUS_EXPECT(retargeted.FindTrack("limb_r") == nullptr);
}

DOMINUS_TEST(Retarget_PlaysCorrectlyOnDifferentlyNamedSkeleton) {
    auto dir = FixtureDir();
    auto genericSkel = SkeletonLoader::LoadFromFile(dir / "generic_biped.skel.json");
    auto idleClip = AnimationClipLoader::LoadFromFile(dir / "brooklyn_idle.clip.json");
    auto map = RetargetMapLoader::LoadFromFile(dir / "generic_to_brooklyn_retarget.json");
    DOMINUS_EXPECT(genericSkel.ok);

    auto retargeted = Retarget::ApplyToClip(*idleClip.value, *map.value);

    // Sample the retargeted clip against the GENERIC skeleton (not
    // Brooklyn's) -- this is the actual point of retargeting: motion
    // authored for one skeleton driving a different, differently-named one.
    auto pose = AnimationPlayer::Sample(*genericSkel.value, retargeted, 0.5f);
    auto spineIdx = *genericSkel.value->FindBoneIndex("spine");

    // Same as the original brooklyn torso track at t=0.5 (world y = 42,
    // since generic's root is also at origin with no rotation).
    DOMINUS_EXPECT(NearlyEqual(pose[spineIdx].y, 42.0f));
}

DOMINUS_TEST(RigBinder_ResolvesRetargetMapRefFromDotDominus) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "generic_biped.dominus");
    DOMINUS_EXPECT(loadResult.ok);

    auto& obj = *loadResult.value;
    auto bindResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* retargetComp = obj.GetComponent<RetargetMapComponent>();
    DOMINUS_EXPECT(retargetComp != nullptr);
    DOMINUS_EXPECT(retargetComp->map.MappingCount() == 4);

    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    DOMINUS_EXPECT(skeleton != nullptr);
    DOMINUS_EXPECT(skeleton->skeleton.BoneCount() == 4);
}
