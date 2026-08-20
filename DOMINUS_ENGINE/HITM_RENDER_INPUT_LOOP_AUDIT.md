# DOMINUS → HITM Rivals: Rendering / Input / Game-Loop / Camera Readiness Audit

**Status: pure dependency-map audit. Zero implementation. No code in this
repository was changed to produce this document.** Same discipline as
`HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md` (commit `72b0c38`), which
stayed untouched through all four of its own implementation phases and is
not touched by this document either.

**Baseline this audit was taken against:** commit `90f4535` (Track H
Phase 4, "Rocket's real Ghost Dash now connects"), 878/878 tests passing.
Brooklyn vs Rocket is a fully real, CPU-observable, match-level combat
simulation — real hp/damage/hitstun/hitstop/KO/round-win/match-win, driven
by real HITM data, verified under ASan/UBSan, deterministic. **Nothing
about that simulation is questioned, extended, or touched here.**

**What this document answers:** what stands between that proven
CPU-observable simulation and an actual windowed, player-controlled fight.
Five areas, exactly as requested: (A) rendering bridge, (B) input, (C)
game loop, (D) camera, (E) the vertical-slice executable itself.

**Explicitly out of scope for this audit** (per the request that produced
it): no implementation, no third fighter, no online multiplayer, no
menus, no cosmetics, no FK invention, no Ghost Dash blocking fix, no
generated data, no new combat mechanics. Where a real gap touches one of
these topics, it is named and left alone.

---

## Method

Every claim below is either (a) a file:line citation into this repository
at commit `90f4535`, or (b) a citation into the real hitm-engine reference
source at
`scratchpad/inspect/hitm-engine/` (the same reference tree Track H has
cited throughout — real `engine/*.js`, real `data/system/*.json`). No
claim in this document is inferred from a filename or a comment alone
without reading the code it describes.

---

## A. Rendering bridge

### A.1 What DOMINUS actually has, proven and working today

A real, complete, already-tested pipeline exists, with a hard architectural
invariant enforced by construction (`GRAPHICS/Renderer/Frame.h:1-30`):

```
WORLD::EntityRegistry
      |
GRAPHICS::SceneFromEntities   (GRAPHICS/Renderer/SceneFromEntities.h)
      |
GRAPHICS::Scene               (GRAPHICS/Renderer/Scene.h)
      |
GRAPHICS::FrameCompiler       (GRAPHICS/Renderer/FrameCompiler.h)
      |
GRAPHICS::Frame               (GRAPHICS/Renderer/Frame.h)  <- the ONLY channel past this point
      |
RasterDevice (CPU)  |  VulkanFrameRenderer (real GPU, windowed via GLFW)
```

Confirmed real, not aspirational:
- `SceneFromEntities::Build` reads real `world::SpatialComponent` (x/y/z)
  and an optional `MaterialGenomeComponent` off real WORLD entities and
  produces a real `Scene` (`SceneFromEntities.h:72-87`).
- `FrameCompiler::Compile` turns a `Scene` + `Camera` into a `Frame` with a
  real, deterministic per-command `screen_transform` and a real content
  hash (`FrameCompiler.h:25-51`).
- Two real renderers consume the exact same `Frame` type: `RasterDevice`
  (CPU, `GRAPHICS/Raster/RasterDevice.h`) and `VulkanFrameRenderer` (a
  real GPU path — real GLFW window, real Vulkan instance/device/swapchain/
  pipeline/command buffers, `GRAPHICS/Vulkan/VulkanFrameRenderer.h:59-79`).
  `VulkanFrameRenderer::Initialize/Render/PollEvents/ShouldClose` is a
  real windowed present loop, exercised end-to-end by
  `TOOLS/Editor/dominus_gpu_demo.cpp:35-39` (`while (!renderer.ShouldClose())`).
