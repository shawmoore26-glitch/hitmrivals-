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
using dominus::character::hitm::HitmMoveInstance;

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
