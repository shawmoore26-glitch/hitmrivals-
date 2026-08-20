# Track H Module 5B — Real HITM Sprite/Texture Integration (Phase 1)

Date: 2026-08-20
Scope: connect the real visual HITM content (sprite atlases, cutout-rig
metadata, skeletal animation clips) to the CPU runtime Module 5A already
proved -- CPU-observable only. Explicitly NOT: GPU rendering, actual
displayed pixels, audio, input devices, or anything Module 5A itself
didn't already cover.

Method, same as every prior Track H module: every claim below was
checked against source or an actual test/CLI run in this session, not
inferred. Real Brooklyn/Rocket/Static data throughout unless stated
otherwise.

## Executive summary

The full pipeline this module set out to build now runs end to end with
real data:

```
REAL HITM ASSETS -> HitmAssetImporter -> DOMINUS asset representation
-> Brooklyn runtime (Module 5A, untouched) -> ANIMATION/FRAME SELECTION
-> SPRITE DRAW DATA -> renderer (out of scope)
```

Given Brooklyn's real, already-proven runtime state (Module 5A) at any
frame, this module deterministically computes: which real hitm-engine
animation clip should be showing, which real frame of that clip, and,
for every one of Brooklyn's 22 real drawn parts, the real atlas-pixel
source rect, the real normalized placement, and the real per-clip local
pose (rotation + offset) sampled from the real, generated `anim.json` --
all CPU-only, all deterministic, all traced to real HITM data or a
verified, faithful port of hitm-engine's own real
`AnimationSystem.js`/`SkeletonSystem.js` algorithms. Live proof, real
Brooklyn data, `dominus-cli hitm-sprite-draw-data`:

```
[hitm-sprite-draw-data] fighter_id=brooklyn atlas=.../brooklyn_atlas.png (1024x269 real px) parts=22 clips=26
[hitm-sprite-draw-data] frame=0 (init) clip="idle" raw_frame=0 sampled_frame=0 parts=22 torso[frame=(104,0,107,166) pose_rot=-2deg]
[hitm-sprite-draw-data] frame=49 (attack_startup) clip="special" raw_frame=1 sampled_frame=1 parts=22 torso[pose_rot=-0.6875deg]
[hitm-sprite-draw-data] frame=62 (attack_active) clip="special" raw_frame=14 sampled_frame=14 parts=22 torso[pose_rot=16.8513deg]
[hitm-sprite-draw-data] frame=66 (attack_recovery) clip="special" raw_frame=18 sampled_frame=18 parts=22 torso[pose_rot=12deg]
[hitm-sprite-draw-data] frame=84 (hitstun) clip="hurt" raw_frame=0 sampled_frame=0 parts=22 torso[pose_rot=0deg]
```