- A real offscreen GPU path also exists (`InitializeHeadless` /
  `RenderOffscreen`, no window required), proven against real WORLD
  entity data by `TOOLS/Editor/dominus_gpu_scene_test.cpp`.

**This means a window that opens, presents frames, and closes cleanly is
already a solved, tested problem in this engine.** That is not the gap.

### A.2 What is drawn today: colored rectangles, not sprites

Both renderers are explicit and self-documented about this, not silently
approximate:

- `RasterDevice.h:8-28`: "What this is NOT: a mesh renderer... There is no
  real geometry anywhere for a rasterizer to draw... draws exactly that —
  one filled rectangle per command, sized by the real transform, colored
  by a real Sha256 of the real material_ref."
- `GRAPHICS/Renderer/Mesh.h:11-18`: "Positions only — explicitly no
  normals, no UVs... no texture/image asset representation anywhere for a
  UV to sample."
- `GRAPHICS/Vulkan/VulkanFrameRenderer.cpp:1967`: "texture, no sampler, no
  image" — confirmed by grep across the entire renderer: **zero
  `VkSampler`, zero texture-image, zero UV anywhere in either renderer.**
- `DrawCommand` (`Frame.h:38-50`) carries `material_ref`/`mesh_ref`
  strings and a resolved flat RGB color — there is no field anywhere on
  `DrawCommand` or `SceneEntity` for an atlas source rect, a texture
  handle, or a per-part pivot/offset.

**Real finding: DOMINUS's rendering path today can draw a positioned,
colored rectangle per entity. It cannot draw a textured sprite at all —
not "not yet wired," but structurally absent: no image/texture asset
type, no sampler, no UV, nothing for a real HITM atlas PNG to be sampled
into.** This is the single largest gap in this section, and it sits below
the "connect the pipes" level — it is a missing capability, not a missing
wire.

### A.3 What HitmSpriteDrawData already computes — and where it dead-ends

Module 5B (`CHARACTER/HitmBridge/HitmSpriteDrawData.h`) is real and
already proven: `BuildSpriteDrawData()` takes a `HitmFighterSnapshot` +
the current move + a real asset bundle and produces, per real drawn part,
in real `drawOrder`: the exact real atlas pixel rect (`frame_x/y/w/h` from
`parts.json`), real normalized size/pivot, real placement (`rig.json`),
and real per-clip sampled pose (rotation/offset from `anim.json`, with
real spring-driven secondary motion for follow bones). This is a direct,
verified, line-by-line port of hitm-engine's own
`AnimationSystem.js`/`SkeletonSystem._secondary()` — see the module's own
header comment for the full, cited derivation.

**Confirmed by grep: `HitmSpriteDrawData`/`HitmPartDraw` is referenced
only inside `CHARACTER/HitmBridge/*`, `TOOLS/Editor/dominus_cli.cpp`, and
tests. No file under `GRAPHICS/` includes or reads it. There is no
function anywhere that turns a `HitmPartDraw` into a `SceneEntity` or a
`DrawCommand`.** The two pipelines — "HITM real per-part draw data" and
"DOMINUS Scene→Frame→pixels" — have never been connected, in either
direction, for any fighter.

Also confirmed (`TOOLS/Editor/dominus_cli.cpp:886-964` vs. `:1037-1206`):
the existing `hitm-sprite` CLI demo calls `BuildSpriteDrawData` for a
**single, standalone fighter**; the existing `hitm-match` CLI demo (the
proven Brooklyn-vs-Rocket match driver) **never calls
`BuildSpriteDrawData` at all**. Combat simulation and sprite/animation
derivation have not yet been run together, even in a text-only demo, let
alone rendered.

### A.4 Where fighter position actually lives — a second, private bridge gap

