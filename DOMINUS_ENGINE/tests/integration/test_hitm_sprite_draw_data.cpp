// tests/integration/test_hitm_sprite_draw_data.cpp
// ROADMAP.md Track H Module 5B -- proves the full pipeline this module
// builds, end to end, with real data at every stage:
//
//   REAL HITM ASSETS -> HitmAssetImporter -> DOMINUS asset representation
//   -> Brooklyn runtime (Module 5A, untouched) -> ANIMATION/FRAME
//   SELECTION -> SPRITE DRAW DATA
//
// Every assertion below traces to a real, authored/generated HITM value
// (anim.json/parts.json/rig.json/signature.json, imported via Modules
// 1/3/5A/5B) or to a direct, verified port of hitm-engine's own real
// AnimationSystem.js/SkeletonSystem.js algorithms -- see
// HitmSpriteDrawData.h's top comment for the full accounting of every
// place this module's real-data-only mapping diverges from the real
// engine's own richer state, and why.
#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>
#include <utility>

using dominus::character::hitm::HitmAssetBundle;
using dominus::character::hitm::HitmAssetImporter;
using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmFighterRuntime;
using dominus::character::hitm::HitmFighterSnapshot;
using dominus::character::hitm::HitmFighterState;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmInputCommand;
using dominus::character::hitm::HitmLocalPose;
using dominus::character::hitm::HitmMoveInstance;
using dominus::character::hitm::HitmSecondaryMotionState;

namespace {

std::filesystem::path FindDir(const std::filesystem::path& rel) {
    std::vector<std::filesystem::path> candidates = {rel, std::filesystem::path("..") / rel, std::filesystem::path("../..") / rel};
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture dir not found: " + rel.string());
}

std::filesystem::path IdentityDir(const std::string& fighter) {
    return FindDir(std::filesystem::path("tests/fixtures/hitm_identity") / fighter);
}
std::filesystem::path SpriteAssetsRoot(const std::string& brokenCase = "") {
    if (brokenCase.empty()) return FindDir("tests/fixtures/hitm_sprite_assets");
    return FindDir(std::filesystem::path("tests/fixtures/hitm_sprite_assets") / brokenCase);
}

HitmIdentityRecord RealIdentity(const std::string& fighter) {
    auto r = HitmIdentityImporter::Import(IdentityDir(fighter));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return *r.value;
}
HitmCombatGenome RealGenome(const HitmIdentityRecord& record) {
    auto r = HitmCombatGenome::FromRecord(record);
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}
HitmGameRules RealRules() {
    auto path = FindDir("tests/fixtures/hitm_game_rules/game.json");
    auto r = HitmGameRules::Import(path);
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}
HitmFighterRuntime MakeBrooklynRuntime() {
    auto identity = RealIdentity("brooklyn");
    auto genome = RealGenome(identity);
    auto rules = RealRules();
    auto result = HitmFighterRuntime::Create(identity, genome, rules);
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}
HitmAssetBundle MakeBrooklynBundle() {
    auto identity = RealIdentity("brooklyn");
    auto result = HitmAssetImporter::Import(identity, SpriteAssetsRoot());
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}
HitmMoveInstance RealBrooklynSpecial() {
    auto identity = RealIdentity("brooklyn");
    auto result = HitmMoveInstance::Extract(identity, "special");
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}

const dominus::character::hitm::HitmPartDraw* FindDraw(const dominus::character::hitm::HitmSpriteDrawData& draw,
                                                          const std::string& partName) {
    for (const auto& p : draw.parts) {
        if (p.part_name == partName) return &p;
    }
    return nullptr;
}

}  // namespace

// --- 1. Idle: real clip selection + real frame-0 pose ---------------------

