// tests/character/test_hitm_asset_importer.cpp
// ROADMAP.md Track H Module 5B. Exercises the full "REAL HITM ASSETS ->
// DOMINUS ASSET REPRESENTATION" discovery/validation/assembly stage
// end to end, against the real hitm-engine directory layout mirrored
// under tests/fixtures/hitm_sprite_assets/ (see that directory's
// README.md for provenance) and against Module 1's real, already-
// imported identity fixtures.
#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmAssetBundle;
using dominus::character::hitm::HitmAssetImporter;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::core::json::Value;

namespace {

std::filesystem::path FindDir(const std::filesystem::path& rel) {
    std::vector<std::filesystem::path> candidates = {rel, std::filesystem::path("..") / rel, std::filesystem::path("../..") / rel};
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: " + rel.string());
}

std::filesystem::path RealIdentityDir(const std::string& fighter) {
    return FindDir(std::filesystem::path("tests/fixtures/hitm_identity") / fighter);
}

std::filesystem::path SpriteAssetsRoot(const std::string& brokenCase = "") {
    if (brokenCase.empty()) return FindDir("tests/fixtures/hitm_sprite_assets");
    return FindDir(std::filesystem::path("tests/fixtures/hitm_sprite_assets") / brokenCase);
}

HitmIdentityRecord RealBrooklynIdentity() { return *HitmIdentityImporter::Import(RealIdentityDir("brooklyn")).value; }

}  // namespace

// --- Real data: full discovery + validation + assembly, end to end -------

DOMINUS_TEST(HitmAssetImporter_ImportsRealBrooklynBundleEndToEnd) {
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(result.ok);
    const HitmAssetBundle& bundle = *result.value;

    DOMINUS_EXPECT(bundle.fighter_id == "brooklyn");
    DOMINUS_EXPECT(bundle.atlas_pixel_width == 1024);
    DOMINUS_EXPECT(bundle.atlas_pixel_height == 269);
    DOMINUS_EXPECT(bundle.parts.Parts().size() == 22);
    DOMINUS_EXPECT(bundle.animations.HasClip("idle"));
    DOMINUS_EXPECT(bundle.placement.HasPart("torso"));
}

DOMINUS_TEST(HitmAssetImporter_ImportsRealRocketBundleEndToEnd) {
    auto record = *HitmIdentityImporter::Import(RealIdentityDir("rocket")).value;
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->atlas_pixel_width == 1024);
    DOMINUS_EXPECT(result.value->atlas_pixel_height == 326);
}

DOMINUS_TEST(HitmAssetImporter_ImportsRealStaticBundleEndToEnd) {
    auto record = *HitmIdentityImporter::Import(RealIdentityDir("static")).value;
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->atlas_pixel_width == 1024);
    DOMINUS_EXPECT(result.value->atlas_pixel_height == 313);
}

// --- PNG header validation, in isolation ----------------------------------

DOMINUS_TEST(HitmAssetImporter_ReadPngDimensions_RealBrooklynAtlas) {
    auto result = HitmAssetImporter::ReadPngDimensions(SpriteAssetsRoot() / "assets/parts/brooklyn_atlas.png");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->first == 1024);
    DOMINUS_EXPECT(result.value->second == 269);
}

DOMINUS_TEST(HitmAssetImporter_ReadPngDimensions_Break_CorruptSignature_Fails) {
    auto result = HitmAssetImporter::ReadPngDimensions(SpriteAssetsRoot("broken_corrupt_atlas_signature") /
                                                          "assets/parts/brooklyn_atlas.png");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("PNG signature") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_ReadPngDimensions_Break_MissingFile_Fails) {
    auto result = HitmAssetImporter::ReadPngDimensions(SpriteAssetsRoot("broken_missing_atlas_file") /
                                                          "assets/parts/brooklyn_atlas.png");
    DOMINUS_EXPECT(!result.ok);
}

// --- Deliberate-break: every real-world corruption mode must fail loud ---

DOMINUS_TEST(HitmAssetImporter_Break_MissingAtlasFile_Fails) {
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot("broken_missing_atlas_file"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("does not exist") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_CorruptAtlasSignature_Fails) {
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot("broken_corrupt_atlas_signature"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("PNG signature") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_AtlasTooSmallForRealFrameRects_Fails) {
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot("broken_atlas_too_small"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("does not fit within the atlas") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_AnimMissingRequiredIdleClip_StillImportsBundle) {
    // The importer itself does not require 'idle' to exist (that
    // requirement belongs to HitmSpriteDrawData -- see
    // tests/integration/test_hitm_sprite_draw_data.cpp); a real fighter
    // whose anim.json is simply missing one clip still has a real,
    // importable asset bundle.
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot("broken_anim_missing_required_clip"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(!result.value->animations.HasClip("idle"));
}

DOMINUS_TEST(HitmAssetImporter_Break_RigMissingPart_FailsCrossCheck) {
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot("broken_rig_missing_part"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("torso") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_RigPivotMismatch_FailsCrossCheck) {
    auto record = RealBrooklynIdentity();
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot("broken_rig_pivot_mismatch"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("pivot disagrees") != std::string::npos);
}

// --- Deliberate-break: identity.json-level mutations (in-memory, same ----
// --- technique Module 5A's own tests use for a real fighter's real -------
// --- record) ---------------------------------------------------------

DOMINUS_TEST(HitmAssetImporter_Break_SpriteAtlasFieldDisagreesWithPartsJson_Fails) {
    auto record = RealBrooklynIdentity();
    record.identity.AsObject().at("sprite").AsObject().at("atlas") = Value(std::string("wrong_atlas_name"));
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("disagrees") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_SpriteRigFieldMissing_Fails) {
    auto record = RealBrooklynIdentity();
    record.identity.AsObject().at("sprite").AsObject().erase("rig");
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("rig") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_SpriteRigPathPointsAtWrongFighterDirectory_Fails) {
    auto record = RealBrooklynIdentity();
    record.identity.AsObject().at("sprite").AsObject().at("rig") = Value(std::string("data/characters/rocket/parts.json"));
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("brooklyn") != std::string::npos);
}

DOMINUS_TEST(HitmAssetImporter_Break_SpriteBlockEntirelyMissing_Fails) {
    auto record = RealBrooklynIdentity();
    record.identity.AsObject().erase("sprite");
    auto result = HitmAssetImporter::Import(record, SpriteAssetsRoot());
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("sprite") != std::string::npos);
}
