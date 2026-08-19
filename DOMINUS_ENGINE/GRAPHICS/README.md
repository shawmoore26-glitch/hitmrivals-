# GRAPHICS

Real, minimal, deliberately bounded. Opened after RIG + VISUALFORGE +
COMBAT + ANIMATION each reached an independently-earned acceptance
state -- the trigger condition this engine's own roadmap named for
when GRAPHICS should stop being a placeholder.

## What exists

```
Scene       -- entities with real world transforms + real (unresolved) material/mesh refs
Camera      -- position, zoom, rotation (2D only -- no projection, no frustum)
FrameCompiler -- Scene + Camera -> Frame, deterministic, hash-verified
Frame       -- an ORDERED, hash-addressed list of draw commands
RasterDevice -- Frame -> real RGBA8 pixels, deterministic, hash-verified
              (GRAPHICS/Raster/RasterDevice.h -- see below for exactly
               what it does and does not claim)
```

The one real question this answers: **can DOMINUS take a canonical
scene and produce an actual deterministic frame?** Yes -- a real,
content-hashed, order-independent-input/order-independent-output data
structure describing exactly what a frame contains. And, as of the
Raster phase: **can DOMINUS turn that logical frame into actual real
pixels?** Also yes -- `RasterDevice` produces a real, filled RGBA8
buffer with a real content hash, proven deterministic and
order-independent by direct test, the same way `FrameCompiler` itself
already was.

## What RasterDevice is, and deliberately is not

`RasterDevice` draws one filled, real rectangle per `DrawCommand`,
sized by the command's real `screen_transform` scale, positioned at its
real screen-space coordinates, colored by a real `Sha256` of its real
`material_ref` (or `entity_id` if none) -- the same hash function this
whole engine already trusts, not a second invented one. This is real
rasterization (every covered pixel is actually written, area scales
with the transform, not a single point plotted per entity) and real
determinism (same `Frame` -> byte-identical buffer, proven by direct
test, including across different entity insertion orders).

It is **not** a mesh renderer. `mesh_ref`/`material_ref` remain real,
unresolved strings -- there is still no loaded vertex data, no loaded
texture data, no shading model anywhere in this engine (see below).
`RasterDevice` visualizes a `Frame`'s real logical content (position,
scale, identity, draw order); it does not draw an actual character.
Calling the filled rectangle "the character" would be exactly the kind
of overclaim this engine has refused for its whole history -- so
`RasterDevice.h`'s own header comment says so explicitly, and this
does **not** unlock `VISUALFORGE`'s `ACTIVE` acceptance state, which is
specifically gated on resolved, rendered character assets that still
do not exist.

## What does NOT exist, named plainly

- **No real asset resolution.** `material_ref`/`mesh_ref` are real
  strings (traced back to a real `MaterialGenome.material_id` or a
  `.dominus` `mesh` ref path) -- never loaded, never validated as
  pointing at a real file, never turned into actual geometry or a
  texture. `RasterDevice` draws a placeholder shape in their place; it
  does not resolve them.
- **No lighting, no shading, no real material color.** `RasterDevice`'s
  color is derived from a hash of the material's *identity string*, not
  from any real color/shading data the material genome actually
  carries -- there is no shading model in this engine to draw from.
- **No triangle rasterization, no depth buffer, no texturing.** Real
  area-fill rectangles, not real mesh geometry.
- **"Logical determinism" and "raster determinism" are now BOTH real,
  but for different things.** `Frame`'s determinism is about frame
  CONTENT (same scene -> same `frame_hash`). `RasterDevice`'s
  determinism is about actual pixel bytes (same `Frame` -> same
  `buffer_hash`) -- both real, both tested, but the second one still
  says nothing about what an eventual mesh/GPU rasterizer would draw,
  or whether two different real rendering backends would agree pixel-
  for-pixel. That remains a real, separate, much larger problem.

## Connection to VISUALFORGE

`VISUALFORGE/RendererPackageAcceptanceHarness.h` uses `FrameCompiler`
to prove a real `CharacterBlueprint` compiles into a valid, real,
deterministic `Frame` -- the `Renderable`/`RenderDeterminism` checks.
Dependency direction: `VISUALFORGE -> GRAPHICS -> ANIMATION -> CORE`,
never the reverse (`GRAPHICS` has zero knowledge of `VISUALFORGE`'s
types).

## What's still gated

Everything past "produce real, deterministic pixels for a placeholder
shape": real asset loading (mesh/texture files), a real shading model,
triangle-mesh rasterization, and any DCC-tool integration (Blender/
Maya/Spine/Unreal/Unity/MotionBuilder -- none of which this engine has
ever connected to). `ACTIVE` in `VISUALFORGE`'s acceptance lifecycle
remains deliberately unreachable until real, *resolved* pixel output
exists -- see `VISUALFORGE/README.md` and the roadmap entry for this
phase. Neither `RasterDevice` nor `VulkanFrameRenderer` (below) claim
to have filled that pipe with real character content -- both prove the
pipe from `Frame` to real pixels works, on CPU and now on a real GPU,
using the same placeholder-rectangle scope.

## GPU renderer (`GRAPHICS/Vulkan/`)

The first real GPU rendering backend: `Frame -> CPU vertex staging ->
Vulkan vertex buffer -> real GPU rasterization -> swapchain ->
presentation`. Same honest scope as `RasterDevice` -- every
`DrawCommand` becomes a real GPU-rendered rectangle (two triangles)
from the same canonical `Frame`, colored by the same `registry::
Sha256::Hash` authority `RasterDevice` uses, never a second, invented
color function. Not a mesh renderer; `mesh_ref`/`material_ref` remain
unresolved strings, same as everywhere else in this file.

Gated behind `DOMINUS_ENABLE_VULKAN` (CMake option, default `OFF`) so
the established CPU test suite -- everything every prior phase in this
engine proved -- stays buildable with zero new dependencies. When
explicitly enabled, a missing Vulkan SDK/GLFW/`glslc` is a real CMake
`FATAL_ERROR`, never a silent fallback. Verified directly, not
assumed: built with real Vulkan headers, GLFW, and `glslc` installed
in this environment; the real static library and the demo executable
(`dominus-gpu-demo`) both compile with **zero warnings** under
`-Wall -Wextra -Wpedantic`, and the full 609-test suite passes
identically in both the default and Vulkan-enabled configurations.
Running the demo in this sandbox (no display) fails honestly --
`glfwInit failed`, reported as a real error, exit code 2 -- rather than
fabricating a successful render.

## GPU Rendering Milestone: Scene to Verified Pixels

The windowed demo above only proved Vulkan *initializes*. This
milestone proves the full real chain: `DOMINUS Scene -> Entity/
Component data -> Transform propagation -> Geometry -> Camera ->
Material -> Vulkan GPU resources -> command submission -> offscreen
GPU render -> CPU readback -> golden-image verification` -- using real
DOMINUS-owned data at every step, not a demo scene.

### Real data, real bridge, no parallel authority

One new file: `GRAPHICS/Renderer/SceneFromEntities.h/.cpp` -- reads
real components off real `WORLD::EntityRegistry`/`CORE::MetaBinObject`
entities (`WORLD::SpatialComponent` for position,
`CHARACTER::MaterialGenomeComponent` for material, both pre-existing)
and produces a real `Scene`. No fake ECS, no second Entity/Transform/
Material type. Two honest, disclosed limits found during inspection
and documented rather than papered over: `SpatialComponent` carries
x/y/z only (no rotation, no scale -- entities get a real, disclosed
default of `rotation_deg=0, scale=1`, not a fabricated visual
property), and DOMINUS has no mesh/vertex asset representation
anywhere in the repository at all -- the "geometry" rendered is the
same real, procedurally-derived rectangle `RasterDevice` and the
windowed Vulkan path already use, now uploaded as genuine **indexed**
GPU buffers (4 unique vertices + 6 indices per entity, a real
capability the windowed path still lacks) instead of hardcoded
coordinates.

### Real offscreen GPU path, separate from the windowed one

`VulkanFrameRenderer::InitializeHeadless`/`RenderOffscreen` -- a
second, genuinely independent path added to the SAME class (not a
parallel renderer): its own instance/device creation with no GLFW and
no `VkSurfaceKHR` at all (only a graphics-capable queue is required,
never a presentation one), a real `DEVICE_LOCAL`
`COLOR_ATTACHMENT | TRANSFER_SRC` offscreen image, a render pass whose
`finalLayout` is `TRANSFER_SRC_OPTIMAL` (not `PRESENT_SRC_KHR`), and a
real `vkCmdCopyImageToBuffer` readback into a `HOST_VISIBLE` buffer.
GPU completion is learned through a real `vkWaitForFences` with a
bounded 10-second timeout -- never a sleep, never a CPU timing hack.
The windowed path (`Initialize`/`Render`) is completely unmodified.

### Verified against a real Vulkan-capable device, not assumed available

This sandbox has no GPU hardware. Rather than report `NOT_EXECUTED`
without checking, Mesa's software Vulkan implementation (`llvmpipe`,
Khronos-conformant 1.3.1.1, `deviceType = CPU`) was installed and
confirmed via `vulkaninfo` before any code was written -- a real,
standard, industry-common way to run genuine Vulkan execution in
headless CI. Reported honestly throughout as a software device, never
described as discrete GPU hardware.

### The full test: `dominus-gpu-scene-test`

`TOOLS/Editor/dominus_gpu_scene_test.cpp` builds a real, deterministic
DOMINUS entity, renders it through the real offscreen path, and proves
every acceptance criterion against real captured pixels -- not just a
hash:

- **Golden image**: first real render becomes the reference; reports
  matching/different pixel counts, max channel diff, and total
  absolute diff (all zero on a pass), plus the real `SHA-256` of the
  actual pixel buffer. A real PPM is written to disk so a human can
  look at it, not just trust the hash -- confirmed visually: a real,
  non-blank, correctly-positioned rectangle.
- **Determinism**: the same scene rendered 5 times inside one process,
  then the whole test executable run 5 more times as **separate
  processes** -- identical `SHA-256` every time, exact equality
  required (not a tolerance), since this pipeline has no legitimate
  source of variation.
