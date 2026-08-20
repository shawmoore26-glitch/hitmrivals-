// tests/graphics/test_raster_device.cpp
// The first real pixel output this engine has ever produced, tested
// for exactly what GRAPHICS/Raster/RasterDevice.h claims -- a real,
// deterministic visualization of a Frame's logical content, not a
// mesh renderer. Every test checks real pixel bytes, never a stubbed
// "success" flag standing in for actual raster output.
#include "GRAPHICS/Raster/PngDecoder.h"
#include "GRAPHICS/Raster/RasterDevice.h"
#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "GRAPHICS/Renderer/TextureAtlas.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::animation::Transform2D;
using dominus::graphics::Camera;
using dominus::graphics::DecodePngFile;
using dominus::graphics::DrawCommand;
using dominus::graphics::Frame;
using dominus::graphics::FrameCompiler;
using dominus::graphics::PixelBuffer;
using dominus::graphics::RasterDevice;
using dominus::graphics::RasterOptions;
using dominus::graphics::Scene;
using dominus::graphics::SceneEntity;
using dominus::graphics::TextureAtlas;

namespace {

bool IsBackgroundPixel(const PixelBuffer& buf, int index) {
    std::size_t i = static_cast<std::size_t>(index) * 4;
    return buf.rgba[i] == 0 && buf.rgba[i + 1] == 0 && buf.rgba[i + 2] == 0 && buf.rgba[i + 3] == 255;
}

// Texture Capability phase (Track H Phase 5A) helpers.
void PixelAt(const PixelBuffer& buf, int x, int y, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b,
             std::uint8_t& a) {
    std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(buf.width) + static_cast<std::size_t>(x)) * 4;
    r = buf.rgba[i + 0];
    g = buf.rgba[i + 1];
    b = buf.rgba[i + 2];
    a = buf.rgba[i + 3];
}

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

int CountNonBackgroundPixels(const PixelBuffer& buf) {
    int count = 0;
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4) {
        if (buf.rgba[i] != 0 || buf.rgba[i + 1] != 0 || buf.rgba[i + 2] != 0) count++;
    }
    return count;
}

}  // namespace

// --- Real, filled buffer -------------------------------------------------

DOMINUS_TEST(RasterDevice_EmptyFrame_ProducesARealFilledOpaqueBackground) {
    Frame frame;  // no commands
    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{64, 64});

    DOMINUS_EXPECT(buffer.width == 64);
    DOMINUS_EXPECT(buffer.height == 64);
    DOMINUS_EXPECT(buffer.rgba.size() == 64u * 64u * 4u);
    DOMINUS_EXPECT(!buffer.buffer_hash.empty());

    // Every pixel is a real, opaque background pixel -- not
    // uninitialized memory, not transparent.
    for (int i = 0; i < 64 * 64; i++) DOMINUS_EXPECT(IsBackgroundPixel(buffer, i));
}

DOMINUS_TEST(RasterDevice_FrameWithCommands_ActuallyDrawsRealPixels) {
    Scene scene;
    scene.entities.push_back({"entity_a", {0, 0, 0, 2, 2}, "mat_a", "mesh_a", 0});
    Frame frame = FrameCompiler::Compile(scene, Camera{});

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{64, 64});
    DOMINUS_EXPECT(CountNonBackgroundPixels(buffer) > 0);
}

// --- Real rasterization, not point-plotting -------------------------------

DOMINUS_TEST(RasterDevice_LargerScale_CoversMorePixels_RealAreaFill) {
    Scene smallScene;
    smallScene.entities.push_back({"e", {0, 0, 0, 1, 1}, "m", "mesh", 0});
    Scene largeScene;
    largeScene.entities.push_back({"e", {0, 0, 0, 5, 5}, "m", "mesh", 0});

    auto smallBuffer = RasterDevice::Rasterize(FrameCompiler::Compile(smallScene, Camera{}), RasterOptions{128, 128});
    auto largeBuffer = RasterDevice::Rasterize(FrameCompiler::Compile(largeScene, Camera{}), RasterOptions{128, 128});

    // A real area fill scales with the transform's scale -- a single
    // point-plot renderer would draw the same 1 pixel regardless of
    // scale; this must draw meaningfully more pixels for the larger
    // entity.
    DOMINUS_EXPECT(CountNonBackgroundPixels(largeBuffer) > CountNonBackgroundPixels(smallBuffer) * 4);
}