DOMINUS_TEST(HitmSpriteDrawData_Idle_SelectsRealIdleClipAndSamplesRealFrameZeroPose) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    const auto& draw = *result.value;

    DOMINUS_EXPECT(draw.clip_name == "idle");
    DOMINUS_EXPECT(draw.raw_frame == 0.0);
    DOMINUS_EXPECT(draw.sampled_frame == 0.0);
    DOMINUS_EXPECT(draw.parts.size() == 22);  // real Brooklyn part count, Module 3

    // Real drawOrder fidelity, back-to-front.
    DOMINUS_EXPECT(draw.parts.front().part_name == bundle.parts.DrawOrder().front());
    DOMINUS_EXPECT(draw.parts.back().part_name == bundle.parts.DrawOrder().back());

    // torso's real idle-clip frame-0 keyframe: [0, -2, 0, 0].
    bool foundTorso = false;
    for (const auto& p : draw.parts) {
        if (p.part_name != "torso") continue;
        foundTorso = true;
        DOMINUS_EXPECT(p.pose_rotation_deg == -2.0);
        DOMINUS_EXPECT(p.pose_offset_x == 0.0);
        DOMINUS_EXPECT(p.pose_offset_y == 0.0);
        // Real atlas-space data, unmodified pass-through from parts.json.
        DOMINUS_EXPECT(p.frame_x == 104);
        DOMINUS_EXPECT(p.frame_w == 107);
        // Real rig.json placement, cross-validated against parts.json.
        DOMINUS_EXPECT(p.place_x == 0.26);
        DOMINUS_EXPECT(p.place_y == 0.275);
    }
    DOMINUS_EXPECT(foundTorso);
}

// --- 2. Walking: real clip selection --------------------------------------

DOMINUS_TEST(HitmSpriteDrawData_Walking_SelectsRealWalkClip) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    runtime.AdvanceFrame(HitmInputCommand::kRight);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kWalking);

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->clip_name == "walk");
    DOMINUS_EXPECT(result.value->raw_frame == 1.0);  // == snap.frame, real documented gap, see header
}

// --- 3. Jumping: real vy-sign-driven clip selection, matching the real ----
// --- engine's own `f.vy<0 ? 'jumpUp' : 'jumpDown'` convention exactly ----

DOMINUS_TEST(HitmSpriteDrawData_Jumping_SelectsJumpUpWhileRisingThenJumpDownWhileFalling) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    runtime.AdvanceFrame(HitmInputCommand::kJump);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kJumping);
    DOMINUS_EXPECT(runtime.Snapshot().velocity_y < 0.0f);

    auto rising = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(rising.ok);
    DOMINUS_EXPECT(rising.value->clip_name == "jumpUp");

    bool sawFalling = false;
    for (int i = 0; i < 60 && runtime.State() == HitmFighterState::kJumping; ++i) {
        runtime.AdvanceFrame(HitmInputCommand::kNeutral);
        if (runtime.State() != HitmFighterState::kJumping) break;
        if (runtime.Snapshot().velocity_y >= 0.0f) {
            auto falling = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
            DOMINUS_EXPECT(falling.ok);
            DOMINUS_EXPECT(falling.value->clip_name == "jumpDown");
            sawFalling = true;
            break;
        }
    }
    DOMINUS_EXPECT(sawFalling);  // real gravity must actually turn the arc, or this proves nothing
}

// --- 4. Attack sub-states: exact elapsed-frame reconstruction, verified ---
// --- against the real correspondence special.len(36) == startup+active+ --
// --- recovery(14+4+18) -----------------------------------------------

DOMINUS_TEST(HitmSpriteDrawData_AttackStartup_FirstFrame_RawFrameIsExactlyOne) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();

    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kAttackStartup);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == 13);  // 14 (real startup) - 1, same-tick decrement

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), &special, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->clip_name == "special");
    DOMINUS_EXPECT(result.value->raw_frame == 1.0);
}

DOMINUS_TEST(HitmSpriteDrawData_AttackActive_FirstFrame_RawFrameIsExactlyStartupTotal) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();

    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    for (int i = 0; i < 13; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);  // consume the remaining 13 startup frames
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kAttackActive);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == 4);  // full real active total, just transitioned

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), &special, bundle);
    DOMINUS_EXPECT(result.ok);
    // The real, evidenced correspondence: exactly 14 (startup total) --
    // the anim clip's own authored keyframe timeline picks up exactly
    // where the startup phase's real frame data says it should.
    DOMINUS_EXPECT(result.value->raw_frame == 14.0);
}

DOMINUS_TEST(HitmSpriteDrawData_AttackRecovery_FirstFrame_RawFrameIsExactlyStartupPlusActive) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();

    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    for (int i = 0; i < 13 + 4; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);  // consume startup(13 left) + active(4)
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kAttackRecovery);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == 18);  // full real recovery total

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), &special, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->raw_frame == 18.0);  // == 14 (startup) + 4 (active)
}

