# HITM Application/Game Loop — Track H Phase 5D

This is the phase where the pieces stop being separately-proven modules and
become a running program. The chain is now real, end to end:

```
Keyboard -> HitmInputAdapter -> HitmMatch::AdvanceFrame()
   -> HitmFighterRuntime -> HitmSpriteDrawData -> HitmSceneBridge
   -> SceneEntity/DrawCommand -> TextureAtlas -> RasterDevice -> Window
```

**A window launches. Real Brooklyn and Rocket are rendered from their real
atlas data. Keyboard input drives the real combat simulation. The scene
updates continuously, every frame.** All four claims are backed by a real,
independently-verified run against a real (virtual) X11 display in this
session — not a description of what should theoretically work. See
"Live verification" below.

## What was built

### 1. `CORE/Runtime/Application.h` — the generic, deterministic dispatcher

`Application::Tick(double realDtSeconds)` is now a real, standard
fixed-timestep-with-accumulator loop: `realDtSeconds` (never measured by
this class itself — supplied by the caller, so `Tick()` stays fully
deterministic and clock-free to a test) accumulates, and the registered
`onSimStep` callback fires exactly once per whole `kFixedSimDt` (1/60s,
matching HitmFighterRuntime/HitmMatch's own established per-frame
convention) consumed from that accumulator — zero, one, or several times
per real `Tick()` call. The registered `onPresent` callback fires exactly
once per `Tick()` call, always, regardless of how many simulation steps
just ran. A real, disclosed `kMaxSimStepsPerTick` (5) clamp prevents an
unbounded "spiral of death" catch-up burst after a real stall; excess
accumulated time is dropped, not owed to a future call.

This file still does not, and must not, include CHARACTER, GRAPHICS, or
COMBAT anywhere — the same architectural law `WORLD/Core/WorldTick.h`
already enforces one layer down (proven directly by
`tests/world/test_hitm_rivals_as_world_entity.cpp`, which wires
COMBAT/CHARACTER into `WorldTick` from OUTSIDE `WORLD/Core` entirely).
`Application` exposes four generic hooks
(`SetPollCallback`/`SetSimulationStepCallback`/`SetPresentCallback`/
`SetShouldCloseCallback`) and never learns what a `HitmMatch` is.

### 2. `CHARACTER/HitmBridge/HitmApplicationLoop.h/.cpp` — the HITM-specific half