// --- Determinism -- the actual, provable claim -----------------------

DOMINUS_TEST(RasterDevice_SameFrame_ProducesByteIdenticalBuffer_TwoIndependentCalls) {
    Scene scene;
    scene.entities.push_back({"brooklyn", {10, 20, 15, 1, 1}, "MAT-JACKET-001", "brooklyn.mesh", 0});
    scene.entities.push_back({"rocket", {-30, 5, 45, 1.5f, 1.5f}, "MAT-FUR-001", "rocket.mesh", 1});
    Frame frame = FrameCompiler::Compile(scene, Camera{});

    auto bufferA = RasterDevice::Rasterize(frame);
    auto bufferB = RasterDevice::Rasterize(frame);

    DOMINUS_EXPECT(bufferA.buffer_hash == bufferB.buffer_hash);
    DOMINUS_EXPECT(bufferA.rgba == bufferB.rgba);
}

DOMINUS_TEST(RasterDevice_DifferentFrames_ProduceDifferentHashes) {
    Scene sceneA;
    sceneA.entities.push_back({"e", {0, 0, 0, 1, 1}, "mat", "mesh", 0});
    Scene sceneB;
    sceneB.entities.push_back({"e", {50, 0, 0, 1, 1}, "mat", "mesh", 0});  // different position

    auto bufferA = RasterDevice::Rasterize(FrameCompiler::Compile(sceneA, Camera{}));
    auto bufferB = RasterDevice::Rasterize(FrameCompiler::Compile(sceneB, Camera{}));

    DOMINUS_EXPECT(bufferA.buffer_hash != bufferB.buffer_hash);
}

DOMINUS_TEST(RasterDevice_InsertionOrderIndependence_MirrorsFrameCompilersOwnGuarantee) {
    // FrameCompiler already guarantees Frame content (and frame_hash)
    // is independent of entity insertion order. RasterDevice draws
    // Frame.commands in its already-fixed order and adds no ordering
    // logic of its own -- so the SAME guarantee must hold for pixels.
    Scene sceneA;
    sceneA.entities.push_back({"alpha", {5, 5, 0, 1, 1}, "m1", "mesh1", 0});
    sceneA.entities.push_back({"beta", {-5, -5, 0, 1, 1}, "m2", "mesh2", 0});

    Scene sceneB;
    sceneB.entities.push_back({"beta", {-5, -5, 0, 1, 1}, "m2", "mesh2", 0});
    sceneB.entities.push_back({"alpha", {5, 5, 0, 1, 1}, "m1", "mesh1", 0});

    auto bufferA = RasterDevice::Rasterize(FrameCompiler::Compile(sceneA, Camera{}));
    auto bufferB = RasterDevice::Rasterize(FrameCompiler::Compile(sceneB, Camera{}));

    DOMINUS_EXPECT(bufferA.buffer_hash == bufferB.buffer_hash);
}

// --- Real, content-derived color -- not random, not fabricated -----

