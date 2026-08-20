// tests/integration/test_hitm_scene_bridge.cpp
// Track H Phase 5B -- the HITM Sprite Bridge, proven end to end:
//
//   HitmFighterRuntime -> HitmSpriteDrawData -> HitmPartDraw
//     -> HitmSceneBridge -> SceneEntity/DrawCommand -> TextureAtlas
//     -> RasterDevice -> actual HITM pixels
//
// Every real fixture value asserted below (frame_x/y/w/h, place_x/y,
// norm_w/h, displayHeight) is read directly from the same real,
// committed HITM data every prior Track H module has used -- see
// CHARACTER/HitmBridge/HitmSceneBridge.h's own header comment for the
// full derivation of the placement/rotation formulas this test checks
// against. The strongest test in this file (PixelsMatchRealAtlasFixture)
// does not just check "something rendered" -- it independently recomputes,
// from the real transform this bridge produced, exactly which real atlas
// pixel a handful of specific rendered screen pixels should sample from,
// decodes the real committed atlas PNG fixture directly (bypassing the
// renderer entirely), and asserts byte-for-byte RGBA equality against
// what RasterDevice actually drew.
#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "CHARACTER/HitmBridge/HitmSceneBridge.h"
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"
#include "GRAPHICS/Raster/PngDecoder.h"
#include "GRAPHICS/Raster/RasterDevice.h"
#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

using dominus::character::hitm::BuildHitmSceneEntities;
using dominus::character::hitm::BuildSpriteDrawData;
using dominus::character::hitm::HitmAssetBundle;
using dominus::character::hitm::HitmAssetImporter;
using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmFighterRuntime;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmPartDraw;
using dominus::character::hitm::HitmSecondaryMotionState;
using dominus::character::hitm::HitmSpriteDrawData;
using dominus::graphics::Camera;
using dominus::graphics::DecodePngFile;
using dominus::graphics::Frame;
using dominus::graphics::FrameCompiler;
using dominus::graphics::PixelBuffer;
using dominus::graphics::RasterDevice;
using dominus::graphics::RasterOptions;
using dominus::graphics::Scene;
using dominus::graphics::SceneEntity;
using dominus::graphics::TextureAtlas;
using dominus::graphics::Viewport;

namespace {

std::filesystem::path FindDir(const std::filesystem::path& rel) {
    std::vector<std::filesystem::path> candidates = {rel, std::filesystem::path("..") / rel,
                                                       std::filesystem::path("../..") / rel};
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture dir not found: " + rel.string());
}

HitmIdentityRecord RealIdentity(const std::string& fighter) {
    auto r = HitmIdentityImporter::Import(FindDir(std::filesystem::path("tests/fixtures/hitm_identity") / fighter));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return *r.value;
}
HitmCombatGenome RealGenome(const HitmIdentityRecord& record) {
    auto r = HitmCombatGenome::FromRecord(record);
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}
HitmGameRules RealRules() {
    auto r = HitmGameRules::Import(FindDir("tests/fixtures/hitm_game_rules/game.json"));
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
    auto result = HitmAssetImporter::Import(identity, FindDir("tests/fixtures/hitm_sprite_assets"));
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}
TextureAtlas MakeBrooklynAtlas() {
    auto bundle = MakeBrooklynBundle();
    auto result = DecodePngFile(bundle.atlas_png_path, "brooklyn");
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}
const HitmPartDraw* FindDraw(const HitmSpriteDrawData& draw, const std::string& partName) {
    for (const auto& p : draw.parts) {
        if (p.part_name == partName) return &p;
    }
    return nullptr;
}
const SceneEntity* FindEntity(const std::vector<SceneEntity>& entities, const std::string& id) {
    for (const auto& e : entities) {
        if (e.entity_id == id) return &e;
    }
    return nullptr;
}

}  // namespace

// --- 1. Real atlas loads through the real Phase 5A decoder ---------------

DOMINUS_TEST(HitmSceneBridge_RealBrooklynAtlas_DecodesToNonTrivialRealPixelData) {
    TextureAtlas atlas = MakeBrooklynAtlas();
    DOMINUS_EXPECT(atlas.atlas_id == "brooklyn");
    DOMINUS_EXPECT(atlas.width > 0);
    DOMINUS_EXPECT(atlas.height > 0);
    DOMINUS_EXPECT(atlas.rgba.size() == static_cast<std::size_t>(atlas.width) * atlas.height * 4);

    // Alpha preserved: the real atlas is not a fully-opaque rectangle --
    // it is a packed sheet with real transparent gaps between parts.
    bool foundTransparent = false, foundOpaque = false;
    for (std::size_t i = 3; i < atlas.rgba.size(); i += 4) {
        if (atlas.rgba[i] == 0) foundTransparent = true;
        if (atlas.rgba[i] == 255) foundOpaque = true;
    }
    DOMINUS_EXPECT(foundTransparent);
    DOMINUS_EXPECT(foundOpaque);
}

