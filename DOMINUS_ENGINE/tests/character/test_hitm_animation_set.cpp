// tests/character/test_hitm_animation_set.cpp
// ROADMAP.md Track H Module 5B. Fixtures under
// tests/fixtures/hitm_sprite_assets/ are real, byte-identical copies of
// HITM's generated anim.json (see that directory's README.md for
// provenance) plus deliberate single-field mutations of Brooklyn's real
// file.
#include "CHARACTER/HitmBridge/HitmAnimationSet.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmAnimationClip;
using dominus::character::hitm::HitmAnimationSet;

namespace {

std::filesystem::path CharacterDir(const std::string& root, const std::string& fighter) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_sprite_assets") / root / "data/characters" / fighter,
        std::filesystem::path("../tests/fixtures/hitm_sprite_assets") / root / "data/characters" / fighter,
        std::filesystem::path("../../tests/fixtures/hitm_sprite_assets") / root / "data/characters" / fighter,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: " + root + "/" + fighter);
}

std::filesystem::path RealCharacterDir(const std::string& fighter) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_sprite_assets/data/characters") / fighter,
        std::filesystem::path("../tests/fixtures/hitm_sprite_assets/data/characters") / fighter,
        std::filesystem::path("../../tests/fixtures/hitm_sprite_assets/data/characters") / fighter,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: data/characters/" + fighter);
}

}  // namespace

// --- Real data: Brooklyn -------------------------------------------------

DOMINUS_TEST(HitmAnimationSet_ImportsRealBrooklyn) {
    auto result = HitmAnimationSet::Import(RealCharacterDir("brooklyn"));
    DOMINUS_EXPECT(result.ok);
    auto& set = *result.value;
    DOMINUS_EXPECT(set.FighterId() == "brooklyn");
    DOMINUS_EXPECT(set.HasClip("idle"));
    DOMINUS_EXPECT(set.HasClip("walk"));
    DOMINUS_EXPECT(set.HasClip("special"));
    DOMINUS_EXPECT(set.HasClip("hurt"));
    DOMINUS_EXPECT(!set.HasClip("no_such_clip"));
}

DOMINUS_TEST(HitmAnimationSet_BrooklynIdleClipMatchesRealData) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* idle = set.Clip("idle");
    DOMINUS_EXPECT(idle != nullptr);
    DOMINUS_EXPECT(idle->loop == true);
    DOMINUS_EXPECT(idle->len == 84);
    DOMINUS_EXPECT(idle->tracks.size() == 15);
    DOMINUS_EXPECT(idle->tracks.count("torso") == 1);
    DOMINUS_EXPECT(idle->tracks.at("torso").size() == 3);
}

// The real, evidenced correspondence this module's report documents:
// Brooklyn's real "special" clip's authored len (36) exactly equals his
// real signature.json special move's startup+active+recovery
// (14+4+18=36) -- not asserted, verified.
DOMINUS_TEST(HitmAnimationSet_BrooklynSpecialClipLenMatchesRealMoveFrameTotal) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* special = set.Clip("special");
    DOMINUS_EXPECT(special != nullptr);
    DOMINUS_EXPECT(special->loop == false);
    DOMINUS_EXPECT(special->len == 36);  // == 14 (startup) + 4 (active) + 18 (recovery), see HitmMoveInstance's real data
}

DOMINUS_TEST(HitmAnimationSet_BrooklynHurtClipMatchesRealData) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* hurt = set.Clip("hurt");
    DOMINUS_EXPECT(hurt != nullptr);
    DOMINUS_EXPECT(hurt->loop == false);
    DOMINUS_EXPECT(hurt->len == 14);
}

// --- Sample(): direct port of hitm-engine's SkeletonSystem._sample() ----

DOMINUS_TEST(HitmAnimationClip_Sample_ClampsBelowFirstKeyframe) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* idle = set.Clip("idle");
    // torso's real first keyframe: [0, -2, 0, 0]
    auto pose = idle->Sample("torso", -5.0);
    DOMINUS_EXPECT(pose.rotation_deg == -2.0);
    DOMINUS_EXPECT(pose.offset_x == 0.0);
    DOMINUS_EXPECT(pose.offset_y == 0.0);
}

DOMINUS_TEST(HitmAnimationClip_Sample_ClampsAboveLastKeyframe) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* idle = set.Clip("idle");
    // torso's real last keyframe: [84, -2, 0, 0]
    auto pose = idle->Sample("torso", 200.0);
    DOMINUS_EXPECT(pose.rotation_deg == -2.0);
}

DOMINUS_TEST(HitmAnimationClip_Sample_ExactKeyframeReturnsExactValue) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* idle = set.Clip("idle");
    // torso's real middle keyframe: [42, 3, 0, 0]
    auto pose = idle->Sample("torso", 42.0);
    DOMINUS_EXPECT(pose.rotation_deg == 3.0);
}