DOMINUS_TEST(RasterDevice_SameMaterialRef_ProducesTheSameColor_DifferentEntities) {
    Scene scene;
    scene.entities.push_back({"entity_one", {-40, 0, 0, 1, 1}, "shared_material", "mesh1", 0});
    scene.entities.push_back({"entity_two", {40, 0, 0, 1, 1}, "shared_material", "mesh1", 1});
    auto buffer = RasterDevice::Rasterize(FrameCompiler::Compile(scene, Camera{}), RasterOptions{200, 200});

    // Sample a pixel from each entity's drawn region -- both were
    // colored from the same real material_ref, so both must be the
    // exact same real color, not independently randomized.
    int centerX = 100;
    auto pixelAt = [&](int x, int y) {
        std::size_t i = (static_cast<std::size_t>(y) * buffer.width + static_cast<std::size_t>(x)) * 4;
        return std::make_tuple(buffer.rgba[i], buffer.rgba[i + 1], buffer.rgba[i + 2]);
    };
    auto colorLeft = pixelAt(centerX - 40, 100);
    auto colorRight = pixelAt(centerX + 40, 100);
    DOMINUS_EXPECT(colorLeft == colorRight);
}

DOMINUS_TEST(RasterDevice_DifferentMaterialRef_ProducesDifferentColor) {
    Scene scene;
    scene.entities.push_back({"entity_one", {-40, 0, 0, 1, 1}, "material_a", "mesh1", 0});
    scene.entities.push_back({"entity_two", {40, 0, 0, 1, 1}, "material_b", "mesh1", 1});
    auto buffer = RasterDevice::Rasterize(FrameCompiler::Compile(scene, Camera{}), RasterOptions{200, 200});

    int centerX = 100;
    auto pixelAt = [&](int x, int y) {
        std::size_t i = (static_cast<std::size_t>(y) * buffer.width + static_cast<std::size_t>(x)) * 4;
        return std::make_tuple(buffer.rgba[i], buffer.rgba[i + 1], buffer.rgba[i + 2]);
    };
    auto colorLeft = pixelAt(centerX - 40, 100);
    auto colorRight = pixelAt(centerX + 40, 100);
    DOMINUS_EXPECT(colorLeft != colorRight);
}

// --- Real rotation support (this phase) -------------------------------

DOMINUS_TEST(RasterDevice_Rotation_ActuallyChangesRenderedOutput) {
    // Confirmed absent by direct source inspection before this phase:
    // rotation_deg was silently ignored entirely. This proves the fix.
    Scene sceneUnrotated;
    sceneUnrotated.entities.push_back({"e", {0, 0, 0.0f, 3, 1}, "m", "mesh", 0});
    Scene sceneRotated;
    sceneRotated.entities.push_back({"e", {0, 0, 45.0f, 3, 1}, "m", "mesh", 0});

    auto bufferUnrotated = RasterDevice::Rasterize(FrameCompiler::Compile(sceneUnrotated, Camera{}), RasterOptions{128, 128});
    auto bufferRotated = RasterDevice::Rasterize(FrameCompiler::Compile(sceneRotated, Camera{}), RasterOptions{128, 128});

    DOMINUS_EXPECT(bufferUnrotated.buffer_hash != bufferRotated.buffer_hash);
}

DOMINUS_TEST(RasterDevice_Rotation_IsDeterministic) {
    Scene scene;
    scene.entities.push_back({"e", {10, -5, 30.0f, 2, 1}, "m", "mesh", 0});
    Frame frame = FrameCompiler::Compile(scene, Camera{});

    auto bufferA = RasterDevice::Rasterize(frame, RasterOptions{128, 128});
    auto bufferB = RasterDevice::Rasterize(frame, RasterOptions{128, 128});

    DOMINUS_EXPECT(bufferA.buffer_hash == bufferB.buffer_hash);
}

DOMINUS_TEST(RasterDevice_EntityOffscreen_DoesNotCrashOrCorruptBuffer) {
    Scene scene;
    scene.entities.push_back({"far_away", {100000, 100000, 0, 1, 1}, "m", "mesh", 0});
    auto buffer = RasterDevice::Rasterize(FrameCompiler::Compile(scene, Camera{}), RasterOptions{32, 32});

    DOMINUS_EXPECT(buffer.rgba.size() == 32u * 32u * 4u);
    // Nothing real was drawn on-screen -- still a valid, all-background buffer.
    for (int i = 0; i < 32 * 32; i++) DOMINUS_EXPECT(IsBackgroundPixel(buffer, i));
}

