// tests/combat/test_collision.cpp
#include "COMBAT/HitSystem/CollisionEvaluator.h"
#include "COMBAT/HitSystem/HurtboxLoader.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::animation::SkeletonLoader;
using dominus::combat::CollisionEvaluator;
using dominus::combat::HurtboxLoader;
using dominus::combat::MoveLoader;

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

// Translates every bone in a pose by a fixed offset -- stands in for "this
// skeleton's owner is standing somewhere else in the world" without
// needing a second physical fixture.
dominus::animation::Pose Translated(dominus::animation::Pose pose, float dx, float dy) {
    for (auto& t : pose) {
        t.x += dx;
        t.y += dy;
    }
    return pose;
}
}  // namespace

DOMINUS_TEST(CollisionEvaluator_DetectsHitWhenOpponentInRange) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto jab = MoveLoader::LoadFromFile(dir / "brooklyn_move_jab.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    DOMINUS_EXPECT(skel.ok && jab.ok && hurtboxes.ok);

    auto attackerPose = skel.value->ComputeBindPoseWorld();
    // Jab's hitbox is on arm_r at world (~15+5, 50) = (20,50) roughly.
    // Place the defender close enough that their torso hurtbox (radius 12,
    // at their own torso position (0,40) + this offset) overlaps.
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    auto hits = CollisionEvaluator::Evaluate(*skel.value, attackerPose, *jab.value, *skel.value, defenderPose,
                                              *hurtboxes.value);
    DOMINUS_EXPECT(!hits.empty());
}

DOMINUS_TEST(CollisionEvaluator_NoHitWhenOpponentFarAway) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto jab = MoveLoader::LoadFromFile(dir / "brooklyn_move_jab.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");

    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 500.0f, 500.0f);

    auto hits = CollisionEvaluator::Evaluate(*skel.value, attackerPose, *jab.value, *skel.value, defenderPose,
                                              *hurtboxes.value);
    DOMINUS_EXPECT(hits.empty());
}

DOMINUS_TEST(CollisionEvaluator_DodgeHasNoHitboxesSoNeverHits) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto dodge = MoveLoader::LoadFromFile(dir / "brooklyn_move_dodge.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");

    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = skel.value->ComputeBindPoseWorld();  // even at zero distance

    auto hits = CollisionEvaluator::Evaluate(*skel.value, attackerPose, *dodge.value, *skel.value, defenderPose,
                                              *hurtboxes.value);
    DOMINUS_EXPECT(hits.empty());
}

DOMINUS_TEST(CollisionEvaluator_HitReportsCorrectBoneNames) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto jab = MoveLoader::LoadFromFile(dir / "brooklyn_move_jab.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");

    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    auto hits = CollisionEvaluator::Evaluate(*skel.value, attackerPose, *jab.value, *skel.value, defenderPose,
                                              *hurtboxes.value);
    DOMINUS_EXPECT(!hits.empty());
    DOMINUS_EXPECT(hits[0].attacker_bone == "arm_r");
}