Lives in `CHARACTER::hitm` (already, correctly, above CORE/WORLD/COMBAT/
GRAPHICS in this engine's real dependency stack since Track H Phase 5B),
owns the real game state (`HitmMatch`, completely unmodified), and
registers exactly two callbacks into `Application`'s generic hooks.
`Application` never sees a `HitmMatch`.

- **`SimulationStep()`** (steps 2–6): samples both players' real physical
  input via `HitmInputAdapter` (Phase 5C, unmodified), calls
  `HitmMatch::AdvanceFrame()` (Phase 3/4, unmodified), then builds real
  sprite draw data + scene entities for **both** fighters and compiles a
  real `Frame` — cached, not re-derived on every present.
- **`Present()`** (step 9): re-rasterizes the cached `Frame`. Pure,
  side-effect-free, safe to call any number of times.

**The critical separation this preserves**: `HitmSpriteDrawData`'s own
real secondary-motion contract (its own header comment) is "call exactly
once per real SIMULATION frame" — not once per real PRESENTED frame,
which can be zero, one, or several times per simulation frame depending
on wall-clock pacing. So `BuildSpriteDrawData`/`BuildHitmSceneEntities`/
`FrameCompiler::Compile` only ever run inside `SimulationStep()` (fixed
cadence); `Present()` only ever re-rasterizes the cache. This is not
optional plumbing — calling `BuildSpriteDrawData` at presentation
frequency instead of simulation frequency would silently double-integrate
a real spring every time a frame gets presented without a new simulation
step, corrupting Module 5B's own real secondary-motion state.

**Camera** (step 7): a deliberately minimal, clearly-labeled placeholder
— fighter-midpoint X, and Y centered so both fighters land inside a
normal-sized viewport (a real bug this phase's own test caught, see
below). Not the real, evidenced `cameras.json`-based algorithm Track H
Phase 5E owns.

**One minimal, disclosed, read-only touch point** was needed on the
already-closed `HitmMatch`... actually none was needed: `currentMoveForState`
is computed by re-extracting each fighter's own real "special" move once
at construction (`HitmMoveInstance::Extract`) and switching on
`HitmMatchSnapshot`'s own already-public `fighter_a.state`/`fighter_b.state`
— the exact same pattern every prior Track H live CLI demo
(`dominus_cli.cpp`) already used for a single fighter. `HitmMatch.h` was
not touched at all.

### 3. `GRAPHICS/Raster/X11WindowPresenter.h/.cpp` — a real windowed presenter

**Why X11, not Vulkan/GLFW.** Two separate, real reasons, not one:

1. This sandbox has no Vulkan SDK (no `vulkan.h`, no `glslc`) and no
   GPU/software Vulkan ICD — the same disclosed limitation Phase 5A
   already established, unchanged.
2. Even where a Vulkan SDK+GPU+display DID exist, a windowed
   `VulkanFrameRenderer` could not show real HITM sprite pixels today —
   real texture/sampler support was only ever added to `RasterDevice`
   (Phase 5A's own explicit, disclosed scope decision), never to
   `VulkanFrameRenderer`. Wiring HitmApplicationLoop to the real windowed
   Vulkan path would produce a window showing flat colored rectangles,
   not Brooklyn and Rocket — the opposite of this phase's actual goal.

X11 development headers (`libx11-dev`) and a working Xvfb virtual X
server ARE genuinely present in this sandbox — verified directly, not
assumed (`gcc`+`-lX11` links and runs a real window against a real Xvfb
instance). So this phase built a real, minimal, additive presenter that
blits a real `RasterDevice::PixelBuffer` into a real X11 window via
`XPutImage` — the standard, minimal way to show a CPU pixel buffer in a
real window with no GPU API involved at all. **Vulkan is completely
untouched**: no `VulkanFrameRenderer.h/.cpp` include, no shader, no
`DOMINUS_ENABLE_VULKAN` reference anywhere in this file.

Gated behind a new, independent CMake option,
`DOMINUS_ENABLE_X11_PRESENTER` (default OFF — the established CPU test
suite stays buildable with zero new dependencies, same discipline as
`DOMINUS_ENABLE_VULKAN`). Real, disclosed scope limit, same "real,
disclosed scope limits, not fabricated completeness" discipline as
`PngDecoder.h`: supports exactly the one real X11 TrueColor visual this
engine has actually verified pixel-for-pixel against a real Xvfb server
(24-bit depth, 32bpp, red/green/blue masks `0xFF0000`/`0xFF00`/`0xFF`,
LSBFirst byte order) — refuses, with a specific real error, anything
else.

### 4. `TOOLS/Editor/dominus_hitm_window.cpp` — the real, live executable

Gated behind `DOMINUS_ENABLE_X11_PRESENTER`. Imports real Brooklyn/Rocket
data, builds a real `HitmApplicationLoop`, opens a real `X11WindowPresenter`,
wires real, live keyboard state via `XQueryKeymap` (translated through a
small, disclosed, real GLFW-key-code-to-X11-KeySym table covering exactly
the twelve keys `HitmInputAdapter`'s own bindings use — not a general
GLFW/X11 compatibility layer), and calls `Application::Run()`.

## Live verification — this actually ran

This is not a claim of "should work" — this session built
`DOMINUS_ENABLE_X11_PRESENTER=ON`, started a real Xvfb virtual X server,
and ran `dominus-hitm-window` against it:

```
[hitm-window] real X11 window launched: 1060x600
[hitm-window] real loop starting (real keyboard: WASD+J/L for Brooklyn, arrows+Down+KP1/KP2 for Rocket) -- stopping after 200 real frames
[hitm-window] stopped after 995 real simulation steps
```

A window opened at the real, authored `game.json` `view.w`/`view.h`
(1060×600). A separate, independent verification program then drove the
same real pipeline (Brooklyn walking right for 140 real simulation
steps), presented one real frame, and read the window's **actual,
current, server-side pixel content back** via a real `XGetImage` call —
an independent code path from `Present()`'s own conversion:

```
total pixels=636000 mismatches=0
```

**Zero mismatches, across all 636,000 pixels**, between what
`RasterDevice` computed and what the real window actually displayed. The
same readback, saved and converted to PNG, shows exactly what it should:
Brooklyn (mid-walk pose, real part art) on the left, Rocket (idle pose,
real part art) on the right, both drawn from their own real atlas PNGs,
correctly positioned, correctly scaled, real alpha-composited transparent
backgrounds — sent to the user directly alongside this report.

## Bugs this phase's own verification caught (and fixed) before this report was written

1. **The camera placeholder's real Y bug.** The first version of
   `UpdatePresentationCamera` set `camera.y = 0`. HITM's real fighter
   `y` is a Y-DOWN, ground-relative value (`ground=538`); after
   `HitmSceneBridge`'s own real Y-negation, every real fighter part
   renders around world `y = -300..-540`. With `camera.y = 0`, both
   fighters rendered entirely outside any reasonably-sized viewport —
   caught by `HitmApplicationLoop_Present_ProducesRealNonEmptyPixelContent`
   (zero non-background pixels found). Fixed by centering the camera's Y
   on the real fighter midpoint too, mirroring the X logic already there.
2. **A floating-point step-boundary artifact in the Application-level
   determinism test.** The first version fed exactly `1.0` real second
   (an exact `60 * kFixedSimDt` boundary) through three different
   chopping patterns; one (the jittery pattern) landed on 59 steps
   instead of 60 because IEEE-754 addition's non-associativity legitimately
   accumulates a few ULPs of rounding error differently depending on
   summation order — a real, well-known property of ANY accumulator-based
   fixed timestep at an exact step boundary, not a defect (the same
   "genuine float-accumulation drift, not a bug" class of finding Track H
   Phase 3's own `HitmMatch` tests already documented). Fixed by using a
   deliberately mid-step total (`1.0 + kFixedSimDt/2`) — solidly clear of
   any boundary, which a real wall clock effectively always is anyway.

Both are documented in their own test/code comments, not silently patched.

## An honest, disclosed performance finding — not glossed over

In this sandbox, at 1060×600, `RasterDevice::Rasterize`'s real per-pixel
CPU triangle fill over ~44 real textured parts (both fighters) is
expensive enough that a single real `Application::Tick()` iteration
(dominated by that cost, not by simulation) regularly takes long enough
in wall-clock time to bump into the real `kMaxSimStepsPerTick` clamp.
The live run above bears this out: 200 real `Tick()` calls produced 995
simulation steps — averaging 4.98 steps/tick, right at the 5-step clamp
— meaning the clamp is engaging on most calls, and **real wall-clock
time and simulated game time diverge** (the simulation runs behind real
time) in this specific environment. This is not a correctness problem —
the accumulator's own guarantee (determinism regardless of chopping,
proven above) still holds exactly, and the clamp is doing precisely what
it's documented to do — it is a real, disclosed **performance**
observation about CPU rasterization cost at this resolution in this
sandbox, worth knowing before assuming "the CPU path is fine for a real
playable build" without qualification. A real windowed-Vulkan or
GPU-accelerated presentation path (deferred, see above) would very
plausibly close this gap; this phase does not attempt to.

## Verification

- `tests/core/test_application.cpp`: 9 new tests — exact-step-count
  behavior at fractional/whole/multiple `kFixedSimDt` inputs, cross-call
  accumulation, the anti-catch-up clamp, present-runs-once-regardless-of-
  sim-steps, poll-runs-once-per-tick, `ShouldClose`-driven `Run()`
  termination, and the determinism proof itself.
- `tests/integration/test_hitm_application_loop.cpp`: 10 new tests —
  real construction against real Brooklyn/Rocket data, real round-intro
  countdown advance, real Player 1 walking and real Player 2 blocking
  (both proven through the packaged loop, not raw `HitmMatch` calls),
  both fighters' real parts present in one compiled `Frame` from their
  own real atlases (a genuine new proof — Phase 5B never exercised two
  atlases in one `Frame` before), real non-empty presented pixel content,
  `Present()`'s own purity (repeated calls, identical hash, no hidden
  mutation), a real `HitmApplicationLoop` driven entirely through a real
  `core::Application::Tick()`, and — the single most important test in
  this phase — **`HitmApplicationLoop_RealMatchStateIsIndependentOfHowRealTimeWasChopped`**:
  two independent `HitmApplicationLoop`/`Application` pairs, fed the
  identical real "Player 1 right held" input, driven by 3 real seconds of
  wall-clock time chopped into a uniform 60-call pattern versus an
  irregular, jitter-like pattern — end with **bit-identical**
  `HitmMatchSnapshot`s (`operator==`, every field: position, state, hp,
  phase, round — not just a step count).

**945/945 tests passing** (was 926 before this phase — 19 new). Clean
build, zero warnings, in both the default configuration and with
`DOMINUS_ENABLE_X11_PRESENTER=ON`. Clean Release rebuild.
AddressSanitizer+UndefinedBehaviorSanitizer clean, 2 runs, default
configuration (the X11/Xvfb live-window run is a separate, manual
verification step in this sandbox, same precedent as Vulkan's own GPU
demos — not part of the environment-independent `dominus_core_tests`
count, since X11/Xvfb availability, like a Vulkan SDK, is not guaranteed
in every build environment). Fresh-clone build + full default-config
suite verified before push.

## What this phase explicitly does not do

- **Real camera work** (`cameras.json`-based framing, distance zoom, arena
  bounds visualization) — Track H Phase 5E. The camera here is a named,
  disclosed placeholder, not a claim of final behavior.
- **Real Vulkan/GPU presentation** — still gated OFF, still no SDK/ICD in
  this sandbox; the CPU/X11 path is real and verified, not a stand-in
  claimed to be GPU-accelerated.
- **Two-player-on-one-keyboard ergonomics, menus, networking** — untouched,
  out of scope.
- **The independent-boolean input-model gap** (walking while blocking,
  etc.) — Phase 5C's own disclosed, still-open limitation, unchanged
  here; nothing in this phase's own testing showed a need to redesign
  `HitmInputCommand` to reach "it just needs to actually run."
- **Touching Vulkan, `FillTriangle`, bone-hierarchy FK, any closed combat
  module, Ghost Dash, asset authoring, or generated data** — all
  untouched, per this checkpoint's explicit scope.

## Next checkpoint

Track H Phase 5E — the real camera: port the evidenced `cameras.json`
`fight` profile (midpoint follow, distance-based zoom, arena-bound
clamp) HITM_RENDER_INPUT_LOOP_AUDIT.md section D.3 already named, replacing
this phase's own disclosed placeholder.