DOMINUS_TEST(HitmAnimationClip_Sample_InterpolatesBetweenKeyframesWithSmoothstep) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* idle = set.Clip("idle");
    // torso: [0,-2,0,0] -> [42,3,0,0]. Halfway (t=0.5) under smoothstep
    // s = t*t*(3-2t) = 0.5 exactly, so the midpoint is the linear midpoint
    // here specifically (t=0.5 is smoothstep's one fixed point) --
    // rotation = -2 + (3-(-2))*0.5 = 0.5.
    auto pose = idle->Sample("torso", 21.0);
    DOMINUS_EXPECT(pose.rotation_deg > 0.4 && pose.rotation_deg < 0.6);
}

// A first pass at this importer required strictly-increasing keyframe
// frame numbers and immediately failed to import Brooklyn's own real
// data -- see HitmAnimationSet.h's header comment for the full account.
// This test proves the real, out-of-order data both imports AND samples
// correctly, using the exact real track the failure was found against.
DOMINUS_TEST(HitmAnimationClip_Sample_RealOutOfOrderKeyframesStillSampleCorrectly) {
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* light1 = set.Clip("light1");
    DOMINUS_EXPECT(light1 != nullptr);
    // Real track, verbatim: [[0,0,0,0],[1,12,0,0],[0,-18,0,0],[11,0,0,0]].
    const auto& track = light1->tracks.at("legFarU");
    DOMINUS_EXPECT(track.size() == 4);
    DOMINUS_EXPECT(track[2].frame == 0);  // the real, out-of-order entry, imported losslessly

    // frame<=0 clamps to the array's first entry (index 0, value 0) --
    // matching the real engine's own index-based (not value-based) clamp.
    DOMINUS_EXPECT(light1->Sample("legFarU", -1.0).rotation_deg == 0.0);
    // frame==1 brackets between array entries 0 and 1 (0deg -> 12deg);
    // the real, later, out-of-order frame-0 entry never satisfies the
    // bracket condition for any query in [0,11], so it is correctly inert.
    DOMINUS_EXPECT(light1->Sample("legFarU", 1.0).rotation_deg == 12.0);
}

DOMINUS_TEST(HitmAnimationClip_Sample_MissingTrackReturnsZeroPose) {
    // Real fallback hitm-engine's own _sample(null, f) uses -- verified
    // against Brooklyn's real "block" clip (11 tracks, not all 22 real
    // parts -- e.g. "dreadFar" is not directly authored there, driven by
    // the real secondary-motion follow system instead, out of this
    // module's Phase 1 scope).
    auto set = *HitmAnimationSet::Import(RealCharacterDir("brooklyn")).value;
    const auto* block = set.Clip("block");
    DOMINUS_EXPECT(block != nullptr);
    DOMINUS_EXPECT(block->tracks.count("dreadFar") == 0);
    auto pose = block->Sample("dreadFar", 2.0);
    DOMINUS_EXPECT(pose.rotation_deg == 0.0);
    DOMINUS_EXPECT(pose.offset_x == 0.0);
    DOMINUS_EXPECT(pose.offset_y == 0.0);
}

// --- Real data: Rocket and Static (animation-authoring covers moves ------
// --- whose real combat data is incomplete -- a real, evidenced finding) --

DOMINUS_TEST(HitmAnimationSet_RocketHasRealSpecialClipDespiteIncompleteCombatData) {
    // Module 5A's own real finding: Rocket's real signature.json
    // "special" lacks blockstun/range/height (HitmMoveInstance::Extract
    // refuses it). This is independent of animation authoring -- Rocket's
    // real anim.json DOES have a "special" clip. Asset import and combat-
    // data import are separately complete/incomplete per fighter; proving
    // that is exactly this test's point.
    auto result = HitmAnimationSet::Import(RealCharacterDir("rocket"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->HasClip("special"));
}

DOMINUS_TEST(HitmAnimationSet_ImportsRealStatic) {
    auto result = HitmAnimationSet::Import(RealCharacterDir("static"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->HasClip("idle"));
    DOMINUS_EXPECT(result.value->HasClip("special"));
}

// --- Deliberate-break: every real-world corruption mode must fail loud --

DOMINUS_TEST(HitmAnimationSet_Break_MalformedJson_Fails) {
    auto result = HitmAnimationSet::Import(CharacterDir("broken_anim_malformed_json", "brooklyn"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("parse error") != std::string::npos);
}

DOMINUS_TEST(HitmAnimationSet_Break_MissingIdleClip_HasClipIsFalse) {
    // Import() itself does not require any particular clip to exist
    // (anim.json's schema doesn't mandate one) -- the real requirement
    // (a fighter must have 'idle') is enforced by
    // HitmSpriteDrawData::BuildSpriteDrawData, not this importer. Prove
    // the missing-clip data survives losslessly into HasClip()/Clip()
    // here; the downstream failure is covered by
    // tests/integration/test_hitm_sprite_draw_data.cpp.
    auto result = HitmAnimationSet::Import(CharacterDir("broken_anim_missing_required_clip", "brooklyn"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(!result.value->HasClip("idle"));
    DOMINUS_EXPECT(result.value->HasClip("walk"));  // everything else survives untouched
}

DOMINUS_TEST(HitmAnimationSet_Break_MissingDirectory_Fails) {
    auto result = HitmAnimationSet::Import(std::filesystem::path("tests/fixtures/hitm_sprite_assets/does_not_exist"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("does not exist") != std::string::npos);
}