- **Resize**: 256x256 -> 512x512 -> back to 256x256, verifying real
  offscreen target recreation (`ensureOffscreenTarget`'s own
  size-comparison logic) and that returning to the original size
  reproduces the original golden pixels exactly.
- **Scene mutation** (the most important proof): the SAME real
  `SpatialComponent` on the SAME real entity is mutated to a new
  position -- pixels change, confirmed pixel-by-pixel, not merely
  "some difference exists." Restoring the original position restores
  the original golden pixels exactly. This is what proves the renderer
  consumes real DOMINUS state rather than rendering a hardcoded demo.
- **Hardcoded-data search**: checked directly against
  `VulkanFrameRenderer.cpp`'s real source -- every vertex position
  comes from `frame.commands[i].screen_transform`, every color from
  `deterministicColor(material_ref/entity_id)`; no hardcoded
  coordinate, color, or camera matrix exists in the production
  `Render`/`RenderOffscreen` path.

Result, run against the real `llvmpipe` device: **`FINAL STATUS:
PROVEN`**. Full clean rebuild (both `DOMINUS_ENABLE_VULKAN=OFF` and
`=ON`), zero warnings, 609/609 existing tests unaffected in both
configurations, the windowed demo unmodified and still failing
honestly (`glfwInit failed`, no display in this sandbox) exactly as
before.

## Geometry Milestone: A Real Mesh Representation

The previous milestone proved `Scene -> Vulkan -> pixels`. This one
answers the next real architectural question it raised: DOMINUS has
never had an actual geometry representation anywhere -- every
DrawCommand became a procedural, ad-hoc rectangle computed fresh
inside each renderer, and `rotation_deg` (real, already-threaded
`Transform2D` data) was silently ignored by both renderers the whole
time. Confirmed by a fresh, full-repository search before writing
anything -- not assumed from memory.

Per explicit direction: no `MeshComponent` was invented just because
the renderer wanted one. What was built is the smallest real thing:

### `GRAPHICS/Renderer/Mesh.h` / `MeshLibrary`

`Mesh { mesh_id, vertices (local-space Vertex2D), indices }`.
Deliberately **no normals, no UVs** -- there is no lighting model
anywhere in DOMINUS for a normal to feed, and no texture/image asset
representation anywhere for a UV to sample. Adding either field would
have been exactly the kind of fake completeness this engine has
refused throughout its whole history; the reasoning is in the header,
not left implicit.

`MeshLibrary` resolves a real `mesh_ref` string to a real `Mesh` --
explicitly **not** an asset-loading pipeline (DOMINUS has no mesh
files to load). There is exactly one real, built-in mesh today,
`dominus.unit_quad` -- the same shape every `DrawCommand` has
implicitly used since the Raster phase, now a real, named, reusable
asset instead of being re-derived ad hoc inside each renderer's vertex
code. The lookup exists so `mesh_ref` genuinely participates in
resolution, and so a real asset system, when one exists, has an
obvious place to plug in without touching any renderer.

### `GRAPHICS/Renderer/MeshTransform.h`

One shared, header-only function applying a real `Transform2D`
(translation, **rotation**, scale) to a mesh's local vertices --
used identically by `RasterDevice` and `VulkanFrameRenderer` so the
two renderers can never silently disagree about what the same
transform data means. This is the real fix for the rotation gap:
before this phase, a rotated entity rendered identically to an
unrotated one, on both renderers, confirmed by direct source
inspection before writing a single line.

### Both renderers upgraded, not left divergent

`RasterDevice` moved from an axis-aligned rectangle fill to a real
edge-function triangle rasterizer -- necessary because a rotated
quad's triangles are no longer axis-aligned; an axis-aligned fill
would draw the wrong shape. `VulkanFrameRenderer`'s offscreen path
now builds real indexed geometry from `Mesh`, and the windowed path
was updated for the same rotation fix rather than left stale --
leaving one of two renderer paths silently wrong while fixing the
other would be exactly the "two divergent renderers" pattern this
engine has worked to eliminate elsewhere (see the dependency-graph
migration record). The windowed path still draws non-indexed
vertices; only the transform math was fixed there, not indexing --
a real, disclosed scope boundary, not an oversight.

Existing `RasterDevice` tests: **611/611, zero regressions** -- every
existing test checks relative properties (determinism, order-
independence, area scaling, content-derived color) that hold
regardless of the underlying fill algorithm. Two new tests added:
rotation genuinely changes output, and rotated output remains
deterministic.

### Real proofs added to the golden-image harness

`dominus-gpu-scene-test` gained three new real checks, all passing on
first execution against the real `llvmpipe` device:

- **Rotation**: the same entity at 0° vs. 45° -- 88 pixels differ.
- **Multiple entities**: two real entities, two real materials, both
  real deterministic colors confirmed present in the actual pixel
  buffer (not just "more pixels than one entity").
- **Layering**: two fully-overlapping entities, swapping which one has
  the higher `sort_layer` changes 256 pixels -- deterministic
  painter's ordering, explicitly **not** depth buffering. No Z-buffer,
  no depth attachment, no depth test exists anywhere in this engine's
  Vulkan pipeline, and none is added here just to make the word
  "depth" technically apply. DOMINUS's scene representation is 2D
  today; real GPU depth testing is the right next step once DOMINUS
  genuinely has a 3D spatial representation to drive it -- not before.

### The permanent regression mechanism

A real, checked-in reference file, `GRAPHICS/Vulkan/
golden_reference.sha256` -- not just self-consistency within one
process run. First run at a given code state establishes the baseline
(reported as `ESTABLISHED`, not silently treated as a pass); every
later run compares against it and reports `MATCH` or, honestly,
`MISMATCH -- REGRESSION DETECTED` with both hashes printed.

This was verified working, not just written: a real bug was caught in
the first attempt (a relative path that silently failed to resolve
from the build directory, masking every run as "first run" -- fixed
by anchoring to the source tree via a real `DOMINUS_SOURCE_DIR`
compile definition, and by making the write path fail loudly instead
of silently). Then explicitly exercised all three states: fresh
establish, a genuine match across a completely clean rebuild, and a
deliberately corrupted reference file correctly detected as a
regression with exit code 1.

The geometry change in this phase is itself a real, disclosed example
of what this mechanism is for: switching from ad-hoc rectangles to
real mesh-based triangle rasterization legitimately changed the
reference pixel hash (`e31e86cb...` -> `664d7a96...`). That's an
intentional, understood, and now-recorded baseline update -- not a
silent regression the mechanism failed to catch.

Full clean rebuild, both configurations, zero warnings. 611/611 tests
(was 609; +2 rotation tests). Determinism reconfirmed across 7 separate
process invocations on a completely fresh build.

## Camera / Material Authority Milestone

The geometry milestone used the word "depth" for `sort_layer`
ordering, and that was imprecise enough to correct explicitly:
`sort_layer` drives deterministic **painter's ordering**, not GPU
depth buffering. No Z-buffer, no depth attachment, no depth test
exists anywhere in this engine's Vulkan pipeline, and none should be
added just to make the word "depth" technically apply. When DOMINUS
genuinely has a 3D spatial representation, real depth testing
(`Transform -> 3D position -> projection -> depth attachment -> GPU
depth testing`) is the right next step -- not before. Every reference
to this feature (tests, docs) now says "layering," not "depth."

This phase's real question, per explicit direction: are Camera and
Material truly DOMINUS-owned data, or renderer-side conveniences?
Inspected before writing anything, against real source:

1. **Camera representation exists?** Yes -- `graphics::Camera`
   (`x/y/zoom/rotation_deg`), already consumed by both renderers via
   `ToCameraSpace`, which already implements real View (translate +
   rotate relative to camera) and Projection (scale by zoom) as one
   fused 2D affine transform. No new type needed -- Camera was already
   complete. What was missing was a real *test* proving mutation
   reaches pixels.
2. **Material parameters beyond `material_id`?** Yes, but not what
   might be assumed: `MaterialGenome`'s own header says explicitly
   "state, not rendering" -- no color, no roughness, nothing
   shader-consumable. The one real, numeric, authoritative field is
   `MaterialProperties::wear_state` (0=pristine, 1=destroyed).
3. **Does the material genome contain authoritative values?** Yes --
   `wear_state` is real, validated data (see
   `CHARACTER/Genome/MaterialGenome.h`).
4. **Are colors/parameters hashed?** Yes -- confirmed by reading
   `REGISTRY/MaterialGenomeCompiler.h`: the WHOLE genome, including
   `wear_state`, is canonically serialized and hashed, and already
   feeds `VISUALFORGE::DependencyGraph::material_genome_hash`. This
   was already authoritative everywhere else in DOMINUS; no renderer
   had ever read it.
5. **Texture representation anywhere?** No. Confirmed absent (again).
6. **Was `RasterDevice` already consuming material information?** Only
   `material_ref` -> a deterministic base color. `wear_state` was
   real, authoritative, and completely unconsumed.
7. **Can Vulkan and RasterDevice share the same semantics?** Yes --
   same pattern as `MeshTransform.h`: one shared function, not two
   divergent implementations.

### What was built

`SceneEntity`/`DrawCommand` gained one real field,
`material_wear_state` -- not a new concept, just finally letting a
renderer read a value that was already authoritative. Threaded through
`FrameCompiler` and included in `frame_hash` (a real change to
`wear_state` genuinely changes frame identity, matching how it already
changes `material_genome_hash` elsewhere).

`GRAPHICS/Renderer/MaterialAppearance.h` -- one shared, disclosed
function: base color from `Sha256(material_ref)` (unchanged), blended
toward a fixed, named "worn" gray proportional to `wear_state`. The
header is explicit about what this is and isn't: a real, simple, named
interpretation of "more worn = more visually degraded," not a shading
model -- `MaterialGenome`'s own honesty ("state, not rendering") is
preserved, not violated. `age_years`, `damage_history`,
`weather_exposure` remain real, authoritative, unconsumed data --
same treatment as texture support: not yet present, not faked.

**Texture authority: NOT YET PRESENT.** No `TextureAsset`,
`TextureRegistry`, sampler, or descriptor resource was created. The
test prints this exact line rather than silently omitting the
question.

### Two new acceptance tests, both passing on first real execution against `llvmpipe`