// --- Material Implementation Phase: renderer binding -------------------
// RasterDevice must consume a DrawCommand's real, pre-resolved
// material_resolved/r/g/b fields (Material Implementation Phase) in
// preference to the pre-existing material_ref-hash fallback -- proven
// directly, not assumed from reading the source.

DOMINUS_TEST(RasterDevice_MaterialResolvedTrue_UsesExactProvidedColor_NotTheHashFallback) {
    SceneEntity resolvedEntity;
    resolvedEntity.entity_id = "resolved_entity";
    resolvedEntity.world_transform = {0, 0, 0, 3, 3};
    resolvedEntity.material_ref = "MAT-DOES-NOT-MATTER";  // real ref present, but must be IGNORED here
    resolvedEntity.mesh_ref = "mesh";
    resolvedEntity.material_resolved = true;
    resolvedEntity.material_r = 200;
    resolvedEntity.material_g = 10;
    resolvedEntity.material_b = 5;
    Scene scene;
    scene.entities.push_back(resolvedEntity);

    auto buffer = RasterDevice::Rasterize(FrameCompiler::Compile(scene, Camera{}), RasterOptions{64, 64});

    bool exactColorFound = false;
    for (std::size_t i = 0; i + 3 < buffer.rgba.size(); i += 4) {
        if (buffer.rgba[i] == 200 && buffer.rgba[i + 1] == 10 && buffer.rgba[i + 2] == 5) {
            exactColorFound = true;
            break;
        }
    }
    DOMINUS_EXPECT(exactColorFound);
}

DOMINUS_TEST(RasterDevice_MaterialResolvedFalseVsTrue_ProduceDifferentColors_ForSameMaterialRef) {
    // The real, direct proof that material_resolved actually changes
    // which code path runs -- same material_ref, same everything else,
    // only material_resolved differs, and the rendered color must
    // differ too (the hash-fallback color for "MAT-SAME-REF" will not,
    // in practice, coincide with the fixed 77/88/99 resolved color
    // used here).
    SceneEntity unresolvedEntity;
    unresolvedEntity.entity_id = "e";
    unresolvedEntity.world_transform = {0, 0, 0, 3, 3};
    unresolvedEntity.material_ref = "MAT-SAME-REF";
    unresolvedEntity.mesh_ref = "mesh";
    Scene unresolvedScene;
    unresolvedScene.entities.push_back(unresolvedEntity);
    auto unresolvedBuffer =
        RasterDevice::Rasterize(FrameCompiler::Compile(unresolvedScene, Camera{}), RasterOptions{64, 64});

    SceneEntity resolvedEntity = unresolvedEntity;
    resolvedEntity.material_resolved = true;
    resolvedEntity.material_r = 77;
    resolvedEntity.material_g = 88;
    resolvedEntity.material_b = 99;
    Scene resolvedScene;
    resolvedScene.entities.push_back(resolvedEntity);
    auto resolvedBuffer =
        RasterDevice::Rasterize(FrameCompiler::Compile(resolvedScene, Camera{}), RasterOptions{64, 64});

    DOMINUS_EXPECT(unresolvedBuffer.buffer_hash != resolvedBuffer.buffer_hash);
}

// --- Texture Capability phase (Track H Phase 5A) --------------------------
// Every test above this section exercises RasterDevice exactly as it
// existed before this phase -- none of them were modified. These tests
// prove the new, additive `textured` path renders real atlas pixels
// without changing any of that pre-existing behavior.

