# HITM Sprite Bridge — Track H Phase 5B

Closes the exact gap `HITM_RENDER_INPUT_LOOP_AUDIT.md` section A.3/A.4
named: "the two pipelines — 'HITM real per-part draw data' and 'DOMINUS
Scene→Frame→pixels' — have never been connected, in either direction, for
any fighter." This report covers only that connection. The rendering
capability it connects into (real texture/UV/atlas/alpha support in
`RasterDevice`) was built and verified in Phase 5A
(`HITM_TEXTURE_CAPABILITY` section of `GRAPHICS/README.md`) and is
unmodified by this phase. The combat/animation data this phase reads
(`HitmFighterRuntime`, `HitmSpriteDrawData`) are unmodified, already-closed
Track H modules.

## The chain, end to end

```
HitmFighterRuntime            (Module 5A, real fighter state — unmodified)
      |  .Snapshot()
HitmSpriteDrawData::BuildSpriteDrawData   (Module 5B Phase 1 — unmodified)
      |  produces real HitmPartDraw[]  (atlas rect, placement, sampled pose)
HitmSceneBridge::BuildHitmSceneEntities   (THIS PHASE — new)
      |  produces real GRAPHICS::SceneEntity[]
FrameCompiler::Compile        (extended, additively, this phase)
      |  produces a real GRAPHICS::Frame carrying real DrawCommand[] + TextureAtlas[]
RasterDevice::Rasterize       (Phase 5A — unmodified)
      |
actual HITM pixels
```

## What was built

**`CHARACTER/HitmBridge/HitmSceneBridge.h/.cpp`** (new). One pure
function:

```cpp
core::Result<std::vector<graphics::SceneEntity>> BuildHitmSceneEntities(
    const std::string& fighterId,
    const HitmFighterSnapshot& snapshot,
    const HitmSpriteDrawData& drawData,
    double displayHeight);
```

No animation math, no clip/frame selection, no secondary-motion spring
integration happens here — all of that is `HitmSpriteDrawData`'s own,
already-closed, already-tested job. This file's only job is arithmetic:
turn each already-computed real `HitmPartDraw` into a real, positioned,
textured `SceneEntity`.