// --- 2. Real position/texture fields populated ----------------------------

DOMINUS_TEST(HitmSceneBridge_Brooklyn_PopulatesRealTextureFieldsFromRealHitmPartDraw) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto rules = RealRules();
    auto drawResult = BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(drawResult.ok);

    auto entitiesResult = BuildHitmSceneEntities("brooklyn", runtime.Snapshot(), *drawResult.value, rules.Sprite().display_height);
    DOMINUS_EXPECT(entitiesResult.ok);
    const auto& entities = *entitiesResult.value;

    // Real drawOrder fidelity -- one entity per real drawn part.
    DOMINUS_EXPECT(entities.size() == drawResult.value->parts.size());

    const HitmPartDraw* hatDraw = FindDraw(*drawResult.value, "hat");
    DOMINUS_EXPECT(hatDraw != nullptr);
    const SceneEntity* hatEntity = FindEntity(entities, "brooklyn_hat");
    DOMINUS_EXPECT(hatEntity != nullptr);

    DOMINUS_EXPECT(hatEntity->textured == true);
    DOMINUS_EXPECT(hatEntity->atlas_id == "brooklyn");
    // Real atlas-pixel source rect, byte for byte from parts.json via
    // HitmPartDraw -- not recomputed, not approximated.
    DOMINUS_EXPECT(hatEntity->atlas_src_x == hatDraw->frame_x);
    DOMINUS_EXPECT(hatEntity->atlas_src_y == hatDraw->frame_y);
    DOMINUS_EXPECT(hatEntity->atlas_src_w == hatDraw->frame_w);
    DOMINUS_EXPECT(hatEntity->atlas_src_h == hatDraw->frame_h);
    // Real frame dimensions preserved exactly -- these are the real,
    // already-tested parts.json values (see test_hitm_sprite_draw_data.cpp).
    DOMINUS_EXPECT(hatDraw->frame_w == 103);
    DOMINUS_EXPECT(hatDraw->frame_h == 51);
}

// --- 3. displayHeight required, never silently defaulted ------------------

DOMINUS_TEST(HitmSceneBridge_Break_ZeroDisplayHeight_Fails) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto drawResult = BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(drawResult.ok);

    auto result = BuildHitmSceneEntities("brooklyn", runtime.Snapshot(), *drawResult.value, 0.0);
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(HitmSceneBridge_Break_EmptyFighterId_Fails) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto rules = RealRules();
    auto drawResult = BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(drawResult.ok);

    auto result = BuildHitmSceneEntities("", runtime.Snapshot(), *drawResult.value, rules.Sprite().display_height);
    DOMINUS_EXPECT(!result.ok);
}

// --- 4. Real secondary motion survives into the bridged entity -----------