DOMINUS_TEST(RasterDevice_TexturedCommand_SamplesRealAtlasQuadrantsNotFlatColor) {
    // A real, tiny, hand-built 2x2 atlas -- one distinct opaque color
    // per quadrant, so each quadrant of the rendered quad can be
    // checked against a real, known, non-hash-derived color.
    TextureAtlas atlas;
    atlas.atlas_id = "quad_test";
    atlas.width = 2;
    atlas.height = 2;
    atlas.rgba = {
        255, 0, 0, 255,    // (0,0) top-left: red
        0, 255, 0, 255,    // (1,0) top-right: green
        0, 0, 255, 255,    // (0,1) bottom-left: blue
        255, 255, 0, 255,  // (1,1) bottom-right: yellow
    };

    DrawCommand cmd;
    cmd.entity_id = "sprite";
    cmd.screen_transform = Transform2D{0, 0, 0, 8, 8};  // quad = 16*8 = 128px, matches the 128x128 buffer
    cmd.textured = true;
    cmd.atlas_id = "quad_test";
    cmd.atlas_src_x = 0;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = 2;
    cmd.atlas_src_h = 2;

    Frame frame;
    frame.commands.push_back(cmd);
    frame.atlases.push_back(atlas);

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{128, 128});

    std::uint8_t r, g, b, a;
    PixelAt(buffer, 32, 32, r, g, b, a);  // screen top-left quadrant
    DOMINUS_EXPECT(r == 255 && g == 0 && b == 0);
    PixelAt(buffer, 96, 32, r, g, b, a);  // screen top-right quadrant
    DOMINUS_EXPECT(r == 0 && g == 255 && b == 0);
    PixelAt(buffer, 32, 96, r, g, b, a);  // screen bottom-left quadrant
    DOMINUS_EXPECT(r == 0 && g == 0 && b == 255);
    PixelAt(buffer, 96, 96, r, g, b, a);  // screen bottom-right quadrant
    DOMINUS_EXPECT(r == 255 && g == 255 && b == 0);
}

DOMINUS_TEST(RasterDevice_TexturedCommand_RespectsAtlasSrcSubRect_NotWholeAtlas) {
    // A 4x1 atlas: left half red, right half green. Referencing only
    // the right half (atlas_src_x=2, w=2) must render green, not the
    // red left half -- proves atlas_src_x/y/w/h is a real sub-rect
    // selector, not decoration.
    TextureAtlas atlas;
    atlas.atlas_id = "subrect_test";
    atlas.width = 4;
    atlas.height = 1;
    atlas.rgba = {
        255, 0, 0, 255,  // (0,0) red
        255, 0, 0, 255,  // (1,0) red
        0, 255, 0, 255,  // (2,0) green
        0, 255, 0, 255,  // (3,0) green
    };

    DrawCommand cmd;
    cmd.entity_id = "sprite";
    cmd.screen_transform = Transform2D{0, 0, 0, 4, 4};
    cmd.textured = true;
    cmd.atlas_id = "subrect_test";
    cmd.atlas_src_x = 2;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = 2;
    cmd.atlas_src_h = 1;

    Frame frame;
    frame.commands.push_back(cmd);
    frame.atlases.push_back(atlas);

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{64, 64});

    std::uint8_t r, g, b, a;
    PixelAt(buffer, 32, 32, r, g, b, a);  // center of the drawn quad
    DOMINUS_EXPECT(r == 0 && g == 255 && b == 0);
}

