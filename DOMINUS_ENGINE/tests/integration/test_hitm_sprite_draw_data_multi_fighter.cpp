// tests/integration/test_hitm_sprite_draw_data_multi_fighter.cpp
// ROADMAP.md Track H -- Track A gap #2: proves BuildSpriteDrawData
// executes Rocket's and Static's own real asset sets, not just
// Brooklyn's. Their real asset layer (atlas/parts/rig/anim) already
// imports cleanly (test_hitm_asset_importer.cpp) -- what had never been
// exercised is BuildSpriteDrawData itself against their real data.
//
// WHY HAND-CONSTRUCTED SNAPSHOTS, NOT A RUNNING HitmFighterRuntime:
// Module 5A's own real, documented finding stands unchanged -- Rocket's
// and Static's real signature.json "special" moves are missing fields
// (blockstun/range/hitstop) HitmMoveInstance::Extract requires, so
// HitmFighterRuntime::Create cannot build a full runtime for either of
// them (confirmed live: `dominus-cli hitm-sprite-draw-data` on Rocket
// fails at move extraction). That gap is Module 5A's territory, not
// Track A's, and is not reopened here. But HitmFighterSnapshot is a
// plain, public struct (Module 5A, unchanged) -- BuildSpriteDrawData's
// actual contract is "given any valid snapshot, produce draw data",
// and idle/walking/jumping states need no move data at all. Every
// snapshot field below is either a real, authored physics value (start
// position, computed via the exact same `(wallL+wallR)/2`/`ground`
// formula `HitmFighterRuntime::Create` itself uses) or a plain
// DOMINUS-internal simulation field (frame counter, state enum) --
// never HITM content, so constructing one by hand is ordinary test
// setup, not asset fabrication.
#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::character::hitm::HitmAssetBundle;
using dominus::character::hitm::HitmAssetImporter;
using dominus::character::hitm::HitmFighterSnapshot;
using dominus::character::hitm::HitmFighterState;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmSecondaryMotionState;

namespace {

std::filesystem::path FindDir(const std::filesystem::path& rel) {
    std::vector<std::filesystem::path> candidates = {rel, std::filesystem::path("..") / rel, std::filesystem::path("../..") / rel};
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture dir not found: " + rel.string());
}

HitmGameRules RealRules() {
    auto r = HitmGameRules::Import(FindDir("tests/fixtures/hitm_game_rules/game.json"));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}

HitmAssetBundle RealBundle(const std::string& fighter) {
    auto identity = HitmIdentityImporter::Import(FindDir(std::filesystem::path("tests/fixtures/hitm_identity") / fighter));
    if (!identity.ok) throw std::runtime_error("test setup: " + identity.error);
    auto bundle = HitmAssetImporter::Import(*identity.value, FindDir("tests/fixtures/hitm_sprite_assets"));
    if (!bundle.ok) throw std::runtime_error("test setup: " + bundle.error);
    return std::move(*bundle.value);
}

// Real init position, same formula HitmFighterRuntime::Create uses --
// see this file's header comment for why this is not fabricated data.
HitmFighterSnapshot RealInitSnapshot(const HitmGameRules& rules) {
    HitmFighterSnapshot snap;
    snap.frame = 0;
    snap.state = HitmFighterState::kIdle;
    snap.x = static_cast<float>((rules.Physics().wall_l + rules.Physics().wall_r) / 2.0);
    snap.y = static_cast<float>(rules.Physics().ground);
    snap.grounded = true;
    return snap;
}

const dominus::character::hitm::HitmPartDraw* FindDraw(const dominus::character::hitm::HitmSpriteDrawData& draw,
                                                          const std::string& partName) {
    for (const auto& p : draw.parts) {
        if (p.part_name == partName) return &p;
    }
    return nullptr;
}

}  // namespace

