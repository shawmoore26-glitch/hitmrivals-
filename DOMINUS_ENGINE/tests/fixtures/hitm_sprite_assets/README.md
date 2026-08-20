# hitm_sprite_assets fixtures — provenance

`data/characters/{brooklyn,rocket,static}/{parts,anim,rig}.json` and
`assets/parts/{brooklyn,rocket,static}_atlas.png` are byte-identical
copies of the real, generated hitm-engine files at the same directory
layout the real engine itself uses (`data/characters/<fighter>/*.json`,
`assets/parts/<atlas>.png` -- evidenced by `tools/slice_rig.py`'s and
`tools/build.py`'s own real output paths). Same archive and commit as
`../hitm_identity/README.md` and `../hitm_parts_rig/README.md`
(`babe381`). `parts.json` here is the same real file `hitm_parts_rig/`
already holds (Module 3) -- duplicated here rather than cross-referenced
so this module's fixtures are self-contained and independently
reviewable, the same choice every prior module's fixture directory made.
`anim.json` and `rig.json` are real files no prior Track H module has
ever imported. Verified byte-identical via `diff`/`cmp` at copy time.

This tree is laid out as a miniature real hitm-engine root (`data/` +
`assets/` siblings) specifically so `HitmAssetImporter::Import` can be
pointed at it directly, exercising the real, evidenced discovery
convention end to end rather than a DOMINUS-invented shortcut.

## A real, evidenced finding this module's own audit surfaced

hitm-engine's own real `engine/render/SkeletonSystem.js` `build()`
function (full bone-hierarchy forward kinematics) requires every real
`parts.json` bone entry to carry a bind-pose anchor (`.at`) and a
declared part ownership (`.part`) field. Neither field appears anywhere
in any of the three real fighters' real `parts.json` files -- verified,
not assumed. hitm-engine's own real offline verification tool
(`tools/rig_render.py`, "reproduces SkeletonSystem's transform chain...
proves parts assemble into a figure") does NOT use that code path either:
it places every part directly from `rig.json`'s real `parts.<name>.rect`
(normalized top-left placement, verified 3/3 real fighters to exactly
match `parts.json`'s `pivot` for every part) rather than walking a bone
hierarchy. `HitmRigPlacement`/`HitmAssetImporter`/`HitmSpriteDrawData`
follow `rig_render.py`'s real, working convention, not
`SkeletonSystem.js`'s convention that the checked-in real data cannot
actually satisfy. See `HitmAssetImporter.h`'s and
`HitmSpriteDrawData.h`'s top comments for the full accounting.

## A second real, evidenced finding

An early version of `HitmAnimationSet::Import` required each track's
keyframe `frame` numbers to be strictly increasing and immediately failed
to import Brooklyn's own real `anim.json` (crashing a test that assumed
the check would only ever reject deliberately-broken fixtures). The real
data is genuinely not monotonic: real "anticipation snap" authoring
repeats a frame number with a different value (Brooklyn's real
`heavy`/`footFar` track has two keyframes at frame 0), and a few tracks
even place an earlier frame number after a later one in the array
(Brooklyn's real `light1`/`legFarU`:
`[[0,0,0,0],[1,12,0,0],[0,-18,0,0],[11,0,0,0]]`). hitm-engine's own real
`SkeletonSystem._sample()` never assumed monotonicity either -- its
bracket-pair scan simply never satisfies its condition for an out-of-
order pair, making such entries real but inert. The ordering check was
removed rather than kept as false rigor; see `HitmAnimationSet.h`'s
header comment for the full account and
`test_hitm_animation_set.cpp`'s `Sample_RealOutOfOrderKeyframesStillSampleCorrectly`
for the regression proof.

## `broken_*/` deliberate-break fixtures

Each is a miniature hitm-engine root built from Brooklyn's real files
with exactly one real invariant broken, for
`HitmAssetImporter`/`HitmAnimationSet`/`HitmRigPlacement`'s negative
tests:

- `broken_missing_atlas_file/` — real parts/anim/rig.json, no atlas PNG at all.
- `broken_corrupt_atlas_signature/` — the real atlas PNG's first byte overwritten, so it no longer starts with the real PNG signature.
- `broken_atlas_too_small/` — a synthetic, minimal, VALID 10x10 PNG (not real HITM art -- generated purely as negative-test scaffolding, the same role every other module's `broken_*` fixture plays) standing in for the atlas, too small to contain any of Brooklyn's real part frame rects.
- `broken_anim_malformed_json/` — Brooklyn's real `anim.json` truncated mid-object (invalid JSON syntax).
- `broken_anim_missing_required_clip/` — Brooklyn's real `anim.json` with the `"idle"` clip removed entirely.
- `broken_rig_missing_part/` — Brooklyn's real `rig.json` with the `"torso"` part entry removed, while `parts.json` still declares it.
- `broken_rig_pivot_mismatch/` — Brooklyn's real `rig.json`'s `"torso"` pivot changed to a value that disagrees with `parts.json`'s real pivot for the same part.
