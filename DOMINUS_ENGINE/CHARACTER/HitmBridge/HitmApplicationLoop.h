// CHARACTER/HitmBridge/HitmApplicationLoop.h
// ROADMAP.md Track H Phase 5D -- the HITM-specific half of the outer
// application loop:
//
//   Keyboard -> HitmInputAdapter -> HitmMatch::AdvanceFrame()
//     -> HitmFighterRuntime -> HitmSpriteDrawData -> HitmSceneBridge
//     -> SceneEntity/DrawCommand -> TextureAtlas -> RasterDevice -> Window
//
// WHY THIS CLASS EXISTS, RATHER THAN PUTTING THIS LOGIC INTO
// `CORE::Application` DIRECTLY: `Application::Tick()` (Phase 5D's other
// half, `CORE/Runtime/Application.h`) is deliberately, permanently
// game-agnostic -- it has never included CHARACTER, GRAPHICS, or COMBAT,
// and this phase does not change that. This is the exact same
// architectural law `WORLD/Core/WorldTick.h` already enforces one layer
// down ("a renderer is a system that would READ the world state
// afterward, not something WorldTick calls"; proven directly by
// tests/world/test_hitm_rivals_as_world_entity.cpp, which wires
// COMBAT/CHARACTER into `WorldTick` from OUTSIDE `WORLD/Core` entirely).
// `HitmApplicationLoop` is that same pattern one layer up: it lives in
// `CHARACTER::hitm` (already, correctly, above CORE/WORLD/COMBAT/GRAPHICS
// in this engine's real dependency stack -- `dominus_hitm_runtime`
// already links `dominus_graphics`, see Track H Phase 5B), owns the real
// game state (`HitmMatch`, unmodified), and exposes exactly the two
// callbacks `Application`'s generic hooks expect
// (`SetSimulationStepCallback`/`SetPresentCallback`) via `RegisterInto`.
// `Application` never learns a `HitmMatch` exists.
//
// THE CRITICAL SEPARATION THIS CLASS PRESERVES: `HitmSpriteDrawData`'s
// own real secondary-motion contract (see its own header comment) is
// "call exactly once per real SIMULATION frame" -- not once per real
// PRESENTED frame, which can be zero, one, or several times per
// simulation frame depending on wall-clock pacing (Application.h's own
// fixed-timestep guarantee). So `SimulationStep()` (registered as
// Application's onSimStep, called once per fixed kFixedSimDt) is the
// ONLY place `BuildSpriteDrawData`/`BuildHitmSceneEntities`/
// `FrameCompiler::Compile` ever run -- their real output is cached; the
// onPresent callback (`Present()`) only ever re-rasterizes that cache,
// as many or as few times as real presentation frequency calls for,
// with zero risk of double-integrating a real spring or otherwise
// corrupting simulation-frequency state by calling it at the wrong
// cadence.
//
// CAMERA: `UpdatePresentationCamera()` below is a deliberately minimal,
// clearly-labeled placeholder (fighter-midpoint X, fixed Y/zoom) -- NOT
// the real, evidenced `cameras.json`-based algorithm
// `HITM_RENDER_INPUT_LOOP_AUDIT.md` section D.3 already named and Track
// H Phase 5E owns. Just enough to keep both fighters within the same
// general viewport region as they move, so "the scene updates
// continuously" is actually visible, not a claim about final camera
// behavior.
#pragma once

#include <functional>
#include <string>

#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmInputAdapter.h"
#include "CHARACTER/HitmBridge/HitmMatch.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"
#include "CORE/Runtime/Application.h"
#include "GRAPHICS/Raster/RasterDevice.h"
#include "GRAPHICS/Renderer/Frame.h"
#include "GRAPHICS/Renderer/TextureAtlas.h"

namespace dominus::character::hitm {

class HitmApplicationLoop {
public:
    // Real, already-imported/decoded inputs only -- this constructor
    // does no file I/O of its own (use HitmIdentityImporter/
    // HitmCombatGenome/HitmGameRules/HitmAssetImporter/
    // GRAPHICS::DecodePngFile/HitmMoveInstance::Extract to produce
    // them, exactly as every prior Track H CLI demo already does; see
    // this file's own .cpp for a real, tested example call sequence in
    // its own test fixture helpers). Fails if either fighter's real
    // `HitmMatch::Create` fails, or either fighter's real "special"
    // move fails to extract (Track H Phase 4 already established both
    // Brooklyn's and Rocket's real specials extract successfully; a
    // failure here means a real, different data problem, not
    // something this class routes around).
    static core::Result<HitmApplicationLoop> Create(const HitmIdentityRecord& identityA, const HitmCombatGenome& genomeA,
                                                       const HitmAssetBundle& bundleA,
                                                       graphics::TextureAtlas atlasA, const std::string& fighterIdA,
                                                       const HitmIdentityRecord& identityB, const HitmCombatGenome& genomeB,
                                                       const HitmAssetBundle& bundleB,
                                                       graphics::TextureAtlas atlasB, const std::string& fighterIdB,
                                                       const HitmGameRules& rules);