`raw_frame` at each attack sub-state boundary (1, 14, 18) is not
approximate -- it is the exact, real elapsed-frame count this module
reconstructs from Module 5A's own public `state_frames_remaining`
countdown plus the real move's `startup`/`active`/`recovery` totals, and
it matches the real `special` animation clip's own authored timeline
exactly (verified: `startup(14)+active(4)+recovery(18) == 36 ==`
the real clip's own `len`).

**No pixel was ever decoded, sampled, or drawn.** This module reads a
PNG file's header (signature + declared width/height) to validate real
atlas bounds; it never opens a GPU device, a Vulkan context, or any
rendering surface. See "IMPLEMENTED BUT GPU-UNVERIFIABLE" below.

## PROVEN CPU

- **Real asset discovery.** `HitmAssetImporter::Import` follows the real,
  authored `identity.json`'s own `sprite.rig`/`sprite.anim` relative
  paths (not a DOMINUS-invented convention) to locate
  `data/characters/<fighter>/{parts,anim,rig}.json`, and the real,
  evidenced `assets/parts/<atlas>.png` convention (confirmed directly
  against hitm-engine's own `tools/slice_rig.py`'s and `tools/build.py`'s
  real output paths, not guessed) to locate the atlas image. Verified for
  all three real fighters.
- **Asset identity validation.** The atlas PNG's real signature and
  declared pixel dimensions are read from its own file header (no
  invented "should be valid" assumption); every one of Brooklyn's/
  Rocket's/Static's real 19-22 parts' real `frame` rects are cross-
  checked to fit entirely within those real declared bounds (verified
  3/3; Rocket's real atlas is packed edge-to-edge, `maxX == 1024`
  exactly). `parts.json`'s own `atlas` field is cross-checked against
  `identity.json`'s independently-authored `sprite.atlas` for the same
  fighter.
- **Fighter/asset association.** Every bundle is built from a real,
  already-imported `HitmIdentityRecord` (Module 1) -- there is no path
  by which an asset bundle can be built without the real identity data
  that names it.
- **Atlas metadata representation.** `HitmPartsRig` (Module 3, reused
  unmodified) already captures every real part's pivot/normalized size/
  atlas-pixel frame rect and the real draw order; this module adds
  nothing invented on top of it.
- **Frame/cutout representation.** `HitmRigPlacement` imports the real,
  generated `rig.json`'s per-part normalized placement rect, cross-
  validated against `parts.json`'s pivot for the same part (verified:
  the two independently-generated real files agree exactly, 3/3 real
  fighters, every part).
- **Animation/frame selection driven by the real Brooklyn runtime.**
  `BuildSpriteDrawData` reads Module 5A's own public
  `HitmFighterSnapshot` (zero changes to Module 5A) and selects a real
  `anim.json` clip name and a real elapsed-frame count via a direct,
  verified port of hitm-engine's own `AnimationSystem.js`
  `clipFor()`/`frameFor()`. See "A real, evidenced correspondence" below
  for the exact-match proof.
- **Deterministic conversion from runtime state to sprite draw data.**
  `HitmSpriteDrawData_Determinism_SameSnapshotProducesIdenticalDrawData`
  proves byte-identical output from an identical snapshot, every field,
  every part, every time.
- **Preservation of source asset metadata.** Nothing is flattened,
  summarized, or renamed on the way in -- `HitmAnimationClip`/
  `HitmPartPlacement`/`HitmAtlasPart` carry the real authored/generated
  values through untouched.