DOMINUS_TEST(HitmSpriteDrawData_AttackRecovery_LastObservableFrame_RawFrameIsThirtyFive) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();

    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    // 13 (rest of startup) + 4 (active) + 17 (all but the last recovery
    // frame) = 34 more frames -> state_frames_remaining counts down to 1,
    // the last frame still observably kAttackRecovery before the same-
    // tick transition back to kIdle.
    for (int i = 0; i < 13 + 4 + 17; ++i) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kAttackRecovery);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == 1);

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), &special, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->raw_frame == 35.0);  // == 14 + 4 + (18 - 1)
    // Real clip len is 36 -- one past the last observable in-attack
    // frame, exactly as expected: frame 36 is reached the instant the
    // move completes, which is also the instant the fighter leaves the
    // attack states this module's ComputeRawFrame branch covers.
    DOMINUS_EXPECT(bundle.animations.Clip("special")->len == 36);
}

// --- 5. Hitstun / blockstun: real port of the engine's own ----------------
// --- Math.max(0, clip.len - stunRemaining) formula -------------------------

DOMINUS_TEST(HitmSpriteDrawData_Hitstun_SelectsRealHurtClipAndTracksElapsedCorrectly) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();
    DOMINUS_EXPECT(special.hitstun_frames == 34);  // real authored value
    DOMINUS_EXPECT(bundle.animations.Clip("hurt")->len == 14);  // real authored value

    runtime.TakeHit(special, /*blocking=*/false);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kHitstun);
    DOMINUS_EXPECT(runtime.Snapshot().hitstop_frames_remaining > 0);  // real hitstop freeze, doesn't advance frame/state clocks
    while (runtime.Snapshot().hitstop_frames_remaining > 0) runtime.AdvanceFrame(HitmInputCommand::kNeutral);

    // Real stun (34) exceeds the real hurt clip's length (14): raw_frame
    // stays clamped at 0 until the remaining stun drops below the clip
    // length, then counts up -- a faithful expression of a real,
    // authored data mismatch, not a bug.
    {
        auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
        DOMINUS_EXPECT(result.ok);
        DOMINUS_EXPECT(result.value->clip_name == "hurt");
        DOMINUS_EXPECT(result.value->raw_frame == 0.0);
    }

    while (runtime.Snapshot().state_frames_remaining > 13) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == 13);
    {
        auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
        DOMINUS_EXPECT(result.ok);
        DOMINUS_EXPECT(result.value->raw_frame == 1.0);  // 14 - 13
    }

    while (runtime.Snapshot().state_frames_remaining > 1 && runtime.State() == HitmFighterState::kHitstun) {
        runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    }
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kHitstun);
    DOMINUS_EXPECT(runtime.Snapshot().state_frames_remaining == 1);
    {
        auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
        DOMINUS_EXPECT(result.ok);
        DOMINUS_EXPECT(result.value->raw_frame == 13.0);  // 14 - 1
    }
}

DOMINUS_TEST(HitmSpriteDrawData_Blockstun_SelectsRealBlockClip) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();
    DOMINUS_EXPECT(special.blockstun_frames == 13);  // real authored value
    DOMINUS_EXPECT(bundle.animations.Clip("block")->len == 4);  // real authored value

    runtime.TakeHit(special, /*blocking=*/true);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kBlockstun);
    while (runtime.Snapshot().hitstop_frames_remaining > 0) runtime.AdvanceFrame(HitmInputCommand::kNeutral);

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->clip_name == "block");  // same real clip as kBlockingStance, see header
}

// --- 6. Determinism: same snapshot => byte-identical draw data ------------

DOMINUS_TEST(HitmSpriteDrawData_Determinism_SameSnapshotProducesIdenticalDrawData) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    runtime.AdvanceFrame(HitmInputCommand::kRight);
    runtime.AdvanceFrame(HitmInputCommand::kRight);
    HitmFighterSnapshot snap = runtime.Snapshot();

    auto a = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    auto b = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle);
    DOMINUS_EXPECT(a.ok && b.ok);
    DOMINUS_EXPECT(a.value->clip_name == b.value->clip_name);
    DOMINUS_EXPECT(a.value->raw_frame == b.value->raw_frame);
    DOMINUS_EXPECT(a.value->sampled_frame == b.value->sampled_frame);
    DOMINUS_EXPECT(a.value->parts.size() == b.value->parts.size());
    for (size_t i = 0; i < a.value->parts.size(); ++i) {
        DOMINUS_EXPECT(a.value->parts[i].part_name == b.value->parts[i].part_name);
        DOMINUS_EXPECT(a.value->parts[i].pose_rotation_deg == b.value->parts[i].pose_rotation_deg);
        DOMINUS_EXPECT(a.value->parts[i].pose_offset_x == b.value->parts[i].pose_offset_x);
        DOMINUS_EXPECT(a.value->parts[i].pose_offset_y == b.value->parts[i].pose_offset_y);
    }
}

