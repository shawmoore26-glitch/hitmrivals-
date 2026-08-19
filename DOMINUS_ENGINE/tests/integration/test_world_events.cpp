// tests/integration/test_world_events.cpp
// Priority 4 proof: Wall Impact -> World Event -> Destruction/Particles/
// Audio/Camera, composed with the existing Environment + Reaction systems.
#include "COMBAT/WorldEventSystem.h"
#include "tests/TestFramework.h"

using dominus::combat::DestructionZone;
using dominus::combat::EnvironmentBounds;
using dominus::combat::EnvironmentCollision;
using dominus::combat::ReactionInput;
using dominus::combat::ReactionResult;
using dominus::combat::ReactionSystem;
using dominus::combat::ReactionType;
using dominus::combat::WorldEvent;
using dominus::combat::WorldEventSystem;

DOMINUS_TEST(WorldEventSystem_WallImpactProducesRealEvent) {
    ReactionResult wallHit;
    wallHit.type = ReactionType::kWallImpact;
    wallHit.motion_trigger = "wall_impact";

    auto event = WorldEventSystem::FromReaction(wallHit, 50.0f, 100.0f);
    DOMINUS_EXPECT(WorldEventSystem::IsRealEvent(event));
    DOMINUS_EXPECT(event.event_type == "wall_impact");
    DOMINUS_EXPECT(event.destruction_tag == "wall_debris");
    DOMINUS_EXPECT(!event.particle_tag.empty());
    DOMINUS_EXPECT(!event.audio_tag.empty());
    DOMINUS_EXPECT(!event.camera_tag.empty());
    DOMINUS_EXPECT(event.x == 50.0f);
    DOMINUS_EXPECT(event.y == 100.0f);
}

DOMINUS_TEST(WorldEventSystem_GroundImpactProducesDistinctEvent) {
    ReactionResult groundHit;
    groundHit.type = ReactionType::kGroundImpact;
    auto event = WorldEventSystem::FromReaction(groundHit, 0.0f, 0.0f);
    DOMINUS_EXPECT(event.event_type == "ground_impact");
    DOMINUS_EXPECT(event.destruction_tag == "ground_crater");
}

DOMINUS_TEST(WorldEventSystem_OrdinaryReactionProducesNoEvent) {
    ReactionResult stagger;
    stagger.type = ReactionType::kStagger;
    auto event = WorldEventSystem::FromReaction(stagger, 0.0f, 0.0f);
    DOMINUS_EXPECT(!WorldEventSystem::IsRealEvent(event));
}

DOMINUS_TEST(WorldEventSystem_DestructionZoneProducesEventWithZoneName) {
    DestructionZone crate{"wooden_crate", 30.0f, 10.0f, 8.0f};
    auto event = WorldEventSystem::FromDestructionZone(crate, 32.0f, 11.0f);
    DOMINUS_EXPECT(WorldEventSystem::IsRealEvent(event));
    DOMINUS_EXPECT(event.event_type == "environmental_destruction");
    DOMINUS_EXPECT(event.destruction_tag == "wooden_crate_break");
}

DOMINUS_TEST(Integration_EnvironmentUpgradeFlowsIntoWorldEvent) {
    // The full composed pipeline: a plain knockback reaction near a wall
    // gets upgraded by EnvironmentCollision, and that upgraded result
    // produces a real WorldEvent -- the exact bridge Priority 4 asks for.
    EnvironmentBounds bounds{-10.0f, 10.0f, 0.0f, 500.0f};
    ReactionResult base = ReactionSystem::Determine(ReactionInput{.hit_power = 25.0f}, 1.0f);
    DOMINUS_EXPECT(base.type == ReactionType::kKnockback);  // not yet an impact

    auto upgraded = EnvironmentCollision::ApplyEnvironment(base, bounds, 9.0f, 100.0f, 0.5f);
    DOMINUS_EXPECT(upgraded.type == ReactionType::kWallImpact);

    auto worldEvent = WorldEventSystem::FromReaction(upgraded, 9.0f, 100.0f);
    DOMINUS_EXPECT(WorldEventSystem::IsRealEvent(worldEvent));
    DOMINUS_EXPECT(worldEvent.event_type == "wall_impact");
}

DOMINUS_TEST(Integration_DestructionZoneDetectionFlowsIntoWorldEvent) {
    std::vector<DestructionZone> zones = {{"support_pillar", 40.0f, 20.0f, 6.0f}};
    auto found = EnvironmentCollision::FindZone(zones, 42.0f, 21.0f);
    DOMINUS_EXPECT(found.has_value());

    auto worldEvent = WorldEventSystem::FromDestructionZone(*found, 42.0f, 21.0f);
    DOMINUS_EXPECT(worldEvent.destruction_tag == "support_pillar_break");
}