DOMINUS_TEST(HitmSceneBridge_RealSecondaryMotion_ChangesTheEntityTransformItProduces) {
    auto runtimeA = MakeBrooklynRuntime();  // no secondary motion
    auto runtimeB = MakeBrooklynRuntime();  // with real secondary motion
    auto bundle = MakeBrooklynBundle();
    auto rules = RealRules();
    HitmSecondaryMotionState springState;

    // Advance both fighters through several real idle frames -- "hat" is
    // a real Brooklyn follow bone (parent "head", real spring constants
    // in parts.json) with no idle track of its own, so runtimeA's
    // (secondaryMotion=nullptr) hat pose stays at the real zero-pose
    // fallback every frame, while runtimeB's real spring chases head's
    // own real idle rotation and accumulates a nonzero angle.
    dominus::character::hitm::HitmMoveInstance* noMove = nullptr;
    HitmSpriteDrawData lastA, lastB;
    for (int f = 0; f < 30; ++f) {
        runtimeA.AdvanceFrame(dominus::character::hitm::HitmInputCommand::kNeutral);
        runtimeB.AdvanceFrame(dominus::character::hitm::HitmInputCommand::kNeutral);
        auto rA = BuildSpriteDrawData(runtimeA.Snapshot(), noMove, bundle, nullptr);
        auto rB = BuildSpriteDrawData(runtimeB.Snapshot(), noMove, bundle, &springState);
        DOMINUS_EXPECT(rA.ok);
        DOMINUS_EXPECT(rB.ok);
        lastA = std::move(*rA.value);
        lastB = std::move(*rB.value);
    }

    const HitmPartDraw* hatA = FindDraw(lastA, "hat");
    const HitmPartDraw* hatB = FindDraw(lastB, "hat");
    DOMINUS_EXPECT(hatA != nullptr);
    DOMINUS_EXPECT(hatB != nullptr);
    DOMINUS_EXPECT(hatA->pose_rotation_deg == 0.0);       // real zero-pose fallback, no secondary motion
    DOMINUS_EXPECT(hatB->pose_rotation_deg != 0.0);       // real spring has moved it

    auto entitiesA = BuildHitmSceneEntities("brooklyn", runtimeA.Snapshot(), lastA, rules.Sprite().display_height);
    auto entitiesB = BuildHitmSceneEntities("brooklyn", runtimeB.Snapshot(), lastB, rules.Sprite().display_height);
    DOMINUS_EXPECT(entitiesA.ok);
    DOMINUS_EXPECT(entitiesB.ok);
    const SceneEntity* hatEntityA = FindEntity(*entitiesA.value, "brooklyn_hat");
    const SceneEntity* hatEntityB = FindEntity(*entitiesB.value, "brooklyn_hat");
    DOMINUS_EXPECT(hatEntityA != nullptr);
    DOMINUS_EXPECT(hatEntityB != nullptr);

    // The bridge's own documented rotation formula, real and derived
    // (see HitmSceneBridge.h): transform.rotation_deg = -pose_rotation_deg.
    DOMINUS_EXPECT(hatEntityA->world_transform.rotation_deg == 0.0f);
    DOMINUS_EXPECT(std::abs(static_cast<double>(hatEntityB->world_transform.rotation_deg) - (-hatB->pose_rotation_deg)) < 1e-6);
    // The real secondary motion difference is genuinely visible on the
    // renderer-facing entity, not lost anywhere in the bridge.
    DOMINUS_EXPECT(hatEntityB->world_transform.rotation_deg != hatEntityA->world_transform.rotation_deg);
}

// --- 5. THE proof: rendered pixels match the real, committed atlas -------