`HitmFighterRuntime` internally owns its *own* private `world::World` /
`EntityRegistry`, with one entity carrying a real
`SpatialComponent::Fighter2_5D` (`HitmFighterRuntime.cpp:104`), ticked
through a real `WorldTick`-registered lambda
(`HitmFighterRuntime.cpp:122`). This is real, and it is the reason
Module 5A's `x`/`y` are WORLD-law-compliant 2.5D positions, not ad hoc
floats.

But this internal `World` is private — there is no public accessor on
`HitmFighterRuntime` for its entity id or its `EntityRegistry`. The only
way out is `Snapshot()`, a plain `HitmFighterSnapshot` struct (not a WORLD
entity). **`SceneFromEntities` — the one real bridge from WORLD into
rendering — has nothing to read for Brooklyn or Rocket**: their real
positions exist, but not in any `EntityRegistry` a renderer-facing bridge
could see. Wiring this requires a small new adapter (copy
`HitmFighterSnapshot` fields into an externally-visible registry entity,
or bypass WORLD entirely and build a `Scene`/`SceneEntity` directly from
the snapshot each frame) — not a redesign of either side, but a real,
currently-nonexistent piece.

### A.5 Is full FK actually necessary for first visible presentation? No — evidenced, not assumed

`ANIMATION/SkeletonSystem`'s bone-hierarchy FK machinery is a *separate*,
more general system than what Module 5B already does. HitmSpriteDrawData
does not walk a bone hierarchy at all — it places each real 2D part
directly from `rig.json`'s own authored placement rect plus the part's
own sampled local pose, exactly mirroring hitm-engine's own real,
non-FK, flat-parts convention (see `HitmSpriteDrawData.h:164-190`'s
`HitmPartDraw` fields: placement + local pose per part, no parent-bone
transform composition beyond what secondary motion already spring-drives
per follow bone). **This is already the complete real algorithm the
actual game uses for 2D fighter presentation — full generalized FK is a
different, unrelated capability (relevant to 3D/other DOMINUS content),
not a prerequisite for drawing Brooklyn or Rocket.** The blocking gap is
A.2/A.3 (no texture path, no Scene bridge), not skeletal FK.

### A.6 Section A summary — what's real, what's missing

| Piece | Status |
|---|---|
| Windowed present loop (open/render/poll/close) | **Real, proven** (`VulkanFrameRenderer`, `dominus_gpu_demo.cpp`) |
| Scene → Frame → pixels pipeline | **Real, proven**, rectangles only |
| Real per-part atlas rect / placement / pose derivation | **Real, proven** (`HitmSpriteDrawData`) |
| Texture/image/sampler/UV support in either renderer | **Absent, self-documented as absent** |
| Bridge: `HitmPartDraw` → `SceneEntity`/`DrawCommand` | **Absent** |
| Bridge: `HitmFighterRuntime`'s private position → any externally-visible entity | **Absent** |
| Combat driver (`HitmMatch`) + sprite driver (`BuildSpriteDrawData`) run together | **Never done, even in text form** |
| Full bone-hierarchy FK | **Not required** for this presentation (evidenced above) |

---

## B. Input

### B.1 What exists

A real GLFW window exists and pumps its OS event queue every frame
(`VulkanFrameRenderer::PollEvents` → `glfwPollEvents()`,
`VulkanFrameRenderer.cpp:820-821`). That call is necessary for the window
to stay responsive (resize, close-button) — it is **not** input sampling.

Confirmed by repo-wide grep: **zero occurrences of `glfwGetKey`,
`glfwSetKeyCallback`, `glfwGetJoystickState`, or any other key/gamepad
read anywhere in this codebase.** No keyboard or controller state has
ever been sampled by DOMINUS.

### B.2 The real consumption seam already exists, and is good news

`HitmFighterRuntime::AdvanceFrame(HitmInputCommand)` and
`HitmMatch::AdvanceFrame(HitmInputCommand inputA, HitmInputCommand inputB)`
are **already** the real, per-frame, discrete-command consumption points
(`HitmFighterRuntime.h:355-362`, `HitmMatch.h:152`). Nothing about the
combat state machine needs to change to accept human input instead of a
scripted CLI sequence — it already takes one `HitmInputCommand` per
fighter per frame and has done so since Module 5A. This is the one part
of "input" that is not a gap.