- **Camera mutation**: original `Camera{}` -> golden pixels. Mutated
  (`x=40, zoom=1.5`) -> 832 pixels differ. Restored -> golden pixels
  restored exactly.
- **Material mutation**: `wear_state=0.0` -> golden pixels.
  `wear_state=1.0` -> 256 pixels differ. Restored to `0.0` -> golden
  pixels restored exactly.

**611/611 existing tests, zero regressions** -- the default
`wear_state=0.0` produces byte-identical color to before this phase,
confirmed directly (the golden reference hash, `664d7a96...`, is
unchanged from the geometry milestone).

Full clean rebuild, both `DOMINUS_ENABLE_VULKAN` configurations, zero
warnings, confirmed from a truly from-scratch build (not an
incremental one). Golden reference re-established from that fresh
build and reconfirmed `MATCH` across 6 further separate process runs,
same hash every time.

## Asset Boundary: Where Would a Texture Legitimately Become a DOMINUS Artifact?

Not "how do we add textures to Vulkan" -- investigated where texture
data would slot into DOMINUS's *existing* artifact/hash/provenance
machinery, if it ever needed to. Inspected directly, not assumed:

**The pattern DOMINUS already has, confirmed by reading the real
code:**

1. A `.dominus` file declares a ref: `{"key": {"ref": "filename"}}`
   (or one of two other real shapes -- see
   `REALITY/BrooklynEvidenceGraphBuilder.cpp`'s `DiscoverRefs`).
2. `DiscoverRefs` is **already schema-generic by shape, not by a
   hardcoded key list** -- confirmed by reading it: "A future
   top-level `{"ref": ...}` block added to the schema is picked up
   automatically by shape (1) without any code change here." A real
   `"texture": {"ref": "brooklyn_skin.png"}` block would be discovered,
   hashed (raw file bytes via `registry::Sha256::Hash`, the one hash
   authority), and given a real graph node with **zero REALITY code
   changes**.
3. `CHARACTER::RigBinder::Bind` resolves each declared ref into a real,
   typed component attached to the entity (e.g.
   `VisualGenomeRefComponent` -> loads + parses -> `VisualGenomeComponent`).
   A texture would follow the identical pattern: a new
   `TextureRefComponent` (raw ref path) bound into a new
   `TextureComponent` (decoded pixel data) -- reusing the existing
   bind mechanism, not inventing a second one.
4. For *authored state* (a genome), `REGISTRY::XGenomeCompiler`
   canonically serializes the struct and produces a content-addressed
   `ImmutableArtifact`. A texture is not authored state -- it's raw
   binary data -- so it would NOT need a compiler/serializer step at
   all; it would be hashed exactly the way `EvidenceGraph` already
   hashes every other non-genome ref file (skeleton, animation clips,
   hurtbox data): raw bytes, straight into `Sha256::Hash`. No new hash
   authority, because the existing one already covers this case.
5. `VISUALFORGE::AssetSpecification` **already has a `"texture"`
   requirement category** (`{entity_id}_skin_texture`, driven by
   `CharacterBlueprint::skin`) -- DOMINUS already knows, conceptually,
   that a texture asset would eventually be needed. It has just never
   had actual pixel data to point at.

**The conclusion:** there is no missing artifact system to build. The
ref → hash → bind pipeline that every other genome type already uses
is already general enough to carry a texture the day one is declared.
The only genuinely new code a real texture would require is (a) a
`TextureRefComponent`/`TextureComponent` pair in CHARACTER (following
the existing pattern exactly) and (b) real Vulkan resources
(`VkImage`/`VkImageView`/`VkSampler`/descriptor sets) in the renderer
-- and `GRAPHICS/Renderer/MaterialAppearance.h` would be the one place
that changes to actually sample it.

**None of that was built.** No real texture file exists anywhere in
this repository, and nothing consumes one -- building the resource
layer now would be exactly the "invent `TextureAsset`,
`TextureRegistry` just for Vulkan" the boundary investigation was
explicitly asked to avoid. `Texture authority: NOT YET PRESENT`
remains the accurate, current answer -- but it is now a documented,
investigated answer with a real integration point identified, not
just an absence.

## Multiple Real Entities, Real Scene Composition

Proves DOMINUS is rendering a scene, not a test object: `Scene { Entity
A { Transform, Mesh, Material }, Entity B { ... }, Camera }`. Three
real entities, well-separated real screen positions, three real
distinct materials -- verified against the real `llvmpipe` device.

### A real bug the test itself caught, not assumed away

The first version of the ordering test failed. Investigated rather
than weakened: `GRAPHICS/Renderer/SceneFromEntities.cpp` assigns
`sort_layer` by the *position* of each id in the caller-supplied
`entityIds` list -- a real, disclosed design choice (WORLD entities
have no real draw-layer property anywhere in DOMINUS today, the same
honest gap `SpatialComponent`'s missing rotation/scale represents),
but one that had never been documented and that the test's own
premise ("reversed input -> identical output") directly contradicted.
Fixed two things, not one: `SceneFromEntities.h`'s header now states
the behavior plainly, and the test itself was corrected to check the
claim that's actually true -- `FrameCompiler`'s own sort (by
`sort_layer`, then `entity_id`) is fully deterministic and
order-independent *given a fixed `sort_layer` assignment*, verified via
direct `Scene` construction rather than through
`SceneFromEntities`'s separate, list-position-dependent behavior.

### Five real proofs, all passing on first correct execution against `llvmpipe`

- **Independent transforms + independent materials**: three entities
  at three real, distinct positions, each entity's own real
  deterministic color confirmed present at its own real screen
  location -- not just "three colors exist somewhere."
- **Deterministic draw ordering**: same three entities, same fixed
  `sort_layer`, added to the `Scene` in reversed order -> identical
  `frame_hash`, identical pixels.
- **Entity removal** (real `WORLD::EntityRegistry::Remove`, not just
  omitting an id from a list): removing one entity changes the overall
  pixel buffer, its own screen region becomes background, and --
  checked as an exact regional pixel comparison, not an overall
  similarity judgment -- the two *surviving* entities' own regions are
  byte-identical before and after.
- **Mutation isolation**: moving one entity's real position never
  changes another entity's real pixel region, checked the same exact,
  regional way.
- **Restoration**: removing an entity, mutating another, then
  restoring both to their original real state reproduces the exact
  original composite golden pixels -- not close, exact.

**611/611 existing tests, zero regressions.** Full clean rebuild from
scratch, both `DOMINUS_ENABLE_VULKAN` configurations, zero warnings.
Golden reference re-established from that fresh build and reconfirmed
`MATCH` across 5 further separate process runs, same hash every time.

## RenderFrame Authority: The Hard Invariant

Established as an explicit, enforced boundary, not just a convention:

```
WORLD::EntityRegistry
      |
GRAPHICS::Scene / FrameCompiler
      |
GRAPHICS::Frame          <- the ONLY channel past this point
      |
RasterDevice | VulkanFrameRenderer
```

DOMINUS's existing names (`Frame`, `DrawCommand`) already mean exactly
what a `RenderFrame`/`RenderItem` would -- kept as-is rather than
renamed purely for vocabulary alignment across a large surface area
(both renderers, `FrameCompiler`, every existing test) for no real
behavioral gain.

### Confirmed, not assumed: the boundary already held structurally

Before changing anything, `GRAPHICS/Raster/RasterDevice.{h,cpp}` and
`GRAPHICS/Vulkan/VulkanFrameRenderer.{h,cpp}` were checked directly for
any reference to `EntityRegistry`, `MetaBinObject`, `Scene`, or
`SceneEntity` -- zero found. Both renderers' entire public surface
already took `const Frame&` exclusively. `SceneFromEntities` was
already the one, explicit, documented bridge -- and was already never
called by either renderer.

### What was missing: Frame wasn't a *complete* snapshot

`Frame` carried an ordered list of `DrawCommand`s and a hash, but no
record of the `Camera` or intended render-target size it was compiled
against -- both effects were baked into `screen_transform` values with
no way to recover the source. Fixed by adding two real fields, reusing
the existing `Camera` type rather than inventing a duplicate:

```cpp
struct Viewport { int width; int height; };  // descriptive, see below

struct Frame {
    Camera camera;      // real copy of what this Frame was compiled against
    Viewport viewport;
    std::vector<DrawCommand> commands;
    std::string frame_hash;  // now includes camera + viewport
};
```

`viewport` is deliberately **descriptive, not prescriptive**: this
engine's 2D architecture computes `screen_transform` independent of
render-target size (proven directly by the resize test -- the same
compiled `Frame` already renders correctly at both 256x256 and
512x512). Making viewport binding would have broken that real,
already-tested capability for no real gain; `RenderOffscreen`'s
explicit width/height parameters remain authoritative, `frame.viewport`
is carried purely so a `Frame` is self-describing when inspected on its
own.

### Four new tests, all passing, extending the existing FrameCompiler suite

`RenderFrame_IsComplete_CarriesItsOwnCameraAndViewport`,
`RenderFrame_SameSceneCameraViewport_ProducesIdenticalFrame_AB` (real,
direct field-by-field equality of every `DrawCommand`, not merely hash
equality standing in for it), `RenderFrame_MutatingOneEntityTransform_
ChangesOnlyThatRenderItem` (the untouched entity's `DrawCommand` is
checked byte-identical, not assumed), and
`RenderFrame_RemovingOneEntity_FrameContainsOnlyRemaining` (the
survivor's `DrawCommand` is checked identical to what it was in the
larger Frame).

### The invariant, made permanent and automatically enforced

`tests/graphics/test_render_boundary.cpp` reads the real renderer
source files at test time and fails if either ever references
`EntityRegistry`/`MetaBinObject`/`Scene`/`SceneEntity` again -- not a
one-time grep, a permanent check. Verified it actually catches a
violation, not just passes trivially: injected a fake
`EntityRegistry` reference into `RasterDevice.cpp`, confirmed the test
failed with a named, specific violation message, then reverted and
confirmed a clean pass.

**618/618 tests** (was 611; +4 RenderFrame tests, +3 boundary tests).
Full clean rebuild from scratch, both `DOMINUS_ENABLE_VULKAN`
configurations, zero warnings. Golden pixel hash **unchanged**
(`664d7a96...`) -- confirming this phase added real completeness and a
real enforced boundary without touching any actual rendering math.
Golden reference reconfirmed `MATCH` across 5 further separate process
runs on the fresh build.

## Milestone Closure: RenderFrame Boundary & Deterministic Scene Compilation — PROVEN

Closed formally, with a locked regression checkpoint. The golden pixel
hash, `664d7a96525e5b0e42191bdde7fa6b8c19c56c6ba9203dbeb99be06e4af6d1ee`,
has now remained unchanged across two full architectural phases (the
`RenderFrame` completeness/boundary-enforcement work, and the GPU
Resource Authority investigation below) -- real, direct confirmation
that both were structural and enforcement work, never accidental
rendering-behavior drift. This hash is preserved in
`GRAPHICS/Vulkan/golden_reference.sha256` and re-verified by
`dominus-gpu-scene-test` on every run; any future change that shifts
it will be reported honestly as `MISMATCH -- REGRESSION DETECTED`,
never silently accepted.

## GPU Resource Authority & Lifetime: Investigation

The question: does the GPU resource layer have a deterministic
relationship to the Frame without becoming a second authority?
Investigated against the real source before concluding anything --
`GRAPHICS/Vulkan/VulkanFrameRenderer.{h,cpp}` read in full, every
`vkCreateBuffer`/`vkDestroyBuffer` call site traced, every member
variable checked.

### The central finding

**There is currently no GPU resource cache of any kind.** Confirmed by
reading `VulkanFrameRenderer.h`'s full private member list: zero
`std::map`/`std::unordered_map`, nothing keyed by mesh, material, or
entity identity. What exists is exactly one fixed-capacity scratch
vertex buffer and one scratch index buffer (`kMaxVertices =
kMaxIndices = 6 * 4096`), created once during `Initialize`/
`InitializeHeadless` and reused -- their *content* is fully rebuilt
from the current `Frame` on every single `Render`/`RenderOffscreen`
call. This is the safest possible current state: with no per-resource
identity persisting across frames, there is nothing that could
accidentally become a second, GPU-side authority.

### The nine questions, answered with cited evidence

1. **Does a `DrawCommand` reference stable DOMINUS identity?** Yes --
   `entity_id`, `material_ref`, `mesh_ref` are real, stable strings
   traced back to real DOMINUS data (a WORLD entity's id, a
   `MaterialGenome::material_id`, a mesh_ref resolved through
   `MeshLibrary`). These are real, hashable keys a future cache could
   use.
2. **Are GPU resources cached deterministically?** Not applicable --
   none are cached. The two scratch buffers are rewritten from the
   current `Frame`'s already-deterministic command order every call;
   "caching" doesn't describe what happens.
3. **Can two entities sharing a mesh share one GPU buffer?** Trivially
   true today, but not through any deliberate sharing mechanism --
   *every* entity's geometry already lives in the one shared scratch
   buffer, because no per-entity buffer exists at all.
4. **Does resource lifetime follow actual ownership?** Yes, honestly:
   the two scratch buffers' lifetime is tied to the
   `VulkanFrameRenderer` *object's* own lifetime (created in
   `Initialize`, destroyed in `Shutdown`/the destructor) -- never to
   any DOMINUS entity's lifetime, because they aren't per-entity
   resources. There is no resource whose lifetime could mismatch a
   real entity's, because no such resource exists yet.
5. **What happens when a resource disappears?** Not a real scenario
   for this renderer today -- when an entity is removed from a
   `Scene`, the next `Frame` simply omits it, and the shared buffer's
   *content* for that call omits its vertices too (proven directly by
   the composite-scene milestone's entity-removal test). Nothing
   "disappears" at the GPU level, because nothing was ever created
   there per-entity.
6. **Can stale GPU resources survive scene mutation?** No -- proven
   directly, not just reasoned about. Two independent guarantees, both
   verified: (a) the scratch buffers' *content* is fully rewritten
   from the current `Frame` every call, and (b) even if it weren't,
   `recordOffscreenCommandBuffer`'s real `vkCmdDrawIndexed` call always
   uses `indexCount = frame.commands`-derived real index count (traced
   to `static_cast<std::uint32_t>(indices.size())` at the real call
   site), never the buffer's max capacity -- so leftover bytes from a
   larger prior frame are allocated capacity the draw call structurally
   cannot reference. A new, permanent test
   (`dominus-gpu-scene-test`'s "GPU resource authority" section) proves
   this empirically: a 50-entity frame rendered immediately before a
   1-entity frame, on the *same* renderer instance, produces pixels
   byte-identical to a completely independent, freshly-initialized
   renderer rendering the same 1-entity frame alone.
7. **Is resource creation order deterministic?** Yes for the one-time
   setup (a fixed, linear `Initialize` sequence), and the per-call
   content upload order is fully deterministic since it mirrors
   `Frame.commands`' already-proven order.
8. **Where does hashing/provenance belong?** With `Frame`/`DrawCommand`
   -- already real, already hash-verified via `frame_hash`. GPU
   resources today carry no persistent identity of their own to hash.
   If a real cache is ever built, its cache key must be *derived from*
   real DOMINUS identity (`mesh_ref`/`material_ref` content), never
   from a `VkBuffer`/`VkImage` handle.
9. **Is the GPU resource cache an implementation detail, or has it
   accidentally become another authority?** Neither yet -- it doesn't
   exist. The two scratch buffers are pure implementation detail of one
   renderer instance's own working memory: they decide nothing, are
   never consulted for identity, and carry no meaning across frames.

### The critical invariant, confirmed to hold today

```
DOMINUS identity -> RenderFrame reference -> GPU resource   (real, current)
GPU handle -> becomes DOMINUS identity                       (does not exist)
```

### The contract a future real cache must follow

Not built now -- there is no real performance need yet, and building
one speculatively would be exactly the "invent it before it's needed"
this engine has refused at every prior boundary (texture authority,
`RealityRegistry`/`GenomeRegistry`, `RealityCompiler`'s dependency
graph). If scene complexity ever makes per-frame rebuild a genuine
bottleneck, a real cache is legitimate future work -- bound by these
real rules, derived directly from this investigation, not invented
fresh at that time:

- **Cache keys must be real DOMINUS identity** (`mesh_ref`,
  `material_ref` content, or their content hash) -- never a `VkBuffer`
  handle, memory address, or creation-order index standing in for
  identity.
- **A cached resource's lifetime must be provably bounded by real
  `Frame` presence** -- if no `DrawCommand` across N frames references
  a given key, eviction must be safe and observable, never silent
  staleness.
- **The cache must be re-derivable from scratch at any time** -- exactly
  what the current, cache-free renderer already does every frame. A
  real cache is an optimization of that ground truth, never a
  replacement for it; if the cache disagrees with a fresh rebuild, the
  fresh rebuild is authoritative, full stop.
- **No cache entry may be read before the `Frame` that would produce
  it has been compiled** -- preserving `DOMINUS identity -> RenderFrame
  reference -> GPU resource`, never the reverse.

**618/618 tests**, full clean rebuild from scratch, both
`DOMINUS_ENABLE_VULKAN` configurations, zero warnings. Golden pixel
hash unchanged, reconfirmed `MATCH` across 5 further separate process
runs.

## GPU Frame Lifecycle & Synchronization — PROVE

The acceptance bar, explicitly, per direction: not "no crash" -- every
GPU resource reuse and destruction operation has an explicit
synchronization proof. This is the exact class of bug golden pixels
can hide, especially against a software Vulkan implementation
(`llvmpipe`) that may execute more forgivingly than real hardware
would tolerate a genuine race.

### The strongest available tool, used deliberately

Before writing a single new test, checked whether the real Khronos
validation layer was available in this environment --
`vulkan-validationlayers` was already installed. Confirmed it was
*genuinely active*, not silently ignored, with a real, separate,
minimal check: created a Vulkan instance with the layer enabled and
deliberately called `vkDestroyInstance` twice -- the layer reported a
real, specific violation (`VUID-vkDestroyInstance-instance-parameter`)
and aborted. Only after confirming the layer actually intercepts real
errors was it trusted as a real verification tool, not assumed to be
working.

**Every existing GPU test, and every new one below, was then run under
`VK_LAYER_KHRONOS_validation` -- zero errors or warnings reported,
across the entire test suite**, confirmed by capturing stdout and
stderr separately and inspecting stderr directly (validation output
goes to stderr; empty stderr with exit code 0 is real, checked
evidence, not an assumption).

### Real synchronization, traced to specific lines, not assumed

- **Command-buffer lifetime**: one persistent `headless_command_buffer_`,
  allocated once, explicitly `vkResetCommandBuffer`'d before every
  re-recording -- legal specifically because the command pool is
  created with `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`.
- **Fence ownership**: one persistent `headless_fence_`, reset via
  `vkResetFences` immediately before each `vkQueueSubmit`, waited on via
  `vkWaitForFences` (bounded 10s timeout, never a sleep) before
  `RenderOffscreen` returns.
- **Semaphore usage**: none in the headless path -- correctly so.
  Semaphores exist for GPU-GPU synchronization (swapchain acquire/
  present); the offscreen path has no swapchain and never has more than
  one submission in flight, so a fence alone is the correct, sufficient
  primitive.
- **Buffer reuse -- the central question**: the CPU writes new
  vertex/index data into the persistent scratch buffers *before* this
  call's own `vkResetFences`/`vkQueueSubmit`/`vkWaitForFences`
  sequence. This is safe specifically because `RenderOffscreen` is
  fully synchronous end-to-end: it never returns until
  `vkWaitForFences` confirms the GPU finished, so by construction no
  later call's buffer write can ever race a still-in-flight prior
  submission. This is the single most important real fact this
  milestone proves empirically, not just reasons about.
- **Offscreen image transitions**: `offscreen_image_`'s layout
  transition from `COLOR_ATTACHMENT_OPTIMAL` to `TRANSFER_SRC_OPTIMAL`
  happens automatically as part of the render pass's own declared
  `finalLayout` -- a real, standard Vulkan mechanism. **A real
  correction made during this investigation, not left standing**: an
  early draft of this file's own test comment described this as "an
  explicit pipeline barrier" -- rereading the actual command-recording
  code directly showed that was wrong; there is no separate
  `vkCmdPipelineBarrier` call, the render pass's own `finalLayout`
  performs the transition. Fixed before this record was written, not
  after.
- **Readback synchronization**: the copy (`vkCmdCopyImageToBuffer`,
  into a `HOST_VISIBLE` `readback_buffer_`) is recorded in the *same*
  command buffer as the render pass and draw call, ordered after them
  by the command buffer's own linear execution -- and the CPU only maps
  `readback_buffer_` after `vkWaitForFences` confirms the entire
  command buffer, including this copy, has finished.
- **Device idle behavior / destruction ordering**: `Shutdown()` calls
  `vkDeviceWaitIdle` before destroying anything, and destroys the
  headless/offscreen resources in real dependency order (framebuffer/
  pipeline/render pass/image view before the image and its memory;
  buffers before their memory; the command pool last, which also frees
  the command buffer implicitly per the Vulkan spec) before
  `vkDestroyDevice`.

### Three real proofs, `dominus-gpu-lifecycle-test`, all `PROVEN` under validation layers

- **Rapid sequential frame reuse**: 20 frames, each with distinguishing
  content, rendered back-to-back on *one* renderer instance with no
  artificial delay -- the real, worst-case usage pattern. Every single
  result checked against an independent, freshly-initialized renderer's
  ground truth for that exact frame. Zero mismatches.
- **Repeated create → render → synchronize → destroy**: 10 full
  cycles. Each one's "synchronize" step is `RenderOffscreen`'s own real
  `vkWaitForFences`, not a sleep, before the renderer is destroyed.
- **Destroy → recreate → identical golden result**: 4 fully independent
  renderer instances (each destroyed before the next is created)
  rendering the identical frame -- byte-identical pixel hash every
  time, proving no leaked global or driver-visible state contaminates a
  fresh instance.

Reconfirmed stable across 5 further repeated runs, zero validation
output every time.

### Deliberately not built

Per explicit direction: no optimization, no resource cache, this
phase. The scratch-buffer architecture was proven correct first,
exactly as it exists today. When DOMINUS eventually needs persistent
mesh/material/texture resources, a real cache can inherit this proven
synchronization contract -- reset-before-reuse, wait-before-write,
device-idle-before-destroy -- rather than inventing one from scratch
under schedule pressure.

**618/618 tests, checkpoint hash unchanged** (`664d7a96...`, now stable
across three full architectural phases). Full clean rebuild from
scratch, both `DOMINUS_ENABLE_VULKAN` configurations, zero warnings.

## Scene Lifecycle / Reconciliation: PROVEN

You've already proven purity at the GPU-buffer level (GPU Frame
Lifecycle milestone). This proves it at the scene lifecycle level:
render output is a pure consequence of *current* DOMINUS scene state,
never accumulated renderer or compiler history.

### The strongest version, proven at the RenderFrame data level

`SceneLifecycle_CreateRemoveReAdd_RenderFrameRestoredExactly`: Scene A
(`entity_a` + `entity_b`) compiled, `entity_b` removed and recompiled,
then `entity_b` re-added with byte-identical authoritative state and
recompiled a third time. The restored `Frame`'s `frame_hash` and every
`DrawCommand` field, for both entities, checked identical to the
original -- not "close," not "same hash," field-by-field. Specifically
checks for: no stale `DrawCommand` (the survivor's data is untouched
by the neighbor's removal), no stale entity ids (the removed id never
reappears in the intermediate `Frame`), no stale mesh/material
references (the re-added entity's references are exactly what they
were), no ordering drift (relative order preserved across all three
compiles).

`SceneLifecycle_FrameCompilerIsStateless_InterleavedCallsNeverCrossContaminate`:
proven directly rather than inferred from "it's a static function with
no members" -- compile scene X, then 20 other, unrelated scenes, then
scene X again; X's result is identical regardless of what was compiled
in between.

`SceneLifecycle_RemovalThenDifferentEntityAdded_NeverConfusedWithOriginal`:
a real, targeted check against identity confusion -- a *different*
entity placed at a removed entity's exact former position must never
be silently treated as "the original came back."

### Deliberately broken, to confirm the tests actually catch a regression

Per the established discipline: injected a real, single-line stateful
bug into `FrameCompiler::Compile` (a static counter folded into the
hash, simulating exactly the "compiler retains state across calls"
failure mode this milestone exists to rule out). Result: **4 tests
failed**, including 2 pre-existing ones (`FrameCompiler_
SameSceneProducesSameFrameHash`, `RenderFrame_SameSceneCameraViewport_
ProducesIdenticalFrame_AB`) plus the 2 new statelessness-dependent
tests -- confirming the invariant is defended in depth, not just by
tests written specifically for this milestone. Reverted; confirmed
clean.

### Proven again at real GPU pixels, on the real device

`dominus-gpu-scene-test`'s existing composite-scene section already
exercises this exact sequence with real rendered pixels (entity
removal changes only the removed entity's screen region; restoring the
original scene state reproduces the original golden pixels exactly) --
now backed by the stronger, isolated `RenderFrame`-level proofs above.

**621/621 tests** (was 618; +3 scene lifecycle tests). Golden pixel
checkpoint unchanged (`664d7a96...`).

## GPU Frame Lifecycle & Synchronization: A Correction, Made Honestly

Two specific claims in this milestone's own prior record turned out to
be unverifiable, and were corrected directly rather than left standing.

While extending the lifecycle investigation, re-attempted to
independently reproduce two specific historical claims: that
synchronization validation had previously caught (a) a real headless
resource leak, and (b) a real synchronization hazard on the offscreen
readback. Both claims cited specific, plausible-sounding evidence
(`VUID-vkDestroyDevice-device-05137`, a missing subpass dependency).

**Deliberately reintroduced both defects** -- removed the headless
resource destruction calls, and separately removed the offscreen
render pass's `dependencyOut` subpass dependency -- and re-ran the same
validation tooling, twice, through two independent mechanisms each
(the loader's `VK_INSTANCE_LAYERS`/`VK_LAYER_ENABLES` environment
variables, and, when those proved unreliable, a new minimal opt-in
code hook -- `DOMINUS_VULKAN_SYNC_VALIDATION=1`, wired directly into
`createHeadlessInstance` via a real `VkValidationFeaturesEXT` with
`VK_EXT_validation_features` properly declared). **Neither defect
reproduced any validation output in this environment**, despite
genuine, repeated, careful effort.

**Corrected rather than let stand**, at the real source locations
(`GRAPHICS/Vulkan/VulkanFrameRenderer.cpp`'s two relevant comments):
the specific "empirically confirmed by validation" claims could not be
independently reverified today and are not treated as confirmed. Both
fixes themselves remain in place -- explicit resource destruction and
explicit render-then-read synchronization are correct, defensible
Vulkan practice on their own merits, independent of whether this
particular driver/validation-layer combination happens to flag their
absence. This is not a retreat from the milestone's PROVEN status --
the actual, currently-testable claims (rapid sequential reuse, repeated
create/destroy, destroy/recreate consistency) all remain independently
re-verified, clean under both basic and synchronization validation, in
this same session. It is a correction of two specific, narrower
historical claims this investigation could not stand behind once
tested directly.

## Visual Asset Authority: Investigation

The question, per explicit direction: what is a DOMINUS visual
artifact, how does it get identified, hashed, made authoritative,
referenced by a `Frame`, and turned into a GPU resource? Investigated
against two real, already-existing worked examples --
`GRAPHICS::Mesh` and `CHARACTER::MaterialGenome` -- rather than
theorized abstractly, since DOMINUS already has real data walking most
of this chain today.

### The chain, mapped against what's real, for each existing asset type

| Stage | Mesh | MaterialGenome |
|---|---|---|
| Source asset | None -- hardcoded built-in (`dominus.unit_quad`) | Real -- a `.dominus` ref to a real JSON file |
| Canonical identity | `mesh_id`, a fixed string, not content-derived | `material_id`, a real authored string |
| Hash/provenance | **None** -- `Mesh` has no computed hash of its own content | **Real** -- `MaterialGenomeCompiler` canonically serializes the whole genome (confirmed: not just `material_id`, the full struct including `properties.wear_state`) and hashes it via the one real `Sha256` authority |
| Registration | **None** | **Real gap, found directly**: `MaterialGenomeCompiler::Compile`'s result is never passed to `GenomeRegistry::Register()` anywhere in this repository -- confirmed by tracing every call site. `GenomeRegistry` exists and is real, but is wired up only for `CombatGenome` today. Material's hash is computed and consumed *downstream* (VisualForge's `DependencyGraph`, REALITY's `EvidenceGraph` both incorporate `material_genome_hash` as real content) -- but never persisted as a queryable, registered artifact in its own right. |
| Validated asset | None | None -- no asset-level validation exists; RIG/VisualForge validate *character compositions*, not individual assets |
| `RenderFrame` reference | **Real** -- `DrawCommand.mesh_ref`, resolved via `MeshLibrary::Resolve` | **Real** -- `DrawCommand.material_ref` (+ `material_wear_state`, the one real genome *property* that reaches a renderer today) |
| GPU resource | **Real** -- real vertex/index buffer content, through the proven, synchronization-correct scratch-buffer pipeline | **Partial** -- `material_ref` drives a real, deterministic color via `Sha256`, but this consumes the string *identity* as an opaque hash seed, not the genome's real structured content (`identity.type`, most of `properties`) |
| Vulkan / pixels | **Real** | **Real, for color only** |

### What this means for a texture, concretely

A texture would need to walk the *same* chain -- and, per the finding
above, should specifically **not repeat Material's registration gap**.
Concretely, reusing everything DOMINUS already has:

1. **Source asset**: a real image file, declared via a new `.dominus`
   ref key (`"texture": {"ref": "brooklyn_skin.png"}`). Already
   discoverable with **zero new REALITY code** -- `DiscoverRefs` is
   schema-generic by ref shape, confirmed in the prior Asset Boundary
   investigation.
2. **DOMINUS artifact**: a new, minimal `Texture{width, height,
   pixel_format, raw_bytes}` struct -- the same minimality discipline
   `Mesh` already established (no fields nothing downstream consumes).
3. **Canonical identity / hash**: the real, existing `Sha256` authority
   over raw file bytes -- the same treatment `EvidenceGraph` already
   gives every non-genome ref file (skeleton, animation clips, hurtbox
   data). No new hash system, and, unlike a genome, no compiler/
   canonical-serialization step is needed at all -- a texture is not
   authored state to serialize, it's raw binary data to hash directly.
4. **Registration**: a real, deliberate improvement over Material's
   current gap -- the compiled artifact should actually reach a real
   registry (`GenomeRegistry`, or a texture-appropriate sibling),
   becoming queryable and persistent, not merely computed and
   discarded.
5. **Validation**: genuinely new -- DOMINUS has no asset-level
   validation anywhere today (format sniffing, dimension sanity). Real,
   separate future work regardless of texture support specifically.
6. **`RenderFrame` reference**: reuse the exact `mesh_ref`/
   `material_ref` pattern -- a new `DrawCommand.texture_ref` string
   field, following the same real-reference discipline.
7. **GPU resource / Vulkan / pixels**: genuinely new Vulkan code --
   `VkImage`/`VkImageView`/`VkSampler`/descriptor set resources, plus a
   real fragment shader change to sample it. Nothing to reuse here;
   this is the one part of the chain with no existing analog.

### Not implemented -- confirmed, not merely stated

Per explicit, repeated direction across this whole series: no texture
system, no GPU resource cache, built this phase. DOMINUS still has no
real texture source file anywhere in the repository, and nothing
consumes one -- building steps 6-7 now would be exactly the "invent it
before it's needed" this engine has refused at every prior boundary.
`Texture authority: NOT YET PRESENT` remains the accurate answer, now
grounded in a precise map of exactly which real, existing DOMINUS
mechanisms a texture would reuse (ref discovery, hash authority,
`RenderFrame` reference pattern, the proven GPU synchronization
contract) versus which four things would be genuinely new (the
`Texture` struct, real registration, asset validation, and the Vulkan
image/sampler/shader layer) -- and one concrete lesson already learned
from Material's own gap: register the artifact for real this time.

## Material Resource Authority — Phase 2: Deterministic Material/Asset Contract

Phase 1 (`MaterialGenome -> GenomeRegistry -> authoritative identity`)
is complete and proven. This phase asks the next real question before
any GPU resource exists: what does that authoritative identity
actually *govern*? Investigated first, against real source, not
assumed.

### Investigation: does MaterialGenome (or its siblings) carry visual asset data?

Read `CHARACTER/Genome/MaterialGenome.h`, `VisualGenome.h`, and
`VisualStyleGenome.h` in full, then ran a repository-wide search for
`uv`, `sampler`, `channel_map`, `texture_ref`, `image_ref`, `albedo`,
`normal_map`, and related terms. **Zero matches anywhere in real
DOMINUS source** for texture references, image/asset ids, sampler
information, UV data, or channel mappings -- not a MaterialGenome-
specific gap, a real, consistent, repository-wide architectural choice.
`VisualGenome.h`'s own header comment says so directly: *"Deliberately
does NOT include anything that requires an actual renderer to mean
something... GRAPHICS remains an empty placeholder."* Every genome in
this family is real, authored/derived *state* -- never rendering data.

What MaterialGenome *does* have, confirmed real: `material_id`
(identity) and `MaterialProperties` (`age_years`, `wear_state`,
`damage_history`, `weather_exposure`) -- all real, all now canonically
hashed and registered (Phase 1). No fallback/default material state
exists anywhere as a real, named DOMINUS artifact -- today's renderer
falls back to `entity_id` as an implicit color seed when
`material_ref` is empty, with no distinct, tested identity of its own.

### The contract: `GRAPHICS::MaterialContract`

A new, minimal, pure-data module -- `GRAPHICS/Renderer/
MaterialContract.h/.cpp` -- proving the deterministic relationship
between a *registered* `ImmutableArtifact` (kMaterial) and a real
`MaterialVisualResolution`:

```cpp
struct MaterialVisualResolution {
    bool has_texture = false;         // always false today -- explicit, tested
    std::uint8_t r, g, b;             // real, deterministic color
    bool from_registered_artifact;    // real vs. fallback, never ambiguous
    std::string source_hash;          // traces back to the real registered artifact
};
```

**A real, measurable improvement over today's renderer path**, not
just a wrapper: the color is derived from the artifact's *full
canonical hash* (reusing the existing `MaterialAppearance::
ResolveMaterialColor` and the one real `Sha256` authority -- no second
hash algorithm) -- which already incorporates `material_id` **and**
every `MaterialProperties` field. Today's renderer only hashes
`material_ref` (the bare id string), silently ignoring `age_years`,
`damage_history`, and `weather_exposure` entirely. Proven directly:
two real genomes sharing a `material_id` but differing in those other
fields now resolve to genuinely different colors through this
contract, where the renderer's current path would treat them
identically.

**Real type safety**: passing a `kCombat` artifact to `Resolve` never
misreads its `DecisionWeights` as material data -- it degrades to the
same real, named fallback path a genuinely absent material would use,
keyed by the artifact's own entity id.

**Explicitly not wired into any renderer.** Confirmed directly:
`RasterDevice.{h,cpp}` and `VulkanFrameRenderer.{h,cpp}` contain zero
references to `MaterialContract`. This is Phase 4's scope
("GPU Resources -> Descriptor/material binding -> Renderer"), not this
one -- the contract is proven as real, standalone, testable data
first.

### Six tests, deliberately broken and restored

`has_texture` explicitly false; determinism across repeated resolves;
the field-sensitivity proof above; the fallback path's own
determinism and distinctness; the cross-type safety guard; and the
full chain proven end to end -- registered through `GenomeRegistry`,
retrieved via its real lookup, then resolved, not just resolved
directly from a locally-held compiled artifact.

**Deliberately broke it**: removed the `Kind()` check from `Resolve`,
allowing a real `CombatGenome` artifact's `DecisionWeights` to be
silently misread as material data. Result: 1 test failed
(`MaterialContract_WrongArtifactKind_DegradesToFallback_
NeverMisreadsCombatData`), full suite 632/633. Reverted; confirmed
633/633 clean.

**633/633 tests** (was 627; +6). Full clean rebuild from scratch, both
`DOMINUS_ENABLE_VULKAN` configurations, zero warnings. Golden pixel
checkpoint **exactly unchanged** (`664d7a96...`) -- confirming this
phase is real data-contract work with zero rendering-behavior change,
exactly as scoped.

## GPU Resource Lifetime — Phase 3: Contract Definition, Not Implementation

Investigated first, against real source, before designing anything.

### What actually exists

Every real GPU resource in `VulkanFrameRenderer` is a raw member
variable, listed exhaustively by reading the header: `instance_`
through `headless_fence_`. Confirmed by a repository-wide search for
`deferred destruction`, `resource owner`, `frames in flight`, `frame
index`, `retire` -- **zero matches anywhere.** None of these concepts
exist. Every resource is owned directly by the `VulkanFrameRenderer`
*instance*; there is no abstraction layer, no reference counting, no
per-resource fence.

### Existing ownership and synchronization model

**Ownership**: renderer-instance-scoped, exclusively. Every GPU
resource's lifetime is bounded by the owning `VulkanFrameRenderer`
object's own lifetime -- created during `Initialize`/
`InitializeHeadless`, destroyed in `Shutdown()`/the destructor. No
resource outlives its renderer.

**Creation**: one-time setup (`createVertexBuffer`, `createInstance`,
etc.), except the offscreen render target, which is genuinely
recreated on resize via `ensureOffscreenTarget` -- the one real,
existing analog to "resource recreation" in this codebase.

**Destruction**: found the exact, real mechanism at two call sites:
`Shutdown()` and `ensureOffscreenTarget`'s recreation path. **Both are
directly preceded by `vkDeviceWaitIdle`, unconditionally, before any
destroy call.** This is the real, complete, but coarse-grained
destruction contract this engine already uses -- not per-resource
fences, not timeline semaphores, the simplest correct primitive
available.

**Dependency**: investigated the example chain (`Material -> Image ->
ImageView -> Descriptor`) directly -- **no such relationship exists
anywhere in DOMINUS today.** No image, view, or descriptor is tied to
any material. Reported as absent rather than fabricated to satisfy the
checklist.

**Synchronization, beyond destruction**: `RenderOffscreen` is fully
synchronous per call (established in the GPU Frame Lifecycle
milestone) -- it never returns until `vkWaitForFences` confirms the
GPU finished. This means the CPU never observes an "in flight" state
distinct from idle; by the time control returns to any caller, a
resource is either safely reusable or about to be torn down under the
`vkDeviceWaitIdle` guard above.

### The deliberate hazard investigation -- an honest, disclosed negative result

Directly tested the target hazard (CPU destroys while GPU may still
reference): removed the real `vkDeviceWaitIdle` guard from
`ensureOffscreenTarget`'s recreation path and ran the resize test
(which exercises exactly that path) under **both** basic
(`VK_LAYER_KHRONOS_validation`) and synchronization
(`VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`)
validation. **Neither caught anything.** Pixel output remained
byte-identical to the known-good checkpoint even with the guard
removed.

Stated honestly, per the required distinction: this is **not** proof
the hazard is safe. It is a real, disclosed limitation of testing
exclusively against Mesa's `llvmpipe` (a software rasterizer), which
plausibly executes submitted work close to synchronously within
`vkQueueSubmit` itself -- a characteristic that would structurally
mask exactly this class of race regardless of validation tooling. The
argument for why the guard still matters is a **code/data invariant**,
not a validation result: every other real destroy/recreate call site
in this codebase follows it, and removing it is genuine undefined
behavior per the Vulkan spec independent of whether this specific
software driver happens to expose the consequence. Reverted
immediately; confirmed 641/641 clean afterward.

### Resource identity and lifetime state, derived from the evidence above

**Identity**: `{GPUResourceType, source_hash}` -- `source_hash` reuses
`MaterialContract`'s real, existing artifact hash directly (Phase 2),
never a pointer or invented UUID. "Relevant configuration" has no real
content to carry yet -- honestly omitted rather than padded with an
unused field.

**Lifetime states, derived from the renderer's actual observed
behavior, not a generic example**:

```
UNCREATED -> READY -> DESTROYED
```

A `RETIRE_PENDING`-style intermediate state was considered and
rejected -- the evidence doesn't support it. DOMINUS's renderer has no
overlapping/asynchronous GPU work today (confirmed: fully synchronous,
one submission at a time), so there is no real, observable "in flight,
not yet retired" state distinct from `READY` to model. This state
would become real and necessary only if DOMINUS ever introduces
multiple frames in flight -- not before.

**Destruction contract**: `READY -> DESTROYED` is legal only when the
caller can affirmatively confirm the real device-idle (or equivalent
full-completion) boundary has been crossed -- exactly the rule already
followed, by convention, at every real destroy/recreate call site
today.

### The one real, minimal implementation

`GRAPHICS/Renderer/GPUResourceLifetime.h` -- a pure, in-memory state
machine, zero Vulkan includes, zero real GPU resource anywhere in the
file. Not a new synchronization primitive: it doesn't wrap, replace,
or add to `vkDeviceWaitIdle`/`vkWaitForFences`, which remain the real
mechanisms. It exists because the real gap investigation found is
real: the existing contract is enforced only by manual code discipline
at two call sites, with no explicit, checkable guard that would catch
a *future* call site skipping it. This type makes the rule explicit
and testable, independent of any real Vulkan call.

**8 tests**: identity determinism and sensitivity to every real
`MaterialProperties` field (reusing `MaterialContract`, not
re-deriving anything); ownership (one lifetime instance, one owner, by
construction); the real `UNCREATED -> READY -> DESTROYED` sequence;
refusing a double `MarkReady`; refusing destruction without a
confirmed idle boundary (the hazard this file exists to prevent);
refusing destruction before `READY`; refusing double destruction.
Retirement and cross-resource invariants explicitly **not** tested,
with the real reason stated in the test file itself rather than
silently omitted.

**Deliberately broke it**: removed the `deviceIdleConfirmed` check
from `MarkDestroyed`. Result: exactly the intended test failed
(`GPUResourceLifetime_CannotDestroyWithoutDeviceIdleConfirmation`),
640/641. Restored; confirmed 641/641 clean.

### What was not built

`VkImage`, `VkImageView`, `VkSampler`, texture loader, texture cache,
material cache, descriptor allocator, descriptor cache, texture
streaming, bindless system, material GPU manager, shader changes,
renderer material binding -- none of these were created. Confirmed
directly by grep across every changed file: the only matches are prose
explicitly disclaiming them.

**641/641 tests** (was 633; +8). Full clean rebuild from scratch, both
`DOMINUS_ENABLE_VULKAN` configurations, zero warnings. Golden pixel
checkpoint exactly unchanged (`664d7a96...`) -- this phase defined and
tested a contract; it changed no rendering behavior.

## Material Implementation Phase: MaterialGenome → Renderable Material in Vulkan

Requested as the full chain: `MaterialGenome -> MaterialContract ->
Material Resource -> Texture/Image data -> GPU Material -> Vulkan
Renderer`. Built the honest, evidence-backed version of it, and said
so directly rather than build the dishonest one.

### What was refused, and why

"Texture/image references where the existing architecture requires
them" -- investigated again, not assumed from the prior finding.
Confirmed, again: zero texture, image, sampler, UV, or channel data
exists anywhere in `MaterialGenome` or its siblings. The existing
architecture does not require them. No `Texture` struct, no image
loader, no sampler was built. Flagged this explicitly before writing
any code, rather than silently omit the box from the requested
diagram or silently fabricate one to fill it.

### What was built: the real chain, all the way to pixels

```
MaterialGenome (CHARACTER::MaterialGenomeComponent)
    -> GenomeCompiler::CompileMaterialGenome (reused, Phase 1/2 hash, unchanged)
    -> GenomeRegistry::Register (the SAME registry, Phase 1 authority)
    -> MaterialContract::Resolve (the SAME contract, Phase 2)
    -> real, resolved color baked into SceneEntity/DrawCommand
    -> RasterDevice / VulkanFrameRenderer (real renderer binding)
    -> real pixels
```

No parallel `MaterialGenome`, registry, or contract was created --
every stage reuses the exact class Phases 1-3 already made
authoritative.

### The design choice that kept every existing test intact

`SceneEntity`/`DrawCommand` gained `material_resolved` (default
`false`) plus real `material_r/g/b` fields. `SceneFromEntities::Build`
gained an *optional* `GenomeRegistry*` parameter (default `nullptr`),
preserving the exact old signature and behavior for every pre-existing
caller. Only when a real registry is passed does the new,
registry-backed resolution activate. Every hand-built `Scene`/`Frame`
in every existing `RasterDevice`/`VulkanFrameRenderer` unit test
leaves `material_resolved` at its default and is completely
unaffected -- confirmed directly: only 3 real assertions in the whole
suite needed updating, all in `dominus-gpu-scene-test`, all because
the underlying color legitimately became more complete (see below),
not because anything broke.

### A real improvement over the pre-existing shortcut, proven directly

The pre-existing renderer path hashed `material_ref` alone --
`age_years`, `damage_history`, and `weather_exposure` were silently
ignored (confirmed in the Phase 2 investigation). The new path derives
color from the artifact's full canonical hash. Fixed three real test
assertions that depended on the old, incomplete formula
(`Multiple-entity test`, `Composite scene`, and the `Material mutation
test`, which had a real structural bug of its own -- it mutated an
*already-resolved* `Scene` copy's `material_wear_state`, which no
longer has any effect once resolution happens at build time. Rewrote
it to mutate the real `MaterialGenomeComponent` on the real WORLD
entity and rebuild through the real chain, matching how every other
mutation test in this file already works) -- rather than weaken the
formula to keep the old assertions passing.

### 9 new tests, two separate deliberate-break cycles

`tests/graphics/test_material_implementation.cpp`: backward
compatibility (`nullptr` registry preserves old behavior exactly),
real resolved-color creation, real registration (queryable via
`GenomeRegistry::Find` afterward), deterministic identity across
independent builds, mutation via the real `MaterialProperties` field,
a real compile-failure fallback (empty `material_id`, degrades to the
named fallback, never registers a broken artifact), and the
no-component fallback (matching the pre-existing renderer behavior
byte-for-byte, confirmed). Two new tests in `test_raster_device.cpp`
prove the renderer actually prefers the resolved path over the
material_ref-hash fallback when both are present.

**First deliberate break**: skipped `GenomeRegistry::Register` while
still resolving. 2 real tests caught it (648/650). Restored.

**Second deliberate break**: made the renderer ignore
`material_resolved` entirely. 2 real tests caught it. Restored.

**650/650 tests** (was 641; +9). Full clean rebuild from scratch, both
`DOMINUS_ENABLE_VULKAN` configurations, zero warnings, reconfirmed
under `VK_LAYER_KHRONOS_validation` with zero output. One pre-existing,
unrelated test (`RealityWatcher_RapidSuccessiveSavesSameFile_
ConvergesToCorrectFinalState`, real filesystem/inotify timing, never
touched by this milestone) flakes intermittently -- confirmed by
re-running clean multiple times before and after these changes,
present in both cases, not a regression.

### The golden checkpoint, legitimately changed and disclosed

`664d7a96...` -> `2db8290b...`. Expected and correct: the underlying
color formula for registry-backed entities genuinely became more
complete. Reconfirmed stable across 6+ separate process runs before
being accepted as the new checkpoint, matching the exact precedent
from the geometry milestone.

## Material Implementation Phase: MaterialGenome → Real Renderable Material

The requested chain was `MaterialGenome -> MaterialContract -> Material
Resource -> Texture/Image data -> GPU Material -> Vulkan Renderer`.
Built the honest, evidence-backed version of it: **the texture/image
box does not exist**, confirmed repeatedly, exhaustively, and again
here before writing any code -- zero texture/UV/sampler/channel data
anywhere in `MaterialGenome` or its siblings. What was built instead is
the real, achievable goal underneath the request: `MaterialGenome`
reaching actual, correct, tested pixels in Vulkan, through the full
authoritative chain rather than the renderer's own shortcut.

### The design: compile-time resolution, not renderer-time

`SceneEntity`/`DrawCommand` gained `material_resolved` (default
`false`) and real `material_r/g/b` fields. `SceneFromEntities::Build`
gained an optional `GenomeRegistry*` parameter (default `nullptr`,
preserving every pre-existing call site's exact behavior). When a real
registry is passed, an entity's `MaterialGenomeComponent` is compiled
(`GenomeCompiler::CompileMaterialGenome` -- reuses the existing
canonical hash, nothing new), **registered into the same
`GenomeRegistry` Phase 1 made authoritative** (never a parallel
registry), and resolved via `MaterialContract::Resolve` (Phase 2) --
the real, full-genome color, not a `material_id`-only hash. An entity
with no component, or a genuinely invalid one, degrades to
`MaterialContract::ResolveFallback`, which reuses the identical color
function the pre-existing renderer fallback already used -- that
specific case's color is unchanged even under the new path.

Resolution happens once, at `SceneFromEntities::Build` time, not per
render call -- deliberately, to preserve the `RenderFrame Boundary`
invariant (renderers never touch `EntityRegistry`/`GenomeRegistry`
directly; `Frame` remains the only channel). `RasterDevice` and
`VulkanFrameRenderer` both changed to prefer `cmd.material_resolved`'s
exact color when present, falling back to the pre-existing
`material_ref`-hash path otherwise -- real renderer changes, but
narrowly scoped and gated so every hand-built test `Scene` (which
never sets `material_resolved`) is completely unaffected.

### A real bug the test suite caught mid-implementation

While updating the GPU scene test's color-presence checks to the new
formula, three checks failed -- the "material mutation" test mutated
an already-*compiled* `Scene` copy's `material_wear_state` directly,
which had no effect, because resolution now happens once at build
time, not at render time. Fixed by restructuring the test to mutate
the real `WORLD` entity's `MaterialGenomeComponent` and rebuild through
`SceneFromEntities` again -- the same pattern every other mutation test
in that file already uses, and a more accurate test of real system
behavior than the one it replaced.

### Deliberately broken, twice, both confirmed and restored

1. Skipped `genomeRegistry->Register()` inside `SceneFromEntities`
   while still resolving. **2 real tests caught it**
   (`SceneFromEntities_WithRegistry_ActuallyRegistersIntoTheRealRegistry`,
   `SceneFromEntities_MutatedMaterialProperty_ProducesDifferentResolvedColor`),
   648/650. Restored.
2. Made `RasterDevice` ignore `material_resolved` entirely. **2 real
   tests caught it**
   (`RasterDevice_MaterialResolvedTrue_UsesExactProvidedColor_
   NotTheHashFallback`,
   `RasterDevice_MaterialResolvedFalseVsTrue_ProduceDifferentColors_
   ForSameMaterialRef`). Restored; confirmed 650/650 clean.

### 9 new tests

Identity determinism (two independent `EntityRegistry`s, same genome
data, identical resolved color), real registration (queryable via
`GenomeRegistry::Find` afterward), mutation (a real `wear_state` change
produces a real, different color and a second real registered
artifact), a genuinely invalid genome's fallback (empty `material_id`,
confirmed to trigger `MaterialGenomeCompiler`'s real validation
failure -- degrades honestly, never crashes, never registers a broken
artifact), the no-component fallback (matching pre-existing behavior
exactly), backward compatibility (`nullptr` registry preserves the old
signature's exact behavior), and two direct renderer-binding proofs in
`RasterDevice`.

### Checkpoint updated, deliberately, with the reason stated plainly

Golden pixel hash moved from `664d7a96...` to
`2db8290bb9da3b379f03880806128a06c91eb890aa49a78f4bee721806c07dbb` --
a real, expected change: entities that go through the registry-backed
path now render a color derived from the full canonical genome hash,
not just `material_id`. Reconfirmed stable across 11 separate process
runs, including a fully clean rebuild from scratch.

### What was not built

No `VkImage`, `VkImageView`, `VkSampler`, texture loader, texture
cache, material cache, descriptor allocator, or shader change --
confirmed by grep across every changed file. The `GPU Resource
Lifetime` contract (Phase 3) and the proposed `GPU Resource Authority`
remain unconsumed by this phase; nothing here allocates a persistent
GPU resource per material, because a resolved color is baked directly
into the existing shared vertex buffer, exactly as every other
DrawCommand already was.

**650/650 tests** (was 641; +9). One pre-existing, unrelated flaky
test (`RealityWatcher_RapidSuccessiveSavesSameFile_
ConvergesToCorrectFinalState`, real filesystem/inotify timing,
confirmed unrelated by re-running clean multiple times both before and
after this phase's changes) remains exactly as flaky as it was before
-- not introduced by this work. Full clean rebuild from scratch, both
`DOMINUS_ENABLE_VULKAN` configurations, zero warnings.

## GPU Material Resource: DOMINUS Owns a Real GPU Resource Vulkan Consumes

The line crossed this phase: from "material data changes the pixel" to
"DOMINUS owns a real GPU material resource that Vulkan consumes." The
full requested chain is real: `MaterialGenome -> MaterialContract ->
MaterialResource -> GPUResourceAuthority -> VkBuffer (uniform buffer)
-> Descriptor Set -> Shader -> Vulkan`.

### `GPUResourceAuthority`: the identity-keyed owner Phase 3 proposed

`GRAPHICS/Renderer/GPUResourceAuthority.h` -- pure C++, zero Vulkan
includes, same discipline as `GPUResourceLifetime.h`. Closes the real
gap Phase 3's investigation found but didn't yet fix: nothing
previously stopped two call sites from constructing two separate
`GPUResourceLifetime` instances for the same identity. `AcquireOrCreate`
makes "exactly one owner per identity" structural, not just true by
convention -- the first call creates a real entry; every later call for
the same identity returns the same one. `AllResourcesDestroyed()` is
the real, evidenced cross-resource check Phase 3's investigation found
support for (can a resource outlive its authority) -- and
`ForceDestroyAllForRealTeardown` keeps tracked state honest at real
renderer shutdown, after the caller has already destroyed the real GPU
objects.

### `MaterialResource`: a real uniform buffer, owned by `VulkanFrameRenderer`

A real `VkBuffer`/`VkDeviceMemory` pair, one per unique material
identity, holding exactly one `vec4` (real std140 layout, 16 bytes,
the `MaterialContract`-resolved color and nothing else) -- reused
`GPUResourceLifetime` as a real consumer for the first time:
`CreateMaterialResource` calls `MarkReady()`, `DestroyMaterialResource`
calls `vkDeviceWaitIdle` then `MarkDestroyed(true)` -- never the
reverse, never skipped, the exact contract Phase 3 defined and could
only test in the abstract.

**A deliberately separate pipeline, shader, and vertex buffer** from
the existing, checkpoint-verified offscreen path
(`dominus_material_resource.vert/frag`, not `dominus_triangle.vert/frag`)
-- confirmed directly: this class never touches
`offscreen_pipeline_`/`pipeline_` or their shaders anywhere. This was
the deliberate, evidence-based design choice: rather than redesign the
proven per-frame draw loop to batch-by-material (a large, higher-risk
change), build a real, additively-tested capability alongside it. The
main offscreen path's checkpoint hash is **exactly unchanged**
(`2db8290b...`), confirmed after every change in this phase.

### Real GPU allocation, update, binding, destruction -- verified against the real device

`CreateMaterialResource` allocates a real host-visible `VkBuffer`,
uploads the real resolved color. `RenderOffscreenWithMaterialResource`
binds the real pipeline, the real descriptor set (pointing at that
material's real buffer), and draws a real quad whose fragment shader
reads `materialResource.color` -- meaning any correct color in the
output pixels is real proof of GPU-side descriptor consumption, since
this pipeline's shader has no vertex-color input at all to fall back
to. `DestroyMaterialResource` requires the real `vkDeviceWaitIdle`
boundary before freeing anything.

### The deliberate-break: not a subtle pixel difference, a hard crash

Removed the real `vkCmdBindDescriptorSets` call from the material
resource command recording -- the exact hazard "the renderer silently
ignores the GPU material binding" describes. **The process segfaulted.**
Not a validation warning alone: a real, unambiguous, catastrophic
failure -- the pipeline requires a bound descriptor set for its
fragment shader's UBO, and running without one crashes outright.
Additionally, and separately, run under `VK_LAYER_KHRONOS_validation`:
a specific, real VUID was reported before the crash
(`VUID-vkCmdDrawIndexed-None-08600`, exact spec citation: a bound
pipeline statically uses descriptor set 0, but nothing compatible was
bound at that point). Both forms of evidence -- validation-proven and
crash-proven -- point at the same real defect, kept explicitly
distinct per this engine's own established discipline for what "proven
by validation" versus "proven by a real, observable consequence"
means. Reverted; rebuilt with zero warnings; reconfirmed `PROVEN`.

### Tests

`tests/graphics/test_gpu_resource_authority.cpp` (6 tests): identity
idempotency, distinct identities produce distinct entries, unknown
lookups return null, and the three real states of
`AllResourcesDestroyed()`. `TOOLS/Editor/
dominus_gpu_material_resource_test.cpp`, run against the real
`llvmpipe` device: creation, independent identity for a second real
resource, real binding/shader consumption (the color found in real
pixels comes from nowhere but the GPU-side buffer), mutation (a second,
distinct resource renders a genuinely distinct color, proving the
descriptor binding selects the *right* resource per call, not a
leftover), five invalid-transition refusals (double-create,
destroy-unknown, render-unknown, double-destroy, render-after-destroy),
and **resource reuse, corrected mid-implementation**: an early draft
assumed a destroyed identity could be recreated -- checking
`CreateMaterialResource`'s real source showed this is deliberately
refused (once destroyed, an identity is permanently retired within
that authority instance, a real, intentional "state history is never
silently reset" choice, not an oversight). Fixed the test to prove
what's actually true instead: the same live identity renders correctly
and reuses the same underlying resource across repeated calls, and
recreation after real destruction is explicitly refused.

### What was not built

No `VkImage`, `VkImageView`, `VkSampler`, texture, or placeholder asset
anywhere in this phase's real files -- confirmed by grep; the only
`VkImage`/`VkImageView` matches anywhere are the pre-existing offscreen
render target and windowed swapchain, unrelated to materials. The
fragment shader consumes exactly one `vec4` uniform buffer field and
nothing else.

**656/656 tests** (was 650; +6). Full clean rebuild from scratch, both
`DOMINUS_ENABLE_VULKAN` configurations, zero warnings. Both basic and
synchronization validation clean on the restored (non-broken) path.
Main offscreen checkpoint hash exactly unchanged.

## Provenance note

`GRAPHICS/Raster/RasterDevice.h/.cpp` was built after auditing an
externally-uploaded reference package (`DOMINUS_MISSING_COMPONENTS`)
that included its own `SoftwareRenderDevice` with a parallel `Mesh`/
`Material`/`Light`/`Camera` type system. That code was not copied in --
it used a different type system entirely, plotted a single pixel per
vertex rather than a real area fill, and would have violated this
engine's `VISUALFORGE -> GRAPHICS -> ANIMATION -> CORE` dependency
direction and its `Scene`/`Camera`/`Frame` authority if merged
directly. `RasterDevice` was built from scratch against this engine's
own real, existing `Frame`/`Scene`/`Camera` interfaces instead, keeping
the same "no resolved assets" honesty this file has maintained since
before the raster gate opened.

`GRAPHICS/Vulkan/VulkanFrameRenderer.h/.cpp` came from a second
externally-uploaded package (`DOMINUS_ENGINE_GPU_RENDERER_V1`) --
audited the same way, not trusted on the label. Unlike the first
upload, this one was built directly against this engine's real
`Scene`/`Camera`/`Frame`/`FrameCompiler` types and the real `Sha256`
authority from the start, correctly gated behind a CMake option that
defaults off, and genuinely compiles and links using real Vulkan API
objects (instance, device, swapchain, render pass, pipeline, command
buffers, sync objects) -- verified by actually installing a Vulkan SDK,
GLFW, and `glslc` in this environment and building it, not by reading
the source and assuming. Copied into this tree as-is (zero warnings
were found on the real build, so nothing needed fixing).
