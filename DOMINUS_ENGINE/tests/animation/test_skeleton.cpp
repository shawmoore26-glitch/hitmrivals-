// tests/animation/test_skeleton.cpp
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>

using dominus::animation::SkeletonLoader;

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
}  // namespace

DOMINUS_TEST(Skeleton_LoadsFourBoneFixture) {
    auto result = SkeletonLoader::LoadFromFile(FixturePath("brooklyn.skel.json"));
    DOMINUS_EXPECT(result.ok);
    // 7 bones since the bone-rig fix: root/torso/head/arm_r (original 4,
    // untouched) plus arm_l/leg_r/leg_l (added for bilateral symmetry --
    // a fighter had a right arm and no left arm, and no legs at all).
    DOMINUS_EXPECT(result.value->BoneCount() == 7);

    auto rootIdx = result.value->FindBoneIndex("root");
    auto torsoIdx = result.value->FindBoneIndex("torso");
    auto armIdx = result.value->FindBoneIndex("arm_r");
    DOMINUS_EXPECT(rootIdx.has_value());
    DOMINUS_EXPECT(torsoIdx.has_value());
    DOMINUS_EXPECT(armIdx.has_value());

    const auto& bones = result.value->Bones();
    DOMINUS_EXPECT(bones[*rootIdx].parent_index == -1);
    DOMINUS_EXPECT(bones[*torsoIdx].parent_index == *rootIdx);
    DOMINUS_EXPECT(bones[*armIdx].parent_index == *torsoIdx);
}

DOMINUS_TEST(Skeleton_BindPoseWorldComposesHierarchy) {
    auto result = SkeletonLoader::LoadFromFile(FixturePath("brooklyn.skel.json"));
    DOMINUS_EXPECT(result.ok);

    auto world = result.value->ComputeBindPoseWorld();
    auto torsoIdx = *result.value->FindBoneIndex("torso");
    auto headIdx = *result.value->FindBoneIndex("head");

    // torso is root.y(0) + local.y(40) = 40
    DOMINUS_EXPECT(world[torsoIdx].y == 40.0f);
    // head is parented to torso: torso.y(40) + local.y(30) = 70
    DOMINUS_EXPECT(world[headIdx].y == 70.0f);
}

DOMINUS_TEST(Skeleton_RejectsUnknownParent) {
    // Sanity check the loader's forward-reference guard using an inline
    // malformed file written to a temp path.
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "dominus_bad_skel.json";
    {
        std::ofstream out(tmp);
        out << R"({"bones":[{"name":"child","parent":"nonexistent","x":0,"y":0}]})";
    }
    auto result = SkeletonLoader::LoadFromFile(tmp);
    DOMINUS_EXPECT(!result.ok);
    std::filesystem::remove(tmp);
}

DOMINUS_TEST(Skeleton_RejectsDuplicateBoneName) {
    // The real, previously-unguarded gap: two bones sharing a name used
    // to silently succeed, with the second bone's index quietly
    // overwriting the first's in Skeleton's own name->index map --
    // "N entries but fewer unique bones," permanently orphaning the
    // first bone from every name-based lookup (hurtboxes, hitboxes, IK
    // targets) while it still occupied a slot in the bone array.
    auto result = SkeletonLoader::LoadFromFile(FixturePath("broken_duplicate_bone_name.skel.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("duplicate") != std::string::npos);
}
