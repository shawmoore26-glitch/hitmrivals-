// tests/soul/test_environment.cpp
#include "COMBAT/CombatController.h"
#include "COMBAT/Environment.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::DestructionZone;
using dominus::combat::EnvironmentBounds;
using dominus::combat::EnvironmentCollision;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionInput;
using dominus::combat::ReactionResult;
using dominus::combat::ReactionSystem;
using dominus::combat::ReactionType;
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
}  // namespace

DOMINUS_TEST(EnvironmentCollision_DetectsWallCrossing) {
    EnvironmentBounds bounds{-100.0f, 100.0f, 0.0f, 500.0f};
    DOMINUS_EXPECT(EnvironmentCollision::CrossesWallX(bounds, 150.0f));
    DOMINUS_EXPECT(EnvironmentCollision::CrossesWallX(bounds, -150.0f));
    DOMINUS_EXPECT(!EnvironmentCollision::CrossesWallX(bounds, 0.0f));
}

DOMINUS_TEST(EnvironmentCollision_DetectsFloorCrossing) {
    EnvironmentBounds bounds{-100.0f, 100.0f, 0.0f, 500.0f};
    DOMINUS_EXPECT(EnvironmentCollision::CrossesFloorY(bounds, -5.0f));
    DOMINUS_EXPECT(!EnvironmentCollision::CrossesFloorY(bounds, 50.0f));
}

DOMINUS_TEST(EnvironmentCollision_FindsDestructionZoneWithinRadius) {
    std::vector<DestructionZone> zones = {{"crate", 50.0f, 50.0f, 10.0f}};
    auto found = EnvironmentCollision::FindZone(zones, 52.0f, 51.0f);
    DOMINUS_EXPECT(found.has_value());
    DOMINUS_EXPECT(found->name == "crate");

    auto notFound = EnvironmentCollision::FindZone(zones, 500.0f, 500.0f);
    DOMINUS_EXPECT(!notFound.has_value());
}

DOMINUS_TEST(EnvironmentCollision_ApplyEnvironment_UpgradesToWallImpactNearBoundary) {
    // Small arena so a knockback's force clearly overshoots the wall.
    EnvironmentBounds bounds{-10.0f, 10.0f, 0.0f, 500.0f};
    ReactionResult base = ReactionSystem::Determine(ReactionInput{.hit_power = 25.0f}, 1.0f);  // knockback
    DOMINUS_EXPECT(base.type == ReactionType::kKnockback);

    // Defender standing near the wall -- projected position crosses it.
    auto upgraded = EnvironmentCollision::ApplyEnvironment(base, bounds, 9.0f, 100.0f, 0.5f);
    DOMINUS_EXPECT(upgraded.type == ReactionType::kWallImpact);
    DOMINUS_EXPECT(upgraded.motion_trigger == "wall_impact");
}

DOMINUS_TEST(EnvironmentCollision_ApplyEnvironment_LeavesReactionUnchangedInOpenSpace) {
    EnvironmentBounds bounds{-1000.0f, 1000.0f, 0.0f, 500.0f};
    ReactionResult base = ReactionSystem::Determine(ReactionInput{.hit_power = 25.0f}, 1.0f);
    auto unchanged = EnvironmentCollision::ApplyEnvironment(base, bounds, 0.0f, 100.0f, 0.5f);
    DOMINUS_EXPECT(unchanged.type == base.type);
    DOMINUS_EXPECT(unchanged.motion_trigger == base.motion_trigger);
}

DOMINUS_TEST(EnvironmentCollision_ApplyEnvironment_UpgradesToGroundImpactWhenFalling) {
    EnvironmentBounds bounds{-1000.0f, 1000.0f, 0.0f, 500.0f};
    // ReactionSystem never produces downward force (only knockback's
    // horizontal push or launch/knockdown's upward pop), so construct a
    // downward-force reaction directly -- standing in for a stomp/slam-type
    // move that drives the defender toward the floor rather than away.
    ReactionResult stomp;
    stomp.type = ReactionType::kKnockback;
    stomp.force_x = 0.0f;
    stomp.force_y = -50.0f;
    stomp.motion_trigger = "knockback";

    auto upgraded = EnvironmentCollision::ApplyEnvironment(stomp, bounds, 0.0f, 2.0f, 0.5f);
    DOMINUS_EXPECT(upgraded.type == ReactionType::kGroundImpact);
    DOMINUS_EXPECT(upgraded.motion_trigger == "ground_impact");
}

DOMINUS_TEST(Integration_UpgradedReactionFlowsThroughCombatControllerViaApplyPrecomputedReaction) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    EnvironmentBounds bounds{-10.0f, 10.0f, 0.0f, 500.0f};
    ReactionResult base = ReactionSystem::Determine(ReactionInput{.hit_power = 25.0f}, 1.0f);
    auto upgraded = EnvironmentCollision::ApplyEnvironment(base, bounds, 9.0f, 100.0f, 0.5f);
    DOMINUS_EXPECT(upgraded.type == ReactionType::kWallImpact);

    controller.StartMove("jab");
    auto result = controller.ApplyPrecomputedReaction(upgraded);
    DOMINUS_EXPECT(result.type == ReactionType::kWallImpact);
    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);  // interrupted, same as ApplyHit
}