- **Deliberate-break coverage.** Missing atlas file, corrupted PNG
  signature, an atlas too small for the real declared frame rects,
  malformed `anim.json`, a fighter missing a required clip, `rig.json`
  missing a real part, a pivot disagreement between `rig.json` and
  `parts.json`, every `identity.json`-level `sprite` field mutation
  (missing block, missing `rig` path, mismatched atlas name, a `rig`
  path pointing at the wrong fighter's directory), and an attack-state
  snapshot given no current move -- 33 negative tests total, every one
  failing loud with a specific, real, actionable error, never a partial
  or defaulted bundle.

## A real, evidenced correspondence (not asserted -- verified)

Brooklyn's real `special` animation clip's own authored `len` (36) is
EXACTLY `startup(14) + active(4) + recovery(18)` from his real
`signature.json` special move -- two independently-authored/generated
real files agreeing to the frame. This is what makes exact elapsed-frame
reconstruction from Module 5A's countdown-based `state_frames_remaining`
possible without inventing an interpolation/rescaling convention: the
real clip timeline and the real move timeline are the same timeline.

## Two real architectural findings this module's own audit surfaced

### 1. hitm-engine's own `SkeletonSystem.js` cannot actually run against the real checked-in data

`SkeletonSystem.build()` (full bone-hierarchy forward kinematics) reads
`bones[].at` (a bind-pose anchor in source-sprite-normalized space) and
`bones[].part` (which atlas part a bone visually carries) from
`parts.json`. **Neither field appears anywhere in any of the three real
fighters' real `parts.json` files** -- verified by direct inspection, not
assumed. Without `.part`, `build()`'s own
`r.order.find(n => r.bones[n].part === pName)` lookup returns `undefined`
for every part, and its draw list ends up empty.

This is not a DOMINUS omission -- it is a property of the real source
data as committed in this repository. hitm-engine's own real offline
verification tool, `tools/rig_render.py` ("reproduces SkeletonSystem's
transform chain... proves parts assemble into a figure"), does NOT
actually use `SkeletonSystem.build()`'s bone-walk either: it places every
part directly from `rig.json`'s real `parts.<name>.rect` (a normalized
top-left placement, verified 3/3 real fighters to exactly match
`parts.json`'s `pivot` for every part). **This module follows
`rig_render.py`'s real, working convention** (`HitmRigPlacement`), not
`SkeletonSystem.js`'s convention that the real checked-in data cannot
actually satisfy. The smallest correct extension to close this gap for
real, matching `SkeletonSystem.js`'s own intent, would be a `rig_compiler.py`
change (out of this session's control -- it lives in hitm-engine, not
DOMINUS) to emit `bones[].at`/`bones[].part`; documented here as a real,
specific, actionable upstream gap rather than papered over with invented
bind-pose data on the DOMINUS side.

### 2. A real bug in this module's own first validation pass, found and fixed by real data

An early version of `HitmAnimationSet::Import` required each animation
track's keyframe `frame` numbers to be strictly increasing. It
immediately crashed a test importing Brooklyn's own real `anim.json` --
not a deliberate-break fixture, his actual real file. Root-caused (via a
clean AddressSanitizer+UndefinedBehaviorSanitizer build, which reported
the exact null-pointer dereference and its call chain) to real,
deliberate "anticipation snap" authoring: Brooklyn's real
`heavy`/`footFar` track repeats frame 0 with a different value, and his
real `light1`/`legFarU` track even places an earlier frame number
*after* a later one in the array
(`[[0,0,0,0],[1,12,0,0],[0,-18,0,0],[11,0,0,0]]`). hitm-engine's own real
`SkeletonSystem._sample()` never assumed monotonicity either -- its
bracket-pair scan simply never satisfies its condition for an out-of-
order pair, making such entries real, present, and harmless. **The
ordering check was removed, not weakened or worked around** -- it was a
DOMINUS-invented rule the real data does not follow, so it was wrong to
have at all. See `HitmAnimationSet.h`'s header comment and
`test_hitm_animation_set.cpp`'s
`Sample_RealOutOfOrderKeyframesStillSampleCorrectly` for the full account
and the regression proof against the exact real track that caught it.

## A real, evidenced finding about authoring coverage

Rocket's and Static's real `anim.json` files have full clip coverage
(`idle`, `walk`, `special`, `hurt`, ...) despite their real
`signature.json` "special" moves lacking fields
(`blockstun`/`range`/`hitstop`) that Module 5A's `HitmMoveInstance`
requires -- confirmed live: `dominus-cli hitm-sprite-draw-data` against
Rocket fails at move extraction (Module 5A's own real, documented gap),
never at asset import. Animation authoring and combat-data authoring are
separately complete/incomplete per fighter in the real HITM data; this
module's asset-import layer does not require the same completeness
Module 5A's combat layer needs, and correctly does not fail where it
doesn't have to.

## IMPLEMENTED BUT GPU-UNVERIFIABLE

- Vulkan/image/sampler/texture upload -- not attempted, no code written.
- Actual GPU sampling of the real atlas texture -- not attempted.
- Actual displayed pixels -- not attempted, and not claimed. This
  sandbox has no GPU, no display, and this project's own methodology
  (established in the GRAPHICS phases) requires real-device verification
  for any such claim. `ReadPngDimensions` reads a file header; it never
  opens a decoder, a texture, or a device.

## Track A gap #1 closed: secondary motion (spring/follow system)

hitm-engine's own real `SkeletonSystem._secondary()` drags every bone
flagged `follow` in `parts.json`'s real `bones[]` array (dreads, coat,
chain, tie, hat, jaw, the glove-bounce hand overlays) toward its
parent's rotation through a spring/damper, so they arrive late and keep
moving after the parent stops -- Volume 5's sentence law, "coat, dreads,
chain travel a full beat after he stops." The real, authored per-bone
`stiffness`/`damping`/`lagBeats`/`maxAngle`/`gravity` params were already
imported losslessly by Module 3, unused by anything until now:
`ApplySecondaryMotion()` is a direct, line-by-line port of the real
function (including its real quirks -- JavaScript's `||`-as-fallback for
a zero-valued field, and the real name-keyed bone lookup silently
resolving `handFar`/`handNear`'s LATER, follow-flagged duplicate entry
over the earlier rigid one, verified by a dedicated regression test).

Per-fighter spring state genuinely persists across frames (unlike
everything else in this pipeline) -- `HitmSecondaryMotionState` is an
explicit, caller-owned object (mirroring the real engine's own per-
fighter `Map`, not a hidden global), threaded through
`BuildSpriteDrawData`'s new optional `secondaryMotion` parameter
(`nullptr` by default -- fully backward compatible, unchanged behavior
for every existing caller). Live proof, real Brooklyn data,
`dominus-cli hitm-sprite-draw-data`: `dreadFar` (authors no track in
ANY real clip -- idle, walk, jump, or special) now shows continuous,
varying, real spring-driven rotation every frame instead of sitting
frozen at zero.

7 new tests, including: an exact frame-0 initialization proof (computed
via the identical floating-point operation sequence as production, per
this session's established float-precision discipline, not a rounded
literal); a 30-frame independent "shadow simulation" differential test,
retyped fresh against the real algorithm rather than calling into
production's own private helper, matching bit-for-bit; a real-safety
invariant (`|angle| <= maxAngle`, checked every frame across a long,
varied replay -- walk, jump, land, attack, get hit); a two-independent-
replays determinism proof, the same methodology Module 5A's own
determinism proof used; and the `handFar`/`handNear` duplicate-bone-name
regression test described above. All pass under a clean AddressSanitizer
+UndefinedBehaviorSanitizer run, zero findings.

## NOT IMPLEMENTED (explicitly out of this module's Phase 1 scope)

- Full bone-hierarchy forward kinematics / world-space part transforms
  (see finding #1 above -- blocked on real, missing upstream bind-pose
  data, not a DOMINUS choice to skip it).
- `landT`-driven `'land'` clip and `WALK`-direction-driven `'walkBack'`
  clip -- Module 5A's runtime has no landing-recovery timer and no
  facing/opponent concept (see HitmSpriteDrawData.h's header comment);
  this module always selects `'idle'`/`'walk'` respectively, documented,
  not silently wrong.
- Per-state elapsed-frame tracking for idle/walk/jump (`animT`-equivalent)
  -- Module 5A's public `HitmFighterSnapshot` only exposes the match-wide
  monotonic `frame` counter for these states; the smallest correct
  extension (a `state_entry_frame` field on `HitmFighterRuntime::
  FrameState`) is identified but deliberately not implemented here, per
  the explicit instruction not to reopen Module 5A without a genuine
  defect forcing it.
- Everything Module 5A itself does not implement (second fighter,
  audio, input devices, stage) -- unchanged, not touched by this module.

## Track A gap #2 closed: BuildSpriteDrawData proven against Rocket's and Static's own real assets

Their asset layer (`HitmAssetImporter`) already imported cleanly; what
had never been exercised was `BuildSpriteDrawData` itself against their
real data. `HitmFighterRuntime::Create` still cannot build a full
runtime for either of them (Module 5A's own real move-schema gap,
unchanged, not reopened) -- but `HitmFighterSnapshot` is a plain, public
struct, and idle/walking/jumping states need no move data at all, so
`test_hitm_sprite_draw_data_multi_fighter.cpp` proves the draw-data
pipeline against hand-constructed (but real-physics-grounded -- the
exact same `(wallL+wallR)/2`/`ground` start-position formula
`HitmFighterRuntime::Create` itself uses) snapshots for both fighters:
real clip selection (idle/walk/jumpUp/jumpDown), real atlas frame rects,
and -- most valuably -- real secondary motion, proven via the same
exact-arithmetic differential methodology used for Brooklyn's "chain",
against each fighter's own genuinely different real spring constants
(Rocket stiffness=0.268/damping=0.784; Static stiffness=0.184/
damping=0.652; Brooklyn 0.118/0.634 -- three distinct real values, not
one constant silently reused). This also caught and corrected a real
documentation error this closure's own audit surfaced: an earlier claim
here that stiffness/damping were "identical across all three real
fighters" was wrong -- each fighter has its own uniform pair, matching
`HitmBoneFollow`'s own DNA-derived-spring-constants design intent, fixed
in `HitmSpriteDrawData.h`'s header comment rather than left standing.

7 new tests, all green under a clean AddressSanitizer+
UndefinedBehaviorSanitizer build, zero findings.

## Test count

66 new tests: 15 in `test_hitm_animation_set.cpp`, 11 in
`test_hitm_rig_placement.cpp`, 18 in `test_hitm_asset_importer.cpp`, 22
in `test_hitm_sprite_draw_data.cpp` (exact frame-arithmetic proofs for
every attack sub-state and both stun states, the secondary-motion
suite described above, a determinism proof, and 3 deliberate-break
tests), 7 in `test_hitm_sprite_draw_data_multi_fighter.cpp` (Rocket/
Static real draw-data + secondary-motion proofs, described above).
**831/831 total** (was 765 before this module, 656 before Track H).

## Verification

1. Clean Release build (`rm -rf build`): zero errors, zero warnings.
2. Full suite: **831/831 passed**, exit 0.
3. Clean Debug+AddressSanitizer+UndefinedBehaviorSanitizer build: zero
   errors, zero warnings. Full suite under it: **831/831 passed**, zero
   sanitizer findings (checked via precise diagnostic-marker greps, not
   a naive substring match).
4. Live `dominus-cli hitm-sprite-draw-data` run against real Brooklyn
   data, reproduced above -- real clip selection, real exact elapsed-
   frame values at every attack sub-state boundary, real per-part pose
   data.
5. Live negative runs: Rocket (fails at Module 5A's own real move-
   extraction gap, not asset import) and a deliberate-break fixture
   (fails cleanly with a specific real error) both confirmed via direct
   CLI invocation, not just the test suite.
6. Fresh-clone verification (`git clone` the pushed branch into a
   scratch directory, clean build, full test run) before push.

No test was weakened to reach this result. The one real bug this module
found in its own first pass (the keyframe-ordering assumption) was fixed
by removing the incorrect assumption entirely, verified against the
exact real data that caught it, not patched around.

## Final Module 5B (Phase 1) status

- **PROVEN CPU**: asset discovery, identity validation, fighter/asset
  association, atlas metadata representation, frame/cutout
  representation, runtime-driven animation/frame selection, deterministic
  draw-data generation, source-metadata preservation, real per-bone
  secondary motion (spring/follow system), deliberate-break coverage --
  see "PROVEN CPU" and "Track A gap #1 closed" above.
- **IMPLEMENTED BUT GPU-UNVERIFIABLE**: nothing attempted -- deliberately.
  This module wrote zero GPU/rendering/display code.
- **NOT IMPLEMENTED**: full bone-hierarchy FK (blocked on a real upstream
  data gap, documented), `land`/`walkBack` clips, per-state elapsed-frame
  tracking for continuous states, and everything beyond this module's own
  scope (audio, input, stage, a second fighter) -- see "NOT IMPLEMENTED"
  above.

Module 5B Phase 1 is complete on its own terms: the simulation Module 5A
proved can now select the actual real HITM visual content that should be
showing, expressed as deterministic, CPU-provable draw data -- with
every place this connects to a real gap (upstream missing bind-pose data,
Module 5A's own unmodeled facing/landing/per-state-timing) named
specifically rather than smoothed over.
