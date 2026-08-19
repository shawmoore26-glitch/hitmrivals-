// tests/motion/test_ik.cpp
#include "ANIMATION/IK/IKChain.h"
#include "ANIMATION/IK/IKChainLoader.h"
#include "ANIMATION/IK/TwoBoneIK.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::animation::IKChainDef;
using dominus::animation::IKChainLoader;
using dominus::animation::IKChainSolver;
using dominus::animation::Pose;
using dominus::animation::SkeletonLoader;
using dominus::animation::Transform2D;
using dominus::animation::TwoBoneIK;
using dominus::character::IKChainSetComponent;
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
bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(TwoBoneIK_SolvesReachableTarget) {
    // Two 10-unit segments, target straight out at distance 15 (reachable,
    // between minReach=0 and maxReach=20). Root at origin.
    auto result = TwoBoneIK::Solve(0.0f, 0.0f, 10.0f, 10.0f, 15.0f, 0.0f, 1.0f);
    DOMINUS_EXPECT(result.target_reachable);
}

DOMINUS_TEST(TwoBoneIK_ClampsUnreachableTargetBeyondMaxReach) {
    auto result = TwoBoneIK::Solve(0.0f, 0.0f, 10.0f, 10.0f, 1000.0f, 0.0f, 1.0f);
    DOMINUS_EXPECT(!result.target_reachable);
}

DOMINUS_TEST(TwoBoneIK_FullyExtendedChainReachesStraightLineTarget) {
    // Target exactly at max reach (upper+lower) along +x: chain should fully
    // extend, both joints at 0 world rotation relative to +x axis.
    auto result = TwoBoneIK::Solve(0.0f, 0.0f, 10.0f, 10.0f, 20.0f, 0.0f, 1.0f);
    DOMINUS_EXPECT(result.target_reachable);
    DOMINUS_EXPECT(NearlyEqual(result.root_rotation_deg, 0.0f, 0.5f));
}

DOMINUS_TEST(IKChainSolver_EndEffectorReachesTargetForReachableDistance) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "ik_arm_test.skel.json");
    DOMINUS_EXPECT(skel.ok);

    auto chain = IKChainLoader::LoadFromFile(FixtureDir() / "ik_arm_test.ikchain.json");
    DOMINUS_EXPECT(chain.ok);

    // Start from bind pose (shoulder/elbow/wrist all at rotation 0).
    Pose pose = skel.value->ComputeBindPoseWorld();

    // Chain reach is 20 (upper) + 15 (lower) = 35 max. Target at distance 25
    // is comfortably reachable.
    float targetX = 25.0f, targetY = 10.0f;
    auto applyResult = IKChainSolver::Apply(*skel.value, *chain.value, targetX, targetY, pose);
    DOMINUS_EXPECT(applyResult.reachable);

    auto wristIdx = *skel.value->FindBoneIndex("wrist");
    DOMINUS_EXPECT(NearlyEqual(pose[wristIdx].x, targetX, 0.05f));
    DOMINUS_EXPECT(NearlyEqual(pose[wristIdx].y, targetY, 0.05f));
}

DOMINUS_TEST(IKChainSolver_ClampsToMaxReachWhenTargetTooFar) {
    auto skel = SkeletonLoader::LoadFromFile(FixtureDir() / "ik_arm_test.skel.json");
    auto chain = IKChainLoader::LoadFromFile(FixtureDir() / "ik_arm_test.ikchain.json");
    Pose pose = skel.value->ComputeBindPoseWorld();

    // Way beyond the 35-unit max reach.
    auto applyResult = IKChainSolver::Apply(*skel.value, *chain.value, 500.0f, 0.0f, pose);
    DOMINUS_EXPECT(!applyResult.reachable);

    auto wristIdx = *skel.value->FindBoneIndex("wrist");
    float dist = std::sqrt(pose[wristIdx].x * pose[wristIdx].x + pose[wristIdx].y * pose[wristIdx].y);
    DOMINUS_EXPECT(NearlyEqual(dist, 35.0f, 0.1f));  // fully extended, not overshooting
}

DOMINUS_TEST(RigBinder_ResolvesIKChainRefFromDotDominus) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "ik_test_rig.dominus");
    DOMINUS_EXPECT(loadResult.ok);

    auto& obj = *loadResult.value;
    auto bindResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* ikSet = obj.GetComponent<IKChainSetComponent>();
    DOMINUS_EXPECT(ikSet != nullptr);
    const auto* chain = ikSet->Find("arm_reach");
    DOMINUS_EXPECT(chain != nullptr);
    DOMINUS_EXPECT(chain->root_bone == "shoulder");
    DOMINUS_EXPECT(chain->mid_bone == "elbow");
    DOMINUS_EXPECT(chain->end_bone == "wrist");
}

DOMINUS_TEST(RigBinder_FullPipeline_LoadBindAndSolveIKFromDotDominus) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "ik_test_rig.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);

    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    auto* ikSet = obj.GetComponent<IKChainSetComponent>();
    const auto* chain = ikSet->Find("arm_reach");

    Pose pose = skeleton->skeleton.ComputeBindPoseWorld();
    auto result = IKChainSolver::Apply(skeleton->skeleton, *chain, 20.0f, 20.0f, pose);
    DOMINUS_EXPECT(result.reachable);

    auto wristIdx = *skeleton->skeleton.FindBoneIndex("wrist");
    DOMINUS_EXPECT(NearlyEqual(pose[wristIdx].x, 20.0f, 0.05f));
    DOMINUS_EXPECT(NearlyEqual(pose[wristIdx].y, 20.0f, 0.05f));
}