// --- Rocket: real idle/walk/jump snapshots produce real draw data --------

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_RocketIdle_ProducesRealDrawData) {
    auto rules = RealRules();
    auto bundle = RealBundle("rocket");
    auto snap = RealInitSnapshot(rules);

    auto result = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    const auto& draw = *result.value;
    DOMINUS_EXPECT(draw.clip_name == "idle");
    DOMINUS_EXPECT(draw.parts.size() == 19);  // real Rocket part count (Module 3)
    DOMINUS_EXPECT(draw.parts.front().part_name == bundle.parts.DrawOrder().front());
    DOMINUS_EXPECT(draw.parts.back().part_name == bundle.parts.DrawOrder().back());

    const auto* head = FindDraw(draw, "head");
    DOMINUS_EXPECT(head != nullptr);
    // Real Rocket parts.json head frame rect, unmodified pass-through.
    DOMINUS_EXPECT(head->frame_x == 145);
    DOMINUS_EXPECT(head->frame_y == 0);
    DOMINUS_EXPECT(head->frame_w == 130);
    DOMINUS_EXPECT(head->frame_h == 180);
}

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_RocketWalking_SelectsRealWalkClip) {
    auto rules = RealRules();
    auto bundle = RealBundle("rocket");
    auto snap = RealInitSnapshot(rules);
    snap.frame = 40;  // deliberately different from state_frame below --
                       // proves raw_frame tracks state_frame, not frame
    snap.state = HitmFighterState::kWalking;
    snap.state_frame = 5;  // 5 real frames into this walk

    auto result = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->clip_name == "walk");
    DOMINUS_EXPECT(result.value->raw_frame == 5.0);
    // Rocket's real walk clip: loop=true, len=34.
    DOMINUS_EXPECT(result.value->sampled_frame == 5.0);
}

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_RocketJumping_SelectsRealJumpClipByVelocitySign) {
    auto rules = RealRules();
    auto bundle = RealBundle("rocket");
    auto snap = RealInitSnapshot(rules);
    snap.state = HitmFighterState::kJumping;
    snap.grounded = false;

    snap.velocity_y = static_cast<float>(rules.Physics().jump_vel);  // real, negative -> rising
    DOMINUS_EXPECT(snap.velocity_y < 0.0f);
    auto rising = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(rising.ok);
    DOMINUS_EXPECT(rising.value->clip_name == "jumpUp");

    snap.velocity_y = 5.0f;  // falling
    auto falling = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(falling.ok);
    DOMINUS_EXPECT(falling.value->clip_name == "jumpDown");
}

// --- Rocket: real secondary motion, real per-fighter spring constants ----

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_RocketSecondaryMotion_TailFollowsHipWithRocketsOwnRealSpringConstants) {
    // Real Rocket "tail" follow params (distinct from Brooklyn's real
    // values, proving this isn't a Brooklyn-only-tuned constant baked in
    // anywhere): stiffness=0.268, damping=0.784, lagBeats=1.5,
    // maxAngle=50, gravity=0.3. Parent "hip", which the real "idle" clip
    // does author a track for.
    auto rules = RealRules();
    auto bundle = RealBundle("rocket");
    auto idleClip = bundle.animations.Clip("idle");
    DOMINUS_EXPECT(idleClip != nullptr);

    const double kLagBeats = 1.5, kStiffness = 0.268, kGravity = 0.3, kDamping = 0.784, kMaxAngle = 50.0;
    double shadowAngle = 0.0, shadowVelocity = 0.0;
    bool shadowInitialized = false;
    const double kDegToRad = 3.14159265358979323846 / 180.0;

    HitmSecondaryMotionState state;
    for (int frame = 0; frame < 20; ++frame) {
        auto snap = RealInitSnapshot(rules);
        snap.frame = static_cast<uint64_t>(frame);
        // This hand-built scenario represents a fighter idle continuously
        // since frame 0 (state never transitions), so state_frame == frame
        // -- see HitmFighterRuntime.h's "A DELIBERATELY SCOPED EXTENSION"
        // for what state_frame actually tracks (frames since the last
        // real state transition, reset to 0 on one, not the match-wide
        // frame counter animation frame selection used before it existed).
        snap.state_frame = frame;

        auto result = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle, &state);
        DOMINUS_EXPECT(result.ok);

        double sampledFrame = static_cast<double>(frame % 96);  // real Rocket idle: loop=true, len=96
        double hipRot = idleClip->Sample("hip", sampledFrame).rotation_deg;

        double target = hipRot * kLagBeats;
        if (!shadowInitialized) {
            shadowAngle = target;
            shadowVelocity = 0.0;
            shadowInitialized = true;
        }
        shadowVelocity += (target - shadowAngle) * kStiffness;
        shadowVelocity += kGravity * 0.6 * std::sin(shadowAngle * kDegToRad);
        shadowVelocity *= kDamping;
        shadowAngle += shadowVelocity * 1.0;
        if (shadowAngle > kMaxAngle) { shadowAngle = kMaxAngle; shadowVelocity *= -0.35; }
        if (shadowAngle < -kMaxAngle) { shadowAngle = -kMaxAngle; shadowVelocity *= -0.35; }
        double expectedTailRot = shadowAngle - hipRot;

        const auto* tail = FindDraw(*result.value, "tail");
        DOMINUS_EXPECT(tail != nullptr);
        DOMINUS_EXPECT(tail->pose_rotation_deg == expectedTailRot);
    }
}