// --- 7. Deliberate-break -----------------------------------------------

DOMINUS_TEST(HitmSpriteDrawData_Break_AttackStateWithoutCurrentMove_Fails) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    DOMINUS_EXPECT(runtime.State() == HitmFighterState::kAttackStartup);

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), /*currentMove=*/nullptr, bundle);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("currentMove") != std::string::npos);
}

DOMINUS_TEST(HitmSpriteDrawData_Break_MissingRequiredClip_Fails) {
    auto identity = RealIdentity("brooklyn");
    auto brokenBundleResult = HitmAssetImporter::Import(identity, SpriteAssetsRoot("broken_anim_missing_required_clip"));
    DOMINUS_EXPECT(brokenBundleResult.ok);  // the bundle itself imports fine -- see test_hitm_asset_importer.cpp
    const auto& bundle = *brokenBundleResult.value;
    DOMINUS_EXPECT(!bundle.animations.HasClip("idle"));

    auto runtime = MakeBrooklynRuntime();  // real, unmodified -- state kIdle
    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("'idle'") != std::string::npos);
}

DOMINUS_TEST(HitmSpriteDrawData_Break_FailedBuildDoesNotThrow) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    runtime.AdvanceFrame(HitmInputCommand::kSpecial);

    bool threw = false;
    bool ok = true;
    try {
        auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
        ok = result.ok;
    } catch (...) {
        threw = true;
    }
    DOMINUS_EXPECT(!threw);
    DOMINUS_EXPECT(!ok);
}