### B.3 What's missing, and one real, disclosed model gap

To go from "a key is down" to "a command reaches `AdvanceFrame`" needs:
one new, small function (`GLFWwindow* → HitmInputCommand`, or two calls,
one per fighter) plus the actual `glfwGetKey` polling it doesn't do yet.
Straightforward, and does not touch `HitmFighterRuntime`/`HitmMatch`.

One real, worth-naming architecture gap surfaces on comparison with
hitm-engine's own real `InputSystem.js`
(`scratchpad/inspect/hitm-engine/engine/input/InputSystem.js:8-53`): the
real engine's per-frame input is an **independent-boolean struct**
(`{left,right,up,down,light,heavy,special,meter,dash}` — several can be
true simultaneously, e.g. holding block while walking). DOMINUS's
`HitmInputCommand` is a **single mutually-exclusive enum**
(`kNeutral|kLeft|kRight|kJump|kSpecial|kBlock`,
`HitmFighterRuntime.h:355-362`) — one command per frame, by design (see
the enum's own comment: "no real authored data exists" yet for a second
attack button). This is a real, already-disclosed simplification from
Module 5A, not a new finding this audit invents — but it is directly
relevant here: **holding block while walking, or moving in one direction
while attacking, is not representable in the current input model at
all**, independent of where the input bytes come from. That is a combat
state-machine question, explicitly out of this audit's scope ("no new
combat mechanics"), but it will eventually gate how "playable" the game
feels once real human input is wired in, so it is named here rather than
discovered silently later.

### B.4 Section B summary

| Piece | Status |
|---|---|
| Window event pump | **Real** (`PollEvents`) |
| Keyboard/gamepad state sampling | **Absent** — zero calls anywhere |
| Per-frame discrete command consumption by the state machine | **Real, already proven** (`AdvanceFrame`) |
| Independent simultaneous inputs (block+walk, move+attack) | **Not representable** by the current single-enum command model (pre-existing, disclosed Module 5A scope, not new) |

---

## C. Game loop

### C.1 What exists: an empty scaffold, not a running loop

`CORE/Runtime/Application.h` is real but explicitly a stub:
`Run()`/`Tick()` exist (`Application.h:37-62`), but `Tick()`'s entire body
is `jobSystem_->WaitIdle()` — the header's own comment says so plainly:
"Phase 1: empty tick. Systems attach here starting Phase 2"
(`Application.h:59-60`). **No dt is computed, no WorldTick is called, no
rendering happens, no frame pacing (no sleep, no vsync wait, no
target-FPS clamp) exists anywhere in this file.** `Run()`'s `while`
loop spins as fast as the OS schedules it.

`WORLD/Core/WorldTick.h` is real and does carry a `dt` concept
(`Tick(EntityRegistry&, float dt)`, `WorldTick.h:31-35`, ordered-system
list, deterministic registration order) — but `Application::Tick()` never
calls it. The only place `WorldTick` is actually exercised today is
directly, by hand, inside test code
(`tests/world/test_hitm_rivals_as_world_entity.cpp`) and inside
`HitmFighterRuntime`'s own *private*, per-fighter internal `WorldTick`
instance (A.4 above). There is no single, shared, top-level game loop
anywhere in this repository that ties input → simulation → animation →
rendering together, for anything, HITM or otherwise.

### C.2 What's already real and correctly ordered, at the simulation layer

