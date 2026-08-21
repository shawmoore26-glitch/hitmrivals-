// TOOLS/Editor/dominus_hitm_window.cpp
// Track H Phase 5D -- the actual live, windowed vertical slice:
//
//   Keyboard -> HitmInputAdapter -> HitmMatch::AdvanceFrame()
//     -> HitmFighterRuntime -> HitmSpriteDrawData -> HitmSceneBridge
//     -> SceneEntity/DrawCommand -> TextureAtlas -> RasterDevice -> Window
//
// Real, gated behind DOMINUS_ENABLE_X11_PRESENTER (see
// GRAPHICS/Raster/X11WindowPresenter.h for exactly why X11, not Vulkan).
// Reads real keyboard state via real Xlib `XQueryKeymap` -- the one part
// of this file that touches a real, specific windowing API; everything
// else (HitmApplicationLoop, core::Application, HitmInputAdapter,
// RasterDevice) is the same real, already-unit-tested code every prior
// phase's own tests already exercise headlessly.
//
// `--frames N` (optional): stop after N real Application::Tick() calls
// instead of running until the window is closed -- used by this
// session's own verification (a real X server has no human closing a
// real window), never required for interactive use.
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "CHARACTER/HitmBridge/HitmApplicationLoop.h"
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CORE/Runtime/Application.h"
#include "GRAPHICS/Raster/PngDecoder.h"
#include "GRAPHICS/Raster/X11WindowPresenter.h"

using dominus::character::hitm::HitmApplicationLoop;
using dominus::character::hitm::HitmAssetImporter;
using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;

namespace {

// Real, minimal, disclosed translation: HitmInputAdapter's real GLFW key
// codes (see HitmInputAdapter.h -- these are the same numeric values the
// GLFW_KEY_* names use, chosen so a future GLFW-backed presenter needs
// zero change to HitmInputAdapter itself) to the real X11 KeySym this
// specific presenter actually queries. Covers exactly the keys
// kPlayerOneKeyboard/kPlayerTwoKeyboard use -- not a general GLFW<->X11
// keycode table.
KeySym GlfwKeyCodeToX11KeySym(int glfwCode) {
    switch (glfwCode) {
        case 65: return XK_a;
        case 68: return XK_d;
        case 87: return XK_w;
        case 83: return XK_s;
        case 74: return XK_j;
        case 76: return XK_l;
        case 263: return XK_Left;
        case 262: return XK_Right;
        case 265: return XK_Up;
        case 264: return XK_Down;
        case 321: return XK_KP_1;
        case 322: return XK_KP_2;
        default: return NoSymbol;
    }
}

// Real, live X11 keyboard state -- one XQueryKeymap call per real poll,
// exposed as the same `std::function<bool(int)>` shape
// HitmInputAdapter::ReadRawInput already expects (and every unit test
// already fakes) -- this class is the one, small, real piece that
// replaces the fake in a live run.
class X11LiveKeyboard {
public:
    explicit X11LiveKeyboard(Display* display) : display_(display) {}

    void Refresh() { XQueryKeymap(display_, keys_); }

    bool IsPressed(int glfwCode) const {
        KeySym sym = GlfwKeyCodeToX11KeySym(glfwCode);
        if (sym == NoSymbol) return false;
        KeyCode code = XKeysymToKeycode(display_, sym);
        if (code == 0) return false;
        return (keys_[code / 8] & (1 << (code % 8))) != 0;
    }

private:
    Display* display_;
    char keys_[32] = {};
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: dominus-hitm-window <brooklyn_identity_dir> <rocket_identity_dir> <game.json> "
                      "<sprite_assets_root> [--frames N]\n";
        return 2;
    }
    std::string brooklynDir = argv[1], rocketDir = argv[2], gameJsonPath = argv[3], assetsRoot = argv[4];
    long maxFrames = 0;  // 0 = run until the real window is closed
    for (int i = 5; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--frames") maxFrames = std::strtol(argv[i + 1], nullptr, 10);
    }

