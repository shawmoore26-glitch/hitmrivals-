// tests/integration/test_hitm_application_loop.cpp
// Track H Phase 5D -- the outer application loop, proven end to end:
//
//   Keyboard -> HitmInputAdapter -> HitmMatch::AdvanceFrame()
//     -> HitmFighterRuntime -> HitmSpriteDrawData -> HitmSceneBridge
//     -> SceneEntity/DrawCommand -> TextureAtlas -> RasterDevice
//
// Every prior real, already-tested Track H module is exercised here
// UNMODIFIED, through the new HitmApplicationLoop/core::Application
// orchestration -- this file proves the WIRING, not the underlying
// combat/sprite/render logic (each already has its own dedicated test
// file). The single most important test in this file
// (RealMatchStateIsIndependentOfHowRealTimeWasChopped) is the HITM-level
// version of the exact determinism guarantee
// tests/core/test_application.cpp already proves generically: the real
// combat simulation ends up bit-identical regardless of real wall-clock
// frame-rate variance.
#include "CHARACTER/HitmBridge/HitmApplicationLoop.h"
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "GRAPHICS/Raster/PngDecoder.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmApplicationLoop;
using dominus::character::hitm::HitmAssetBundle;
using dominus::character::hitm::HitmAssetImporter;
using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmFighterState;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmMatchPhase;
using dominus::character::hitm::kPlayerOneKeyboard;
using dominus::character::hitm::kPlayerTwoKeyboard;
using dominus::core::AppConfig;
using dominus::core::Application;
using dominus::graphics::DecodePngFile;
using dominus::graphics::RasterOptions;
using dominus::graphics::TextureAtlas;

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
HitmAssetBundle RealBundle(const HitmIdentityRecord& identity) {
    auto r = HitmAssetImporter::Import(identity, FindDir("tests/fixtures/hitm_sprite_assets"));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}
TextureAtlas RealAtlas(const HitmAssetBundle& bundle, const std::string& atlasId) {
    auto r = DecodePngFile(bundle.atlas_png_path, atlasId);
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}

HitmApplicationLoop MakeLoop() {
    auto identityA = RealIdentity("brooklyn");
    auto genomeA = RealGenome(identityA);
    auto bundleA = RealBundle(identityA);
    auto atlasA = RealAtlas(bundleA, "brooklyn");

    auto identityB = RealIdentity("rocket");
    auto genomeB = RealGenome(identityB);
    auto bundleB = RealBundle(identityB);
    auto atlasB = RealAtlas(bundleB, "rocket");

    auto rules = RealRules();

    auto result = HitmApplicationLoop::Create(identityA, genomeA, bundleA, std::move(atlasA), "brooklyn", identityB,
                                                 genomeB, bundleB, std::move(atlasB), "rocket", rules);
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}

int CountNonBackgroundOpaquePixels(const dominus::graphics::PixelBuffer& buffer) {
    int count = 0;
    for (std::size_t i = 0; i < buffer.rgba.size(); i += 4) {
        if (buffer.rgba[i] != 0 || buffer.rgba[i + 1] != 0 || buffer.rgba[i + 2] != 0) count++;
    }
    return count;
}

}  // namespace

// --- 1. Real construction, real frame-0 state -----------------------------

DOMINUS_TEST(HitmApplicationLoop_Create_RealBrooklynVsRocket_Succeeds) {
    HitmApplicationLoop loop = MakeLoop();
    auto snap = loop.Snapshot();
    DOMINUS_EXPECT(snap.phase == HitmMatchPhase::kRoundIntro);
    DOMINUS_EXPECT(snap.fighter_a.x == 300.0f);  // real starting position, Track H Phase 3
    DOMINUS_EXPECT(snap.fighter_b.x == 760.0f);
    DOMINUS_EXPECT(loop.LastError().empty());
}

DOMINUS_TEST(HitmApplicationLoop_Create_CachesARealFrameBeforeAnySimulationStepIsCalledExternally) {
    HitmApplicationLoop loop = MakeLoop();
    // Create() itself runs one real SimulationStep() so CachedFrame()/
    // Present() are meaningful immediately -- both fighters' real parts
    // should already be present.
    DOMINUS_EXPECT(loop.CachedFrame().commands.size() > 0);
    DOMINUS_EXPECT(loop.CachedFrame().atlases.size() == 2);
}

// --- 2. SimulationStep really advances the real HitmMatch -----------------

DOMINUS_TEST(HitmApplicationLoop_SimulationStep_AdvancesRealRoundIntroCountdown) {
    HitmApplicationLoop loop = MakeLoop();
    int before = loop.Snapshot().round_intro_frames_remaining;
    loop.SimulationStep();
    int after = loop.Snapshot().round_intro_frames_remaining;
    DOMINUS_EXPECT(after == before - 1);
}

DOMINUS_TEST(HitmApplicationLoop_SimulationStep_PlayerOneRightHeld_RealFighterActuallyWalks) {
    HitmApplicationLoop loop = MakeLoop();
    for (int i = 0; i < 121; ++i) loop.SimulationStep();  // real 120-frame round intro
    float xBefore = loop.Snapshot().fighter_a.x;

    loop.SetPlayerOneInput([](int code) { return code == kPlayerOneKeyboard.right; });
    for (int i = 0; i < 10; ++i) loop.SimulationStep();

    DOMINUS_EXPECT(loop.Snapshot().fighter_a.x > xBefore);
    DOMINUS_EXPECT(loop.Snapshot().fighter_a.state == HitmFighterState::kWalking);
}

