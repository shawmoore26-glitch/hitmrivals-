// tests/graphics/test_raster_device.cpp
// The first real pixel output this engine has ever produced, tested
// for exactly what GRAPHICS/Raster/RasterDevice.h claims -- a real,
// deterministic visualization of a Frame's logical content, not a
// mesh renderer. Every test checks real pixel bytes, never a stubbed
// "success" flag standing in for actual raster output.
#include "GRAPHICS/Raster/RasterDevice.h"
#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "tests/TestFramework.h"

using dominus::animation::Transform2D;
using dominus::graphics::Camera;
using dominus::graphics::DrawCommand;
using dominus::graphics::Frame;
using dominus::graphics::FrameCompiler;
using dominus::graphics::PixelBuffer;
using dominus::graphics::RasterDevice;
using dominus::graphics::RasterOptions;
using dominus::graphics::Scene;
using dominus::graphics::SceneEntity;

namespace {

bool IsBackgroundPixel(const PixelBuffer& buf, int index) {
    std::size_t i = static_cast<std::size_t>(index) * 4;
    return buf.rgba[i] == 0 && buf.rgba[i + 1] == 0 && buf.rgba[i + 2] == 0 && buf.rgba[i + 3] == 255;
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