**`GRAPHICS/Renderer/Scene.h`**: `SceneEntity` gained the same additive
`textured`/`atlas_id`/`atlas_src_x/y/w/h` fields `DrawCommand` already had
from Phase 5A — default `false`/empty/`0`, zero behavior change for every
pre-existing `SceneEntity` (`SceneFromEntities`'s own output included).

**`GRAPHICS/Renderer/FrameCompiler.h`**: `Compile()` now copies the new
fields straight from `SceneEntity` to `DrawCommand`, and gained an
optional fourth parameter, `std::vector<TextureAtlas> atlases = {}`,
copied into the resulting `Frame`. Every pre-existing three-argument call
site is unaffected.

## The placement formula, and why it is not `rig_render.py`'s literal arithmetic

hitm-engine's own real, working offline verification tool
(`tools/rig_render.py`, already cited as the canonical placement reference
by `HitmRigPlacement.h` and `HitmSpriteDrawData.h`) composes a part by
pasting it at `(rect_x0 * canvasWidth, rect_y0 * canvasHeight)` as a
top-left corner, where `canvasHeight = H` (an arbitrary debug default,
520) and `canvasWidth = H * 0.62`, and scales each part's pixel size by a
further, undocumented `* 0.62` beyond `normW/normH * H`. The script's own
root-bone placement uses a THIRD, different, equally uncited constant
(`0.5*H*0.42`). None of `0.62`/`0.42` is evidenced anywhere in real
authored data — `game.json` (`HitmGameRules`) has exactly one real,
authored screen-scale value, `sprite.displayHeight`, a single scalar, not
a width/height pair — and the constants are mutually inconsistent within
the script's own arithmetic. This is real, direct evidence that they are
debug-tool convenience fudges, not meaningful design values worth
reverse-engineering byte for byte.

`HitmSceneBridge` therefore does not replicate that arithmetic. It uses
the one real, authored, evidenced scale reference —
`HitmGameRules::Sprite().display_height` — uniformly for both axes. This
is not a guess: `HitmAnimationSet.h`'s own header comment already
documents that `anim.json`'s real `offsetX`/`offsetY` keyframe values are
"in the same normalized-to-display-height units as `HitmPartsRig`'s
`normW`/`normH`" — the real data itself already treats X and Y as sharing
one scale reference. Everything else is a real, direct pass-through:
`place_x`/`place_y` (`rig.json`'s real rect corner), `norm_w`/`norm_h`
(`parts.json`'s real normalized size), `pose_rotation_deg`/
`pose_offset_x/y` (`anim.json`'s real sampled pose, including real
secondary motion when the caller supplied it), and `frame_x/y/w/h`
(`parts.json`'s real atlas pixel rect, carried byte for byte into
`atlas_src_x/y/w/h` — exactly as Phase 5A's own `Frame.h` header comment
anticipated this bridge would do).

**Anchoring.** The fighter's own real `(x, y)` (`HitmFighterSnapshot`,
Module 5A) is HITM's real screen-pixel-space convention — evidenced
directly in `HitmFighterRuntime.cpp` (`spatial->y >= phys.ground` clamps
the fighter to the real, authored `ground` value from below, i.e. Y
increases DOWNWARD, PNG/`rig_render.py`'s own convention). A part's
normalized local position `(place_x + pose_offset_x, place_y +
pose_offset_y)` is anchored so `(0.5, 1.0)` — character horizontal
center, character base, matching `rig_render.py`'s own root-bone
placement near the bottom-center of its canvas — lands exactly on the
fighter's own `(x, y)`. Converted into DOMINUS's own real world
convention (Y-UP, the same convention every other `Camera`/`SceneEntity`
in this engine already uses) by negating Y once — the same axis flip
`RasterDevice` already performs on every entity it draws, made explicit
here instead of left implicit.

**Rotation sign.** `pose_rotation_deg` is real `anim.json` data, authored
in the convention `rig_render.py`'s own `piece.rotate(-rot, ...)` call
expects (PIL's `rotate()` is a real, standard, visually-CCW-positive
convention). DOMINUS's own `MeshTransform.h` applies `rotation_deg` as a
standard math-CCW rotation in Y-UP world space; composed with this file's
own Y-negation (a reflection, which reverses rotational handedness once),
the derived result is `transform.rotation_deg = -pose_rotation_deg`.

**Pivot.** `pivot_x`/`pivot_y` (real, authored, carried through by
`HitmPartDraw`) is not consumed by this placement convention — matching
`rig_render.py`'s own real, working reference implementation, which also
does not use it for composition. Real data, not discarded, simply not
load-bearing for this convention, same as the real reference tool.

## Verification

New file: `tests/integration/test_hitm_scene_bridge.cpp`, 6 tests:

1. `HitmSceneBridge_RealBrooklynAtlas_DecodesToNonTrivialRealPixelData` —
   the real `brooklyn_atlas.png` fixture decodes to real, non-uniform
   RGBA data with both real transparent and real opaque regions present
   (alpha genuinely preserved, not a fixture assumption).
2. `HitmSceneBridge_Brooklyn_PopulatesRealTextureFieldsFromRealHitmPartDraw`
   — one `SceneEntity` per real drawn part; `hat`'s `atlas_src_x/y/w/h`
   match its `HitmPartDraw` exactly; real frame dimensions (103×51)
   preserved.
3. `HitmSceneBridge_Break_ZeroDisplayHeight_Fails` /
   `HitmSceneBridge_Break_EmptyFighterId_Fails` — deliberate-break
   coverage; `displayHeight` is required, never silently defaulted.
4. `HitmSceneBridge_RealSecondaryMotion_ChangesTheEntityTransformItProduces`
   — `hat` (a real Brooklyn follow bone with no idle track of its own)
   stays at the real zero-pose fallback with `secondaryMotion=nullptr`
   across 30 real frames, but accumulates a real nonzero spring angle
   with it supplied — and that real difference survives, unchanged, into
   the bridged entity's `world_transform.rotation_deg`, matching the
   bridge's own documented `-pose_rotation_deg` formula exactly (checked
   to `1e-6`).
5. **`HitmSceneBridge_Brooklyn_RasterDeviceProducesActualRealAtlasPixelsForHat`**
   — the proof this phase was asked for. Real idle, real frame 0, no
   secondary motion: `hat` has no idle track (verified against the real
   `anim.json`), so its real pose is the zero-pose fallback — the one
   case predictable without reimplementing rotated nearest-neighbor
   sampling as a second, independent renderer. Builds the real entity,
   compiles a real `Frame` with the real decoded atlas attached, rasterizes
   it, then for a 3×3 grid of sample points independently recomputes —
   from the bridge's own real transform and RasterDevice's own real
   pixel-center sampling convention — exactly which real atlas pixel each
   destination screen pixel should show, decodes that pixel directly from
   the real committed atlas fixture, and asserts byte-for-byte RGBA
   equality against what `RasterDevice` actually wrote. Not "something
   rendered" — the literal real HITM pixel content.

All 905 tests pass (was 899 before this phase). Clean build, zero
warnings (`-Wall -Wextra -Wpedantic`). Clean Release rebuild. Clean
AddressSanitizer+UndefinedBehaviorSanitizer, 2 runs, zero findings. The
existing `hitm-match` live CLI demo re-verified byte-identical under ASan
(unmodified by this phase). Fresh-clone build + full suite verified
before push.

### A real bug this phase's own test caught and fixed — in the test, not the production code

The first version of test 5 compared a nominally-chosen sample fraction
(e.g. `fracX=0.5`) against a pixel selected by `floor()`-ing that same
fraction into a pixel index, then re-used the *nominal* fraction (not the
selected pixel's own center) to compute the expected atlas source
coordinate. `RasterDevice` samples at the pixel's actual center
(`px+0.5`), which is a slightly different fraction than the nominal one —
enough, for a 51-pixel-tall real atlas region scaled to 24 screen pixels,
to select the wrong source row about half the time. Fixed by recomputing
the real fraction from the actually-selected pixel's own center before
computing the expected atlas coordinate — the same convention
`RasterDevice` itself uses. `RasterDevice`'s own sampling code was never
in question; this was purely a test-side precision bug, caught by the
test's own strict byte-for-byte assertion rather than a looser
"approximately right" check, exactly the kind of bug a strict test is
supposed to surface.

## What this phase explicitly does not do

- No animation math, clip/frame selection, or secondary-motion spring
  integration — `HitmSpriteDrawData` still owns all of that.
- No Vulkan/GPU work — unchanged from Phase 5A's own disclosed scope
  boundary (no Vulkan SDK/GPU in this sandbox).
- No camera framing, no arena-boundary visualization — Track H Phase 5E,
  not this one. The test's own camera is a fixed, entity-centered
  isolation choice, explicitly not a claim about final game-viewport
  positioning.
- No input, no application loop — Track H Phases 5C/5D.
- No new CLI command — this phase adds a library function and tests, not
  a demo; a live "render Brooklyn" CLI path is a natural candidate for a
  later phase once camera/viewport conventions exist to make its output
  meaningful.
- No touching of `FillTriangle`, the combat runtime, Ghost Dash, or any
  already-closed Track H combat module.

## Next checkpoint

Track H Phase 5C — input: keyboard/gamepad sampling around the existing
`HitmInputCommand` → `AdvanceFrame` seam, one player first.