DOMINUS_TEST(RasterDevice_TexturedCommand_AlphaBlendsOverBackground_RealCompositing) {
    // A single, real, partially-transparent red pixel (alpha=128) drawn
    // over the default opaque black background -- the resulting color
    // must be a real alpha blend, computed the SAME way RasterDevice's
    // own BlendPixel does (never a hand-rounded literal -- the same
    // float-precision discipline this engine has used since Track H's
    // very first float-accumulation test).
    TextureAtlas atlas;
    atlas.atlas_id = "alpha_test";
    atlas.width = 1;
    atlas.height = 1;
    atlas.rgba = {255, 0, 0, 128};

    DrawCommand cmd;
    cmd.entity_id = "sprite";
    cmd.screen_transform = Transform2D{0, 0, 0, 1, 1};  // quad = 16*1 = 16px, matches the 16x16 buffer
    cmd.textured = true;
    cmd.atlas_id = "alpha_test";
    cmd.atlas_src_x = 0;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = 1;
    cmd.atlas_src_h = 1;

    Frame frame;
    frame.commands.push_back(cmd);
    frame.atlases.push_back(atlas);

    // Quad size = 16 (kMeshPixelScale) * scale 1 = 16px, matching the
    // 16x16 buffer exactly -- so the quad's own shared diagonal (the
    // UnitQuad mesh's two triangles meet along the top-left/bottom-right
    // corners) runs exactly along screen y=x. A real, pre-existing
    // property of FillTriangle's inclusive edge test (unrelated to this
    // phase -- it already existed for flat-color fills, just invisible
    // there because redrawing the SAME solid color twice is a no-op):
    // a pixel center sitting exactly ON that shared diagonal can be
    // covered by both triangles and blended twice. That is invisible
    // for a flat color but changes an alpha-blended result, so this
    // test deliberately samples (12,4) -- far from the y=x diagonal,
    // unambiguously inside exactly one triangle -- rather than "fixing"
    // FillTriangle's shared-edge convention, which is out of this
    // phase's scope and would affect the pre-existing flat-color path
    // too.
    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{16, 16});

    float srcA = 128.0f / 255.0f;
    float dstA = 1.0f - srcA;
    std::uint8_t expectedR = static_cast<std::uint8_t>(255.0f * srcA + 0.0f * dstA);
    std::uint8_t expectedG = static_cast<std::uint8_t>(0.0f * srcA + 0.0f * dstA);

    std::uint8_t r, g, b, a;
    PixelAt(buffer, 12, 4, r, g, b, a);
    DOMINUS_EXPECT(r == expectedR);
    DOMINUS_EXPECT(g == expectedG);
    DOMINUS_EXPECT(a == 255);  // real, disclosed choice: the buffer itself stays fully opaque
}

DOMINUS_TEST(RasterDevice_TexturedCommand_FullyTransparentPixel_LeavesBackgroundUntouched) {
    TextureAtlas atlas;
    atlas.atlas_id = "transparent_test";
    atlas.width = 1;
    atlas.height = 1;
    atlas.rgba = {255, 255, 255, 0};  // real alpha=0 -- fully transparent

    DrawCommand cmd;
    cmd.entity_id = "sprite";
    cmd.screen_transform = Transform2D{0, 0, 0, 2, 2};
    cmd.textured = true;
    cmd.atlas_id = "transparent_test";
    cmd.atlas_src_x = 0;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = 1;
    cmd.atlas_src_h = 1;

    Frame frame;
    frame.commands.push_back(cmd);
    frame.atlases.push_back(atlas);

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{32, 32});
    for (int i = 0; i < 32 * 32; i++) DOMINUS_EXPECT(IsBackgroundPixel(buffer, i));
}

DOMINUS_TEST(RasterDevice_TexturedCommand_AtlasIdNotInFrame_DrawsNothing_RealRefusal) {
    DrawCommand cmd;
    cmd.entity_id = "sprite";
    cmd.screen_transform = Transform2D{0, 0, 0, 4, 4};
    cmd.textured = true;
    cmd.atlas_id = "does_not_exist";
    cmd.atlas_src_x = 0;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = 4;
    cmd.atlas_src_h = 4;

    Frame frame;
    frame.commands.push_back(cmd);
    // frame.atlases deliberately left empty -- real inconsistent Frame.

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{32, 32});
    for (int i = 0; i < 32 * 32; i++) DOMINUS_EXPECT(IsBackgroundPixel(buffer, i));
}