DOMINUS_TEST(HitmApplicationLoop_SimulationStep_PlayerTwoBlockHeld_RealFighterEntersRealBlockingStance) {
    HitmApplicationLoop loop = MakeLoop();
    for (int i = 0; i < 121; ++i) loop.SimulationStep();

    loop.SetPlayerTwoInput([](int code) { return code == kPlayerTwoKeyboard.block; });
    loop.SimulationStep();

    DOMINUS_EXPECT(loop.Snapshot().fighter_b.state == HitmFighterState::kBlockingStance);
}

// --- 3. Real presentation: both fighters, from their own real atlases ----

DOMINUS_TEST(HitmApplicationLoop_CachedFrame_CarriesBothFightersFromTheirOwnRealAtlases) {
    HitmApplicationLoop loop = MakeLoop();
    const auto& frame = loop.CachedFrame();

    bool foundBrooklyn = false, foundRocket = false;
    for (const auto& cmd : frame.commands) {
        DOMINUS_EXPECT(cmd.textured);
        if (cmd.atlas_id == "brooklyn") foundBrooklyn = true;
        if (cmd.atlas_id == "rocket") foundRocket = true;
    }
    DOMINUS_EXPECT(foundBrooklyn);
    DOMINUS_EXPECT(foundRocket);

    bool haveBrooklynAtlas = false, haveRocketAtlas = false;
    for (const auto& atlas : frame.atlases) {
        if (atlas.atlas_id == "brooklyn") haveBrooklynAtlas = true;
        if (atlas.atlas_id == "rocket") haveRocketAtlas = true;
    }
    DOMINUS_EXPECT(haveBrooklynAtlas);
    DOMINUS_EXPECT(haveRocketAtlas);
}

DOMINUS_TEST(HitmApplicationLoop_Present_ProducesRealNonEmptyPixelContent) {
    HitmApplicationLoop loop = MakeLoop();
    RasterOptions options{1060, 600};  // real game.json view.w/h
    auto buffer = loop.Present(options);
    DOMINUS_EXPECT(buffer.width == 1060);
    DOMINUS_EXPECT(buffer.height == 600);
    DOMINUS_EXPECT(CountNonBackgroundOpaquePixels(buffer) > 0);
}

DOMINUS_TEST(HitmApplicationLoop_Present_IsPureAndSafeToCallRepeatedlyWithoutChangingState) {
    HitmApplicationLoop loop = MakeLoop();
    RasterOptions options{256, 256};
    auto first = loop.Present(options);
    auto second = loop.Present(options);
    DOMINUS_EXPECT(first.buffer_hash == second.buffer_hash);  // no hidden mutation between calls
}

// --- 4. Registered into a real core::Application --------------------------

DOMINUS_TEST(HitmApplicationLoop_RegisteredIntoApplication_TickDrivesTheRealMatch) {
    HitmApplicationLoop loop = MakeLoop();
    loop.SetPlayerOneInput([](int code) { return code == kPlayerOneKeyboard.right; });

    Application app;
    AppConfig config;
    config.jobThreadCount = 1;
    app.Initialize(config);

    int presentCount = 0;
    loop.RegisterInto(app, RasterOptions{64, 64}, [&](const dominus::graphics::PixelBuffer&) { presentCount++; });

    for (int i = 0; i < 121; ++i) app.Tick(Application::kFixedSimDt);  // clear round intro
    float xBefore = loop.Snapshot().fighter_a.x;
    for (int i = 0; i < 10; ++i) app.Tick(Application::kFixedSimDt);

    DOMINUS_EXPECT(loop.Snapshot().fighter_a.x > xBefore);  // real simulation actually advanced through Application
    DOMINUS_EXPECT(presentCount == 131);  // present runs once per real Tick(), regardless of sim step count
    app.Shutdown();
}

// --- 5. THE determinism proof: real match state survives real jitter -----

DOMINUS_TEST(HitmApplicationLoop_RealMatchStateIsIndependentOfHowRealTimeWasChopped) {
    HitmApplicationLoop loopA = MakeLoop();
    loopA.SetPlayerOneInput([](int code) { return code == kPlayerOneKeyboard.right; });
    Application appA;
    { AppConfig c; c.jobThreadCount = 1; appA.Initialize(c); }
    loopA.RegisterInto(appA, RasterOptions{16, 16});

    HitmApplicationLoop loopB = MakeLoop();
    loopB.SetPlayerOneInput([](int code) { return code == kPlayerOneKeyboard.right; });
    Application appB;
    { AppConfig c; c.jobThreadCount = 1; appB.Initialize(c); }
    loopB.RegisterInto(appB, RasterOptions{16, 16});

    // 3 real seconds of real time -- enough to clear the real 120-frame
    // round intro and walk for a while -- fed to the two Applications in
    // completely different real dt patterns.
    const double totalRealSeconds = 3.0;

    for (int i = 0; i < 60; ++i) appA.Tick(totalRealSeconds / 60.0);  // uniform 60 calls

    double remaining = totalRealSeconds;
    bool fast = true;
    while (remaining > 1e-9) {
        double dt = fast ? 0.006 : 0.019;  // irregular, jitter-like real frame timing
        if (dt > remaining) dt = remaining;
        appB.Tick(dt);
        remaining -= dt;
        fast = !fast;
    }

    DOMINUS_EXPECT(appA.SimStepCount() == appB.SimStepCount());
    // The real proof: not just the step count, but the ACTUAL real HITM
    // match state (position, state, hp, phase, everything
    // HitmMatchSnapshot's own real operator== compares) is bit-identical.
    DOMINUS_EXPECT(loopA.Snapshot() == loopB.Snapshot());
    // And it is a real, non-trivial state -- the fighter actually moved,
    // this isn't two untouched frame-0 snapshots trivially matching.
    DOMINUS_EXPECT(loopA.Snapshot().fighter_a.x > 300.0f);

    appA.Shutdown();
    appB.Shutdown();
}