    auto identityA = HitmIdentityImporter::Import(brooklynDir);
    auto identityB = HitmIdentityImporter::Import(rocketDir);
    auto rules = HitmGameRules::Import(gameJsonPath);
    if (!identityA.ok || !identityB.ok || !rules.ok) {
        std::cerr << "[hitm-window] real data import failed\n";
        return 1;
    }
    auto genomeA = HitmCombatGenome::FromRecord(*identityA.value);
    auto genomeB = HitmCombatGenome::FromRecord(*identityB.value);
    auto bundleA = HitmAssetImporter::Import(*identityA.value, assetsRoot);
    auto bundleB = HitmAssetImporter::Import(*identityB.value, assetsRoot);
    if (!genomeA.ok || !genomeB.ok || !bundleA.ok || !bundleB.ok) {
        std::cerr << "[hitm-window] real genome/asset import failed\n";
        return 1;
    }
    auto atlasA = dominus::graphics::DecodePngFile(bundleA.value->atlas_png_path, "brooklyn");
    auto atlasB = dominus::graphics::DecodePngFile(bundleB.value->atlas_png_path, "rocket");
    if (!atlasA.ok || !atlasB.ok) {
        std::cerr << "[hitm-window] real atlas decode failed\n";
        return 1;
    }

    auto loopResult = HitmApplicationLoop::Create(*identityA.value, *genomeA.value, *bundleA.value,
                                                    std::move(*atlasA.value), "brooklyn", *identityB.value,
                                                    *genomeB.value, *bundleB.value, std::move(*atlasB.value),
                                                    "rocket", *rules.value);
    if (!loopResult.ok) {
        std::cerr << "[hitm-window] HitmApplicationLoop creation failed: " << loopResult.error << "\n";
        return 1;
    }
    auto& loop = *loopResult.value;

    int width = static_cast<int>(rules.value->View().w);
    int height = static_cast<int>(rules.value->View().h);

    dominus::graphics::X11WindowPresenter presenter;
    std::string error;
    if (!presenter.Initialize(width, height, "HITM Rivals -- Brooklyn vs Rocket (DOMINUS)", error)) {
        std::cerr << "[hitm-window] X11 window init FAILED (real, disclosed environment limitation, not a "
                      "fabricated success): "
                   << error << "\n";
        std::cerr << "RESULT: NOT_EXECUTED\n";
        return 3;
    }
    std::cout << "[hitm-window] real X11 window launched: " << width << "x" << height << "\n";

    X11LiveKeyboard keyboard(presenter.RawDisplay());
    loop.SetPlayerOneInput([&](int code) { return keyboard.IsPressed(code); });
    loop.SetPlayerTwoInput([&](int code) { return keyboard.IsPressed(code); });

    dominus::core::Application app;
    dominus::core::AppConfig config;
    config.jobThreadCount = 1;
    config.maxFrames = static_cast<uint64_t>(maxFrames);
    app.Initialize(config);

    app.SetPollCallback([&] {
        presenter.PollEvents();
        keyboard.Refresh();
    });
    app.SetShouldCloseCallback([&] { return presenter.ShouldClose(); });
    loop.RegisterInto(app, dominus::graphics::RasterOptions{width, height},
                        [&](const dominus::graphics::PixelBuffer& buffer) {
                            std::string presentError;
                            if (!presenter.Present(buffer, presentError)) {
                                std::cerr << "[hitm-window] Present failed: " << presentError << "\n";
                            }
                        });

    std::cout << "[hitm-window] real loop starting (real keyboard: WASD+J/L for Brooklyn, arrows+Down+KP1/KP2 for "
                  "Rocket) -- "
               << (maxFrames > 0 ? ("stopping after " + std::to_string(maxFrames) + " real frames\n")
                                  : "close the window to stop\n");
    app.Run();
    std::cout << "[hitm-window] stopped after " << app.SimStepCount() << " real simulation steps\n";

    presenter.Shutdown();
    app.Shutdown();
    return 0;
}