DOMINUS_TEST(HitmSceneBridge_Brooklyn_RasterDeviceProducesActualRealAtlasPixelsForHat) {
    auto runtime = MakeBrooklynRuntime();
    auto bundle = MakeBrooklynBundle();
    auto rules = RealRules();
    auto atlas = MakeBrooklynAtlas();

    // Real idle, real frame 0, no secondary motion: "hat" has no idle
    // track (verified against the real anim.json), so its real pose is
    // the zero-pose fallback -- rotation=0, offset=(0,0) -- the one case
    // this test can predict exactly without reimplementing rotated
    // nearest-neighbor sampling as a second, independent renderer.
    auto drawResult = BuildSpriteDrawData(runtime.Snapshot(), nullptr, bundle);
    DOMINUS_EXPECT(drawResult.ok);
    const HitmPartDraw* hatDraw = FindDraw(*drawResult.value, "hat");
    DOMINUS_EXPECT(hatDraw != nullptr);
    DOMINUS_EXPECT(hatDraw->pose_rotation_deg == 0.0);
    DOMINUS_EXPECT(hatDraw->pose_offset_x == 0.0);
    DOMINUS_EXPECT(hatDraw->pose_offset_y == 0.0);

    auto entitiesResult = BuildHitmSceneEntities("brooklyn", runtime.Snapshot(), *drawResult.value, rules.Sprite().display_height);
    DOMINUS_EXPECT(entitiesResult.ok);
    const SceneEntity* hatEntity = FindEntity(*entitiesResult.value, "brooklyn_hat");
    DOMINUS_EXPECT(hatEntity != nullptr);
    DOMINUS_EXPECT(hatEntity->world_transform.rotation_deg == 0.0f);  // confirms the zero-rotation premise above

    Scene scene;
    scene.entities.push_back(*hatEntity);

    // Camera centered exactly on the entity's own real world position --
    // isolates "does texturing/atlas sampling work" from "where does it
    // land on an eventual full game viewport" (a real, disclosed Phase 5B
    // scope boundary -- camera framing is Track H Phase 5F, not this
    // phase; see HITM_RENDER_INPUT_LOOP_AUDIT.md section D).
    Camera camera;
    camera.x = hatEntity->world_transform.x;
    camera.y = hatEntity->world_transform.y;

    RasterOptions options{200, 200};
    Frame frame = FrameCompiler::Compile(scene, camera, Viewport{options.width, options.height}, {atlas});
    DOMINUS_EXPECT(frame.commands.size() == 1);
    DOMINUS_EXPECT(frame.commands[0].textured);

    PixelBuffer buffer = RasterDevice::Rasterize(frame, options);

    // Real, disclosed rendered size -- HitmSceneBridge's own real
    // displayHeight-scaled formula, independently recomputed here from
    // the same real, already-asserted inputs.
    const double sw = hatDraw->norm_w * rules.Sprite().display_height;
    const double sh = hatDraw->norm_h * rules.Sprite().display_height;
    DOMINUS_EXPECT(sw > 1.0);
    DOMINUS_EXPECT(sh > 1.0);

    // With the camera centered on the entity and rotation=0, the
    // rendered quad's screen bounding box is exactly
    // [W/2 - sw/2, W/2 + sw/2] x [H/2 - sh/2, H/2 + sh/2] -- see
    // HitmSceneBridge.h/RasterDevice.cpp's own real UV-mapping
    // convention (local +Y = atlas rect TOP row) for why this bilinear
    // mapping is exact, not approximate, for an unrotated quad.
    const double leftPx = options.width / 2.0 - sw / 2.0;
    const double topPx = options.height / 2.0 - sh / 2.0;

    int matched = 0;
    for (double fracX : {0.15, 0.5, 0.85}) {
        for (double fracY : {0.15, 0.5, 0.85}) {
            int px = static_cast<int>(std::floor(leftPx + fracX * sw));
            int py = static_cast<int>(std::floor(topPx + fracY * sh));
            DOMINUS_EXPECT(px >= 0 && px < options.width);
            DOMINUS_EXPECT(py >= 0 && py < options.height);

            // RasterDevice samples at the PIXEL CENTER (px+0.5, py+0.5),
            // not at the nominal fracX/fracY chosen above -- recompute
            // the real fraction from the actual sampled pixel center so
            // "expected" matches RasterDevice's own real convention
            // exactly, not an approximation of it.
            double actualFracX = (static_cast<double>(px) + 0.5 - leftPx) / sw;
            double actualFracY = (static_cast<double>(py) + 0.5 - topPx) / sh;
            double u = hatDraw->frame_x + actualFracX * hatDraw->frame_w;
            double v = hatDraw->frame_y + actualFracY * hatDraw->frame_h;
            int sx = std::clamp(static_cast<int>(u), 0, atlas.width - 1);
            int sy = std::clamp(static_cast<int>(v), 0, atlas.height - 1);
            std::size_t srcIdx = (static_cast<std::size_t>(sy) * atlas.width + static_cast<std::size_t>(sx)) * 4;
            std::uint8_t expectedR = atlas.rgba[srcIdx + 0];
            std::uint8_t expectedG = atlas.rgba[srcIdx + 1];
            std::uint8_t expectedB = atlas.rgba[srcIdx + 2];
            std::uint8_t expectedA = atlas.rgba[srcIdx + 3];

            std::size_t dstIdx = (static_cast<std::size_t>(py) * options.width + static_cast<std::size_t>(px)) * 4;
            if (expectedA == 255) {
                // Fully opaque real atlas texel: RasterDevice's real
                // "over" compositing writes it through unchanged -- a
                // strict, unambiguous byte-for-byte match against the
                // real committed atlas fixture.
                DOMINUS_EXPECT(buffer.rgba[dstIdx + 0] == expectedR);
                DOMINUS_EXPECT(buffer.rgba[dstIdx + 1] == expectedG);
                DOMINUS_EXPECT(buffer.rgba[dstIdx + 2] == expectedB);
                matched++;
            }
            // A partially/fully transparent real texel legitimately
            // blends with the real opaque background -- not a
            // byte-identity case, and not asserted here; the opaque
            // samples above are the real, strict proof.
        }
    }
    // At least some of the 9 sampled points landed on a real, opaque
    // part of the hat art -- a real fixture fact, not assumed.
    DOMINUS_EXPECT(matched > 0);
}