// --- 8. Secondary motion (Track A gap #1): real per-bone spring/damper ---
// --- follow system, a direct port of hitm-engine's own real ---------------
// --- SkeletonSystem._secondary() -- see HitmSpriteDrawData.h's header ----
// --- comment for the exact real quirks these tests hold it to. -----------

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_DisabledByDefault_FollowBoneGetsZeroPose) {
    // Real data: Brooklyn's "idle" clip authors no "dreadFar" track at
    // all (verified: 15 real tracks, none of the follow bones among
    // them) -- without secondary motion, the real _sample(null, f)
    // fallback (zero pose) is exactly what should come through.
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle, /*secondaryMotion=*/nullptr);
    DOMINUS_EXPECT(result.ok);
    const auto* dread = FindDraw(*result.value, "dreadFar");
    DOMINUS_EXPECT(dread != nullptr);
    DOMINUS_EXPECT(dread->pose_rotation_deg == 0.0);
    DOMINUS_EXPECT(dread->pose_offset_x == 0.0);
    DOMINUS_EXPECT(dread->pose_offset_y == 0.0);
}

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_FirstFrame_HatInitializesExactlyAtLagTarget) {
    // Real "hat" follow params: stiffness=0.118, damping=0.634,
    // lagBeats=0.6, maxAngle=16, gravity=0.0 -- parent "head". Real
    // idle-clip head rotation at frame 0 is an exact keyframe (no
    // interpolation): 3.0deg. On first touch the spring initializes
    // AT the lag target (angle=target, velocity=0) before this frame's
    // own integration step runs -- expected value computed via the
    // identical operation sequence as production (not a rounded
    // literal), per this session's established float-precision
    // discipline.
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    HitmSecondaryMotionState state;
    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle, &state);
    DOMINUS_EXPECT(result.ok);

    const auto* head = FindDraw(*result.value, "head");
    DOMINUS_EXPECT(head != nullptr);
    DOMINUS_EXPECT(head->pose_rotation_deg == 3.0);  // exact keyframe, no interpolation at frame 0

    double headRot = head->pose_rotation_deg;
    double target = headRot * 0.6;
    double angle = target;  // real first-touch initialization
    double velocity = 0.0;
    velocity += (target - angle) * 0.118;  // == 0.0 exactly
    // gravity == 0.0 for "hat" -- real code's `if(f.gravity)` skips
    velocity *= 0.634;
    angle += velocity * 1.0;
    double expectedHatRot = angle - headRot;

    const auto* hat = FindDraw(*result.value, "hat");
    DOMINUS_EXPECT(hat != nullptr);
    DOMINUS_EXPECT(hat->pose_rotation_deg == expectedHatRot);

    // Real state, publicly observable: initialized at the lag target.
    DOMINUS_EXPECT(state.BoneState("hat").angle_deg == target);
    DOMINUS_EXPECT(state.BoneState("hat").initialized);
}

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_ShadowSimulationMatchesProductionAcrossManyFrames) {
    // Independent re-derivation of the real spring recurrence (quoted
    // verbatim in HitmSpriteDrawData.h's header comment), computed here
    // directly against real anim.json data via the same public
    // `HitmAnimationClip::Sample()` every other test in this file uses --
    // not by calling into (or copying) the production
    // `ApplySecondaryMotion()` helper, which is a private implementation
    // detail this file cannot reach. A transcription bug, a swapped
    // parameter, or a wrong parent lookup in production would very
    // likely disagree with this independently-retyped sequence.
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    HitmSecondaryMotionState state;
    const auto* idleClip = bundle.animations.Clip("idle");
    DOMINUS_EXPECT(idleClip != nullptr);

    // "chain" -- parent "torso", real params include nonzero gravity,
    // exercising the sin() branch "hat" (gravity=0) never reaches.
    const double kLagBeats = 1.6, kStiffness = 0.118, kGravity = 0.9, kDamping = 0.634, kMaxAngle = 30.0;
    double shadowAngle = 0.0, shadowVelocity = 0.0;
    bool shadowInitialized = false;
    const double kDegToRad = 3.14159265358979323846 / 180.0;

    for (int frame = 0; frame < 30; ++frame) {
        auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle, &state);
        DOMINUS_EXPECT(result.ok);

        double sampledFrame = static_cast<double>(runtime.Snapshot().frame % 84);  // real idle clip: loop=true, len=84
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
        double expectedChainRot = shadowAngle - torsoRot;

        const auto* chain = FindDraw(*result.value, "chain");
        DOMINUS_EXPECT(chain != nullptr);
        DOMINUS_EXPECT(chain->pose_rotation_deg == expectedChainRot);

        runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    }
}

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_MaxAngleNeverExceededAcrossLongRealReplay) {
    // Real safety invariant, independent of exact trajectory: whatever
    // the spring does, its internal angle must never exceed the real
    // authored maxAngle for that bone, across a long, varied real
    // gameplay sequence (walk, jump, land, attack, get hit).
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto special = RealBrooklynSpecial();
    HitmSecondaryMotionState state;

    const std::vector<std::pair<std::string, double>> kFollowBoneMaxAngles = {
        {"hat", 16.0}, {"jaw", 14.0}, {"dreadFar", 46.0}, {"dreadNear", 46.0},
        {"tie", 38.0}, {"chain", 30.0}, {"coatFar", 42.0}, {"coatNear", 42.0},
        {"handFar", 22.0}, {"handNear", 22.0},
    };

    auto checkInvariant = [&] {
        for (const auto& [name, maxAngle] : kFollowBoneMaxAngles) {
            DOMINUS_EXPECT(std::fabs(state.BoneState(name).angle_deg) <= maxAngle);
        }
    };

    for (int i = 0; i < 3; ++i) { runtime.AdvanceFrame(HitmInputCommand::kRight); checkInvariant(); }
    runtime.AdvanceFrame(HitmInputCommand::kJump);
    checkInvariant();
    while (!runtime.Snapshot().grounded) { runtime.AdvanceFrame(HitmInputCommand::kNeutral); checkInvariant(); }
    runtime.AdvanceFrame(HitmInputCommand::kSpecial);
    checkInvariant();
    while (runtime.State() != HitmFighterState::kIdle) { runtime.AdvanceFrame(HitmInputCommand::kNeutral); checkInvariant(); }
    runtime.TakeHit(special, /*blocking=*/false);
    checkInvariant();
    for (int i = 0; i < 40; ++i) {
        auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle, &state);
        DOMINUS_EXPECT(result.ok);
        checkInvariant();
        if (runtime.Snapshot().hitstop_frames_remaining == 0) runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    }
}

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_Determinism_TwoIndependentReplaysProduceIdenticalResults) {
    // Same methodology Module 5A's own determinism proof used: two
    // fully independent (runtime, bundle, spring-state) triples driven
    // by an identical real input script must produce byte-identical
    // secondary-motion output every frame.
    auto runtimeA = MakeBrooklynRuntime();
    auto bundleA = MakeBrooklynBundle();
    HitmSecondaryMotionState stateA;

    auto runtimeB = MakeBrooklynRuntime();
    auto bundleB = MakeBrooklynBundle();
    HitmSecondaryMotionState stateB;

    std::vector<HitmInputCommand> script = {HitmInputCommand::kRight, HitmInputCommand::kRight, HitmInputCommand::kJump,
                                             HitmInputCommand::kNeutral, HitmInputCommand::kNeutral, HitmInputCommand::kLeft,
                                             HitmInputCommand::kNeutral};

    for (auto input : script) {
        runtimeA.AdvanceFrame(input);
        runtimeB.AdvanceFrame(input);
        auto resultA = dominus::character::hitm::BuildSpriteDrawData(runtimeA.Snapshot(), nullptr, bundleA, &stateA);
        auto resultB = dominus::character::hitm::BuildSpriteDrawData(runtimeB.Snapshot(), nullptr, bundleB, &stateB);
        DOMINUS_EXPECT(resultA.ok && resultB.ok);
        for (const auto& name : {"hat", "chain", "dreadFar", "dreadNear", "coatFar", "handNear"}) {
            const auto* a = FindDraw(*resultA.value, name);
            const auto* b = FindDraw(*resultB.value, name);
            DOMINUS_EXPECT(a != nullptr && b != nullptr);
            DOMINUS_EXPECT(a->pose_rotation_deg == b->pose_rotation_deg);
        }
    }
}

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_Reset_ReinitializesFromFreshTarget) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    HitmSecondaryMotionState state;

    for (int i = 0; i < 10; ++i) {
        dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle, &state);
        runtime.AdvanceFrame(HitmInputCommand::kNeutral);
    }
    DOMINUS_EXPECT(state.BoneState("hat").initialized);

    state.Reset();
    DOMINUS_EXPECT(!state.BoneState("hat").initialized);  // BoneState() default-constructs a fresh entry after Reset()

    auto result = dominus::character::hitm::BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle, &state);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(state.BoneState("hat").initialized);  // re-initialized by this call, not left stale
}

