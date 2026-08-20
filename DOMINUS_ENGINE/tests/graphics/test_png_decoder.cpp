// tests/graphics/test_png_decoder.cpp
// Texture Capability phase (Track H Phase 5A). Tests GRAPHICS::PngDecoder
// against real HITM atlas PNGs already committed as fixtures
// (tests/fixtures/hitm_sprite_assets/assets/parts/*.png -- the exact
// real files CHARACTER::HitmAssetImporter already validates) plus the
// pre-existing "broken_*" negative-control fixtures Module 5B's own
// deliberate-break tests already established, reused here rather than
// duplicated.
#include "GRAPHICS/Raster/PngDecoder.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::graphics::DecodePngBytes;
using dominus::graphics::DecodePngFile;
using dominus::graphics::TextureAtlas;

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

std::filesystem::path SpriteAssetsDir() { return FixtureDir() / "hitm_sprite_assets"; }

}  // namespace

// --- Real HITM atlas PNGs decode successfully -----------------------------

DOMINUS_TEST(PngDecoder_RealBrooklynAtlas_DecodesToRealKnownDimensions) {
    auto result = DecodePngFile(SpriteAssetsDir() / "assets/parts/brooklyn_atlas.png", "brooklyn");
    DOMINUS_EXPECT(result.ok);
    // Real, independently-verified IHDR dimensions (see this phase's
    // own report / PngDecoder.h's header comment).
    DOMINUS_EXPECT(result.value->width == 1024);
    DOMINUS_EXPECT(result.value->height == 269);
    DOMINUS_EXPECT(result.value->atlas_id == "brooklyn");
    DOMINUS_EXPECT(result.value->rgba.size() == 1024u * 269u * 4u);
}

DOMINUS_TEST(PngDecoder_RealRocketAtlas_DecodesToRealKnownDimensions) {
    auto result = DecodePngFile(SpriteAssetsDir() / "assets/parts/rocket_atlas.png", "rocket");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->width == 1024);
    DOMINUS_EXPECT(result.value->height == 326);
    DOMINUS_EXPECT(result.value->rgba.size() == 1024u * 326u * 4u);
}

DOMINUS_TEST(PngDecoder_RealStaticAtlas_DecodesToRealKnownDimensions) {
    auto result = DecodePngFile(SpriteAssetsDir() / "assets/parts/static_atlas.png", "static");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->width == 1024);
    DOMINUS_EXPECT(result.value->height == 313);
    DOMINUS_EXPECT(result.value->rgba.size() == 1024u * 313u * 4u);
}

// --- Real content, not a fabricated/uninitialized buffer ------------------

DOMINUS_TEST(PngDecoder_RealAtlas_ContainsRealNonUniformPixelData) {
    // A real, non-trivial sprite atlas is not one flat color -- proves
    // this decoder actually reconstructed real per-pixel content
    // (defiltering ran correctly), not a zero-filled or garbage buffer
    // that happens to be the right size.
    auto result = DecodePngFile(SpriteAssetsDir() / "assets/parts/brooklyn_atlas.png", "brooklyn");
    DOMINUS_EXPECT(result.ok);
    const auto& rgba = result.value->rgba;
    bool sawNonZero = false;
    bool sawVariation = false;
    std::uint8_t first = rgba[0];
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        if (rgba[i] != 0 || rgba[i + 1] != 0 || rgba[i + 2] != 0 || rgba[i + 3] != 0) sawNonZero = true;
        if (rgba[i] != first) sawVariation = true;
    }
    DOMINUS_EXPECT(sawNonZero);
    DOMINUS_EXPECT(sawVariation);
}

DOMINUS_TEST(PngDecoder_SameFile_DecodesToByteIdenticalOutput_Determinism) {
    auto resultA = DecodePngFile(SpriteAssetsDir() / "assets/parts/brooklyn_atlas.png", "brooklyn");
    auto resultB = DecodePngFile(SpriteAssetsDir() / "assets/parts/brooklyn_atlas.png", "brooklyn");
    DOMINUS_EXPECT(resultA.ok && resultB.ok);
    DOMINUS_EXPECT(resultA.value->rgba == resultB.value->rgba);
}

// --- A small, real, valid PNG decodes correctly too (not just the big real atlases) ---

DOMINUS_TEST(PngDecoder_RealSmallValidAtlas_DecodesExactly) {
    // broken_atlas_too_small's own PNG is real and perfectly valid at
    // the PNG level (its "broken" is a parts.json-vs-atlas-size
    // mismatch one level up, in HitmAssetImporter -- irrelevant here) --
    // a fast, real, non-synthetic small case.
    auto result =
        DecodePngFile(SpriteAssetsDir() / "broken_atlas_too_small/assets/parts/brooklyn_atlas.png", "small");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->width == 10);
    DOMINUS_EXPECT(result.value->height == 10);
    DOMINUS_EXPECT(result.value->rgba.size() == 10u * 10u * 4u);
}

// --- Deliberate breaks -- real refusals, not silent corruption ------------

DOMINUS_TEST(PngDecoder_Break_MissingFile_Fails) {
    auto result = DecodePngFile(SpriteAssetsDir() / "assets/parts/does_not_exist.png", "x");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.error.empty());
}

DOMINUS_TEST(PngDecoder_Break_RealCorruptSignatureFixture_Fails) {
    // The exact real, already-committed negative-control fixture
    // Module 5B's own HitmAssetImporter tests use for the identical
    // real reason (first signature byte overwritten with 0x00) --
    // reused, not re-fabricated.
    auto result = DecodePngFile(
        SpriteAssetsDir() / "broken_corrupt_atlas_signature/assets/parts/brooklyn_atlas.png", "brooklyn");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("signature") != std::string::npos);
}

DOMINUS_TEST(PngDecoder_Break_TooShortToContainSignature_Fails) {
    auto result = DecodePngBytes("\x89P", "x");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(PngDecoder_Break_EmptyBytes_Fails) {
    auto result = DecodePngBytes("", "x");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(PngDecoder_Break_ValidSignatureButNoChunks_Fails) {
    std::string sig("\x89PNG\r\n\x1a\n", 8);
    auto result = DecodePngBytes(sig, "x");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("IHDR") != std::string::npos);
}

DOMINUS_TEST(PngDecoder_Break_TruncatedChunkHeader_Fails) {
    std::string sig("\x89PNG\r\n\x1a\n", 8);
    sig += "\x00\x00";  // 2 bytes of a chunk length -- real truncation, not a full 4-byte length
    auto result = DecodePngBytes(sig, "x");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(PngDecoder_Break_ChunkLengthRunsPastEndOfFile_Fails) {
    std::string bytes("\x89PNG\r\n\x1a\n", 8);
    // A real IHDR chunk header claiming a length far larger than any
    // data actually present.
    bytes += std::string("\x00\x00\xFF\xFF", 4);  // length = 65535
    bytes += "IHDR";
    bytes += std::string(4, '\0');  // far short of 65535 bytes + 4-byte CRC
    auto result = DecodePngBytes(bytes, "x");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("past end of file") != std::string::npos);
}