This is the good news for section C: everything *inside* one simulation
step is already real, frame-accurate, and proven:
- `HitmMatch::AdvanceFrame` already resolves, in the correct real order
  per frame: round/timer/KO-window phase advance → both fighters'
  `AdvanceFrame` → attack resolution (melee or rush) → round/match state
  transitions (`HitmMatch.cpp`, exercised end-to-end by
  `HITM_MATCH_REPORT.md`'s live transcript).
- Hitstop is already real and already frame-frozen correctly: Module 5A's
  `state_frame` is documented as "frozen during hitstop"
  (`HitmSpriteDrawData.h:56` citing the real
  `state_frames_remaining`/hitstop countdown in `HitmFighterRuntime`).
- Determinism is already proven, not assumed: identical input sequences
  produce byte-identical `HitmMatchSnapshot`s
  (`HitmMatch_Fight_IdenticalInputSequencesProduceIdenticalStates`,
  `tests/integration/test_hitm_match.cpp`), and divergent ones produce
  divergent state.

**What's missing is not simulation correctness or ordering — it's the
outer loop that would call it at a real cadence, and that would call
animation/rendering around it.** Concretely, absent today, anywhere:
1. A real per-frame `dt`/tick-rate binding (the real HITM engine and
   Module 5A's own frame model are both implicitly "1 simulation frame
   == 1/60s," matching `hitm-engine`'s `requestAnimationFrame`-driven
   60fps convention — but nothing in DOMINUS enforces or measures this
   against wall-clock time yet).
2. A call site that runs, in order, once per real frame: sample input
   (B) → `HitmMatch::AdvanceFrame` (proven) → `BuildSpriteDrawData` for
   both fighters (proven, but never called from the match path — A.3) →
   assemble a `Scene`/`Frame` (needs the A.4 bridge) → `Render`/
   `Rasterize` → `PollEvents`.
3. Frame pacing: nothing here needs inventing from scratch — a bounded
   `while (!ShouldClose())` loop already exists as a *pattern*
   (`dominus_gpu_demo.cpp:35-39`) for the render-only case; it has never
   been combined with a simulation step.

### C.3 Section C summary

| Piece | Status |
|---|---|
| Per-simulation-step ordering (input→combat→hit-resolution→state) | **Real, proven, deterministic** (`HitmMatch::AdvanceFrame`) |
| Hitstop freeze semantics | **Real, proven** (`state_frame`) |
| Top-level app loop that calls WorldTick/simulation | **Absent** (`Application::Tick` is empty) |
| Fixed timestep / wall-clock pacing | **Absent** anywhere in the repo |
| Single loop combining input+sim+animation+render | **Never built, even as a demo** |

---

## D. Camera

### D.1 What exists

A real, already-used 2D camera: `GRAPHICS::Camera { x, y, zoom,
rotation_deg }` plus `ToCameraSpace()`, a real world→screen transform
already consumed by `FrameCompiler::Compile` for every entity
(`GRAPHICS/Renderer/Camera.h`). This is genuinely wired into the proven
rendering pipeline today — a `Frame` always carries a real camera; it is
simply always whatever fixed `Camera` value the caller supplies. No
per-frame camera *update* logic exists anywhere (nothing computes a new
`x`/`zoom` from fighter state frame to frame) — every existing caller
(tests, `dominus_gpu_scene_test.cpp`) constructs one static `Camera` and
reuses it.

### D.2 Arena boundaries: already real, already enforced — just never drawn

This is a genuine, positive finding, not a gap: real arena boundary data
already exists in DOMINUS and is **already enforced in simulation**, not
just imported inertly. `HitmGameRules`/`HitmPhysicsRules` import the real
`data/system/game.json`'s `physics.wallL`/`wallR`/`ground`
(`HitmGameRules.cpp:60-61`, real values 64/996/538 —
`scratchpad/inspect/hitm-engine/data/system/game.json:3`), and
`HitmFighterRuntime`'s own frame update already clamps horizontal
position to those exact real bounds every frame
(`HitmFighterRuntime.cpp:253-254`:
`if (spatial->x < wall_l) ... if (spatial->x > wall_r) ...`). **A fighter
already cannot walk through the real stage's walls in the proven
simulation.** What's missing is purely visual: nothing consumes
`wall_l`/`wall_r`/`ground` to clamp or frame the *camera* — because no
camera-update logic exists yet at all (D.1).

### D.3 Real, camera-specific authored data exists and was deliberately never imported

`HitmGameRules.h`'s own header comment is explicit and names this exact
gap itself, before this audit: "`data/system/cameras.json`... deliberately
OUT OF SCOPE — the audit and roadmap named `game.json` specifically for
this module; camera and VFX data are real, separate gaps for later,
separately-gated work, not silently folded in here"
(`HitmGameRules.h:14-17`). That file is real and present in the reference
tree (`scratchpad/inspect/hitm-engine/data/system/cameras.json`) — never
read by any DOMINUS code, confirmed by grep (zero references to
`cameras.json` or `cameras` outside that one disclaiming comment).

Its real `fight` profile is exactly "two-fighter framing": midpoint-x
follow, distance-based dynamic zoom, both lerped, with an air-height zoom
cap and a view-bound clamp — a complete, small, self-contained algorithm
(`scratchpad/inspect/hitm-engine/engine/camera/CameraSystem.js:18-34`):

```js
const [a,b]=s.fighters, F=this.p.fight;
let fx=(a.x+b.x)/2, dist=Math.abs(a.x-b.x);
let tz=Math.max(F.minZoom, Math.min(F.maxZoom, F.distanceBias/(dist+F.distanceOffset)));
// ...lerp toward (fx, tz) by followLerp/zoomLerp each frame...
// ...clamp fx so the view rect stays inside [0, game.json view.w]...
```

This maps directly onto DOMINUS's existing `Camera{x,zoom}` — it is a
real, small, already-fully-specified algorithm, not something that would
need inventing. It is explicitly not implemented here (out of scope: "no
new combat mechanics" / this is inspection only), but it is the concrete
answer to "how would two-fighter framing actually work" when that work is
authorized.

### D.4 Section D summary

| Piece | Status |
|---|---|
| Camera struct + world→screen transform | **Real, proven, already wired into FrameCompiler** |
| Per-frame camera *update* (follow/zoom logic) | **Absent** — every caller uses one static Camera |
| Arena boundary data (`wallL`/`wallR`/`ground`) | **Real, imported, and already enforced in simulation** |
| Camera consuming arena boundaries | **Absent** (no camera-update logic exists to consume them) |
| Real two-fighter dynamic-framing algorithm (`cameras.json`) | **Exists in the real source, imported by nothing, explicitly named out-of-scope by Module 4's own header** |

---

## E. Actual vertical-slice executable: what exactly is missing

Can Brooklyn vs Rocket become a windowed, playable fight today? **No** —
but not because of one big missing system. Every individual piece listed
below is either already real and proven, or a small, bounded, well-scoped
addition; nothing requires re-deriving combat data, inventing FK, or
touching the closed Track H combat modules.

Ordered by real dependency (each depends only on items above it):

1. **Texture/image asset support in a renderer** (A.2) — the actual
   largest item: load a real HITM atlas PNG into a GPU (or CPU) texture,
   add a sampler/UV path to `RasterDevice` and/or `VulkanFrameRenderer`,
   extend `DrawCommand` to carry a source rect. Nothing upstream of this
   depends on it, but nothing downstream can show real pixels without it.
2. **`HitmPartDraw` → drawable-command bridge** (A.3) — a new, small,
   pure function turning `HitmSpriteDrawData`'s real per-part output into
   whatever the item-1 renderer needs (extended `DrawCommand`s, or a new
   parallel type if `Scene`/`Frame` aren't the right shape for per-part
   atlas draws — an open design question this audit does not resolve).
3. **Fighter-position visibility bridge** (A.4) — expose
   `HitmFighterRuntime`'s real x/y/facing/state to whatever assembles the
   scene each frame, without breaking its existing private-`World`
   encapsulation (an adapter, not a `HitmFighterRuntime` change).
4. **Input sampling + mapping** (B) — `glfwGetKey` polling plus a small
   `GLFWwindow* → HitmInputCommand` mapping function, per player.
5. **The actual top-level loop** (C) — the one new piece of orchestration
   code that, once per real frame, calls (4) → `HitmMatch::AdvanceFrame`
   (already real) → `BuildSpriteDrawData` × 2 (already real, never
   called from the match path) → (2) → (1)'s render call → `PollEvents`.
   Needs a real dt/tick-rate decision (C.2.1) — the natural,
   evidence-backed choice is locking to hitm-engine's own implicit 60fps
   frame convention, matching Module 5A's existing "1 call ==
   1 simulation frame" model exactly, rather than inventing a new one.
6. **Camera wiring** (D) — at minimum, feed the item-3 bridge's fighter
   positions into a per-frame `Camera` update inside the loop from (5);
   the real `cameras.json` `fight` profile (D.3) is the evidenced
   algorithm to port when that work is authorized, but even a static
   or naively-centered camera would make the slice visually functional
   before that refinement.

**None of items 1-6 require reopening `HitmFighterRuntime`'s,
`HitmMatch`'s, `HitmMoveInstance`'s, or `HitmRushAttack`'s already-closed,
already-proven combat logic.** All of it is additive: new files/functions
that read from the existing, unmodified public surfaces
(`HitmMatch::Snapshot()`, `HitmFighterSnapshot`,
`BuildSpriteDrawData`'s existing signature). This is the same "protect
the 95%, extend the 5%" shape every prior Track H phase has followed.

---

## What this audit found that must NOT be reopened or silently fixed as a side effect of the next milestone

- `HitmFighterRuntime`, `HitmMatch`, `HitmMoveInstance`, `HitmRushAttack`,
  `HitmMeleeHitCheck` — closed, proven, real combat logic. Nothing in
  sections A-E requires changing their public behavior.
- The real, documented Ghost-Dash-always-unblocked gap
  (`HitmFighterRuntime.h`'s "PHASE 4" comment,
  `HitmRushAttack.h`, `HITM_MATCH_REPORT.md`) — explicitly out of scope
  here too, per the request.
- `HitmGameRules`'s deliberate exclusion of `cameras.json`/`vfx.json` —
  this audit read `cameras.json` for evidence (D.3) but imports nothing;
  actually importing it is future, separately-gated work, same as Module
  4's own header already said.
- The generated-`character.json` exclusion (attacker read-engine
  multiplier, combo scaling) — unrelated to this audit, still real, still
  not this milestone's concern.
- `RasterDevice`/`VulkanFrameRenderer`'s existing rectangle-only, no-mesh,
  no-texture behavior for every *non*-HITM caller (existing tests,
  `dominus_gpu_scene_test.cpp`) — any texture/sampler work for item 1
  above must be additive to these renderers' existing, tested paths, not
  a replacement of them (the same discipline the Vulkan offscreen path
  already followed relative to the windowed path: "a genuinely separate
  rendering path... left completely unmodified rather than risk
  regressing its already-proven behavior," `VulkanFrameRenderer.h:33-50`).

---

## Bottom line

The proven claim after Track H Phases 1-4 was "DOMINUS can simulate real
HITM combat." This audit's answer to "what would it take to actually play
it" is: **one real missing capability (texture/sampler support — nothing
upstream can show real pixels without it), three small real bridges
(sprite-data→drawable, fighter-position→visible, input→command), and one
new piece of orchestration (the loop itself, plus a camera update inside
it) — built on top of a simulation core, a windowed present loop, and a
Scene→Frame→pixels pipeline that are all already real and already
proven.** No item on this list requires touching Track H's closed combat
modules, inventing missing HITM data, or building full skeletal FK.