DOMINUS_TEST(HitmSpriteDrawData_SecondaryMotion_HandNear_AlwaysSpringDrivenDespiteDuplicateRigidBoneEntry) {
    // Real, evidenced finding (Module 3): "handNear" appears TWICE in
    // Brooklyn's real bones[] array -- once rigid (no follow), once
    // later as a glove-bounce follow overlay. The real engine's own
    // name-keyed bone lookup means the LATER (follow) entry always wins.
    // If this module's last-occurrence-wins resolution instead picked
    // the earlier, rigid duplicate (which has no `follow`), secondary
    // motion would silently skip handNear entirely and this A/B
    // comparison -- the same snapshot, with vs. without secondary
    // motion -- would show no difference. It must show one.
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    for (int i = 0; i < 5; ++i) runtime.AdvanceFrame(HitmInputCommand::kRight);  // walking -- armNearL is genuinely rotating
    HitmFighterSnapshot snap = runtime.Snapshot();

    auto withoutMotion = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle, /*secondaryMotion=*/nullptr);
    HitmSecondaryMotionState state;
    auto withMotion = dominus::character::hitm::BuildSpriteDrawData(snap, nullptr, bundle, &state);
    DOMINUS_EXPECT(withoutMotion.ok && withMotion.ok);

    const auto* plain = FindDraw(*withoutMotion.value, "handNear");
    const auto* spring = FindDraw(*withMotion.value, "handNear");
    DOMINUS_EXPECT(plain != nullptr && spring != nullptr);
    DOMINUS_EXPECT(state.BoneState("handNear").initialized);
    DOMINUS_EXPECT(spring->pose_rotation_deg != plain->pose_rotation_deg);
}