    HitmApplicationLoop(HitmApplicationLoop&&) noexcept = default;
    HitmApplicationLoop& operator=(HitmApplicationLoop&&) noexcept = default;
    HitmApplicationLoop(const HitmApplicationLoop&) = delete;
    HitmApplicationLoop& operator=(const HitmApplicationLoop&) = delete;
    ~HitmApplicationLoop() = default;

    // Real, injectable per-player physical input state (see
    // HitmInputAdapter.h's own `ReadRawInput`/`HitmButtonBinding`) --
    // defaults to "nothing held" for either player until set. The real
    // `glfwGetKey`-backed callable is the caller's job (a terminal
    // driver, e.g. TOOLS/Editor/dominus_hitm_window.cpp); a test
    // supplies a synthetic one, exactly like HitmInputAdapter's own
    // tests already do.
    void SetPlayerOneInput(std::function<bool(int)> isPressed) { isPressedA_ = std::move(isPressed); }
    void SetPlayerTwoInput(std::function<bool(int)> isPressed) { isPressedB_ = std::move(isPressed); }

    // Steps 2-8: sample input, translate, HitmMatch::AdvanceFrame,
    // build sprite data + scene entities for BOTH fighters, update the
    // presentation camera, compile the real Frame -- and cache it. Must
    // be called at the real, fixed simulation cadence (once per
    // Application::kFixedSimDt) -- see this file's own top comment for
    // why. Real per-fighter failures (a real HitmSpriteDrawData/
    // HitmSceneBridge Result::Fail) leave the previous cached frame
    // untouched rather than corrupting presentation; check LastError()
    // for diagnostics. Brooklyn/Rocket's real fixture data has never
    // actually hit this path in this track's own testing.
    void SimulationStep();

    // Step 9: re-rasterizes the cached Frame (from the most recent
    // SimulationStep() call) at `options`' real pixel dimensions. Pure,
    // side-effect-free, safe to call any number of times -- including
    // zero or several times between SimulationStep() calls -- since it
    // never touches secondary-motion state or re-derives anything
    // frame-selection-related.
    graphics::PixelBuffer Present(graphics::RasterOptions options) const;

    // The real, cached Frame itself -- exposed directly for a real
    // windowed presenter (e.g. a real X11/GPU blit) that wants the
    // logical Frame, not just CPU-rasterized pixels.
    const graphics::Frame& CachedFrame() const { return cachedFrame_; }

    HitmMatchSnapshot Snapshot() const { return match_.Snapshot(); }
    const std::string& LastError() const { return lastError_; }

    // Convenience: wires SimulationStep/Present into a real
    // core::Application's generic hooks -- Application itself never
    // sees a HitmMatch, only these two std::function callbacks (see
    // this file's own top comment). `sink`, if set, receives every
    // presented PixelBuffer (a real windowed/file presenter's own
    // job); `options` is the real pixel size Present() uses.
    void RegisterInto(core::Application& app, graphics::RasterOptions options,
                        std::function<void(const graphics::PixelBuffer&)> sink = nullptr);

private:
    HitmApplicationLoop(HitmMatch match, HitmAssetBundle bundleA, HitmMoveInstance specialA,
                         graphics::TextureAtlas atlasA, std::string fighterIdA, HitmAssetBundle bundleB,
                         HitmMoveInstance specialB, graphics::TextureAtlas atlasB, std::string fighterIdB,
                         double displayHeight, int viewWidth, int viewHeight);

    void UpdatePresentationCamera(const HitmMatchSnapshot& snapshot);

    HitmMatch match_;

    HitmAssetBundle bundleA_;
    HitmMoveInstance specialA_;
    graphics::TextureAtlas atlasA_;
    std::string fighterIdA_;
    HitmSecondaryMotionState secondaryMotionA_;

    HitmAssetBundle bundleB_;
    HitmMoveInstance specialB_;
    graphics::TextureAtlas atlasB_;
    std::string fighterIdB_;
    HitmSecondaryMotionState secondaryMotionB_;

    double displayHeight_;
    int viewWidth_;
    int viewHeight_;

    graphics::Camera camera_;
    graphics::Frame cachedFrame_;
    std::string lastError_;

    std::function<bool(int)> isPressedA_ = [](int) { return false; };
    std::function<bool(int)> isPressedB_ = [](int) { return false; };
};

}  // namespace dominus::character::hitm