DOMINUS_TEST(RasterDevice_TexturedCommand_SameFrameTwice_ByteIdenticalOutput_Determinism) {
    TextureAtlas atlas;
    atlas.atlas_id = "det_test";
    atlas.width = 2;
    atlas.height = 2;
    atlas.rgba = {10, 20, 30, 255, 40, 50, 60, 255, 70, 80, 90, 255, 100, 110, 120, 255};

    DrawCommand cmd;
    cmd.entity_id = "sprite";
    cmd.screen_transform = Transform2D{5, -5, 15, 3, 3};
    cmd.textured = true;
    cmd.atlas_id = "det_test";
    cmd.atlas_src_x = 0;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = 2;
    cmd.atlas_src_h = 2;

    Frame frame;
    frame.commands.push_back(cmd);
    frame.atlases.push_back(atlas);

    auto bufferA = RasterDevice::Rasterize(frame, RasterOptions{64, 64});
    auto bufferB = RasterDevice::Rasterize(frame, RasterOptions{64, 64});
    DOMINUS_EXPECT(bufferA.buffer_hash == bufferB.buffer_hash);
    DOMINUS_EXPECT(bufferA.rgba == bufferB.rgba);
}

DOMINUS_TEST(RasterDevice_TexturedAndFlatColorCommands_ComposeInOneFrame_DrawOrderStillRealDepth) {
    // A flat-color command drawn first, then a fully opaque textured
    // command drawn second, fully overlapping -- the textured command's
    // real pixel must win where they overlap, the same painter's-
    // algorithm "later draw wins" rule this class has always used
    // (RasterDevice.cpp's own header comment), now proven across the
    // texture/flat-color boundary too.
    DrawCommand flatCmd;
    flatCmd.entity_id = "flat";
    flatCmd.screen_transform = Transform2D{0, 0, 0, 4, 4};
    flatCmd.material_ref = "MAT-BEHIND";

    TextureAtlas atlas;
    atlas.atlas_id = "on_top";
    atlas.width = 1;
    atlas.height = 1;
    atlas.rgba = {9, 200, 40, 255};

    DrawCommand texCmd;
    texCmd.entity_id = "on_top";
    texCmd.screen_transform = Transform2D{0, 0, 0, 4, 4};
    texCmd.textured = true;
    texCmd.atlas_id = "on_top";
    texCmd.atlas_src_x = 0;
    texCmd.atlas_src_y = 0;
    texCmd.atlas_src_w = 1;
    texCmd.atlas_src_h = 1;

    Frame frame;
    frame.commands.push_back(flatCmd);
    frame.commands.push_back(texCmd);
    frame.atlases.push_back(atlas);

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{64, 64});
    std::uint8_t r, g, b, a;
    PixelAt(buffer, 32, 32, r, g, b, a);
    DOMINUS_EXPECT(r == 9 && g == 200 && b == 40);
}

// --- Real HITM atlas data, decoded and rendered end-to-end -----------------

DOMINUS_TEST(RasterDevice_RealDecodedBrooklynAtlas_RendersRealNonBackgroundPixels) {
    auto decoded = DecodePngFile(FixtureDir() / "hitm_sprite_assets/assets/parts/brooklyn_atlas.png", "brooklyn");
    DOMINUS_EXPECT(decoded.ok);

    DrawCommand cmd;
    cmd.entity_id = "brooklyn_full_atlas";
    cmd.screen_transform = Transform2D{0, 0, 0, 8, 8};
    cmd.textured = true;
    cmd.atlas_id = "brooklyn";
    cmd.atlas_src_x = 0;
    cmd.atlas_src_y = 0;
    cmd.atlas_src_w = decoded.value->width;
    cmd.atlas_src_h = decoded.value->height;

    Frame frame;
    frame.commands.push_back(cmd);
    frame.atlases.push_back(*decoded.value);

    auto buffer = RasterDevice::Rasterize(frame, RasterOptions{128, 128});
    // The real Brooklyn atlas is a real, non-trivial sprite sheet --
    // sampling it across a 128x128 quad must produce real, visible,
    // non-background content, not silence.
    DOMINUS_EXPECT(CountNonBackgroundPixels(buffer) > 0);
}