// --- Static: real idle/walk snapshots + real secondary motion ------------

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_StaticIdle_ProducesRealDrawData) {
    auto rules = RealRules();
    auto bundle = RealBundle("static");
    auto snap = RealInitSnapshot(rules);

    auto result = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    const auto& draw = *result.value;
    DOMINUS_EXPECT(draw.clip_name == "idle");
    DOMINUS_EXPECT(draw.parts.size() == 19);  // real Static part count

    const auto* head = FindDraw(draw, "head");
    DOMINUS_EXPECT(head != nullptr);
    DOMINUS_EXPECT(head->frame_x == 0);
    DOMINUS_EXPECT(head->frame_y == 0);
    DOMINUS_EXPECT(head->frame_w == 161);
    DOMINUS_EXPECT(head->frame_h == 214);
}

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_StaticWalking_SelectsRealWalkClip) {
    auto rules = RealRules();
    auto bundle = RealBundle("static");
    auto snap = RealInitSnapshot(rules);
    snap.frame = 5;
    snap.state = HitmFighterState::kWalking;

    auto result = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->clip_name == "walk");
}

DOMINUS_TEST(HitmSpriteDrawData_MultiFighter_StaticSecondaryMotion_WireFollowsTorsoWithStaticsOwnRealSpringConstants) {
    // Real Static "wireFar" follow params: stiffness=0.184, damping=0.652,
    // lagBeats=2.0, maxAngle=52, gravity=0.4 -- a third, distinct real
    // stiffness/damping pair (Brooklyn 0.118/0.634, Rocket 0.268/0.784,
    // Static 0.184/0.652), each fighter's own DNA-derived spring feel.
    auto rules = RealRules();
    auto bundle = RealBundle("static");
    auto idleClip = bundle.animations.Clip("idle");
    DOMINUS_EXPECT(idleClip != nullptr);

    const double kLagBeats = 2.0, kStiffness = 0.184, kGravity = 0.4, kDamping = 0.652, kMaxAngle = 52.0;
    double shadowAngle = 0.0, shadowVelocity = 0.0;
    bool shadowInitialized = false;
    const double kDegToRad = 3.14159265358979323846 / 180.0;

    HitmSecondaryMotionState state;
    for (int frame = 0; frame < 20; ++frame) {
        auto snap = RealInitSnapshot(rules);
        snap.frame = static_cast<uint64_t>(frame);
        // This hand-built scenario represents a fighter idle continuously
        // since frame 0 (state never transitions), so state_frame == frame
        // -- see HitmFighterRuntime.h's "A DELIBERATELY SCOPED EXTENSION"
        // for what state_frame actually tracks (frames since the last
        // real state transition, reset to 0 on one, not the match-wide
        // frame counter animation frame selection used before it existed).
        snap.state_frame = frame;

        auto result = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle, &state);
        DOMINUS_EXPECT(result.ok);

        double sampledFrame = static_cast<double>(frame % 66);  // real Static idle: loop=true, len=66
        double torsoRot = idleClip->Sample("torso", sampledFrame).rotation_deg;

        double target = torsoRot * kLagBeats;
        if (!shadowInitialized) {
            shadowAngle = target;
            shadowVelocity = 0.0;
            shadowInitialized = true;
        }
        shadowVelocity += (target - shadowAngle) * kStiffness;
        shadowVelocity += kGravity * 0.6 * std::sin(shadowAngle * kDegToRad);
        shadowVelocity *= kDamping;
        shadowAngle += shadowVelocity * 1.0;
        if (shadowAngle > kMaxAngle) { shadowAngle = kMaxAngle; shadowVelocity *= -0.35; }
        if (shadowAngle < -kMaxAngle) { shadowAngle = -kMaxAngle; shadowVelocity *= -0.35; }
        double expectedWireRot = shadowAngle - torsoRot;

        const auto* wire = FindDraw(*result.value, "wireFar");
        DOMINUS_EXPECT(wire != nullptr);
        DOMINUS_EXPECT(wire->pose_rotation_deg == expectedWireRot);
    }
}
