# DOMINUS Rig Forge — Phase R0 Audit

**Status: pure dependency/design-specification audit. Zero implementation. No
code in this repository was changed to produce this document.** Same
discipline as `HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md` and
`HITM_RENDER_INPUT_LOOP_AUDIT.md`.

**The objective, stated as given:** not "fix the old rig." Determine
whether DOMINUS can construct a real, usable character rig — skeleton,
bind pose, pivots, part→bone bindings, transform hierarchy, animation
bindings, secondary motion, collision attachments — directly from the
real HITM source assets, for all three fighters, without inventing
missing data.

**Answer, in one line:** yes, almost entirely — and the evidence for how
much is real, tool-generated, pixel-verified source data (not previously
imported by any Track H module) is stronger than expected. Exactly one
real, well-bounded field (a per-bone anchor coordinate, `.at`) is
genuinely absent from the checked-in source tree; it is very plausibly
derivable from other real data already imported, but that derivation is
an unproven hypothesis, not yet implemented or tested, and is named as
exactly that below.

---

## 1. What exactly is broken?

Nothing is "broken" in the sense of a bug. **The old rig
(`brooklyn.dominus`'s skeleton, and its migrated `brooklyn_canonical`
counterpart) is not derived from real HITM data at all — it is a
separate, independently-authored, fictional dataset that happens to share
Brooklyn's name.**

Evidence, not inference:

- `tests/fixtures/brooklyn.dominus`'s own `provenance` field:
  `"source_assets": ["brooklyn_art_v3", "brooklyn_motion_bible"]` — these
  names correspond to nothing in the real hitm-engine source tree
  (`assets/parts/brooklyn_atlas.png`, `data/characters/brooklyn/*.json`).
  They are placeholder labels for a dataset built to exercise DOMINUS's
  own generic character pipeline (`CHARACTER/Rig/RigBinder.cpp` loads a
  `.dominus`'s `skeleton`/`animations`/`moves`/seven more "genome"
  components — a fully generic, HITM-agnostic authoring format) before
  the real HITM asset-import track (this repo's Track H) existed.
- Its skeleton — `root`/`torso`/`head`/`arm_r`/`arm_l`/`leg_r`/`leg_l`,
  per `RIG/CanonicalSkeleton.h`'s own header comment ("Brooklyn's real
  skeleton, extended in the prior 'bone rig fix' phase") — bears no
  relationship to any real HITM bone/part name. Real HITM parts are named
  `torso`, `head`, `armFarU`/`armFarL`, `armNearU`/`armNearL`,
  `legFarU`/`legFarL`, `footFar`/`footNear`, `legNearU`/`legNearL`,
  `footNear`, plus per-fighter features (`hat`/`jaw`/`dreadFar`/
  `dreadNear`/`tie`/`chain`/`coatFar`/`coatNear`/`handFar`/`handNear` for
  Brooklyn). Six of the old rig's seven bone names don't even share a
  *word* with a real HITM part name.
- The later, more elaborate `brooklyn_canonical.skel.json` (24-bone
  canonical biped: `root`/`pelvis`/`spine_01-03`/`chest`/`neck`/`head`/
  `clavicle_L`/`upperarm_L`/`forearm_L`/`hand_L`/... ) has real, but
  small, round, hand-typed-looking bind-pose offsets (`pelvis: y=40`,
  `neck: y=30`, `clavicle_L: x=-15,y=10`) — nothing resembling the real
  HITM data's own numeric character (`0.5188349514563106`,
  `0.26`,`0.275`, precise to 10+ decimal places, machine-computed). This
  skeleton was proven mathematically equivalent to the OLD 7-bone rig
  (`RIG/README.md`'s own "Phase 4" section, 546 position/rotation
  comparisons) — equivalent to a fictional rig is still fictional.
- `RIG/README.md`'s own words, already on record: "Brooklyn's real
  skeleton... does not conform" to the canonical contract, and the
  canonical migration is "proven mathematically and numerically
  equivalent to Brooklyn's **original 7-bone rig**" — the README itself
  never claims either skeleton traces back to real HITM source assets.

**Root cause, precisely:** this old system assumes a conventional
mesh-and-bone character pipeline — a skeleton exists so that a mesh can
be skin-weighted to it and deformed. `RIG/README.md`'s own "5 explicitly
NOT_DECLARED" checks list names exactly this gap: "inverse bind
matrices, skin weights, weight normalization... This engine has no mesh,
skin-weight, or inverse-bind-matrix data model anywhere." **There has
never been anything for this skeleton's bones to actually deform.** It
is bones with nothing attached — real, valid, generic infrastructure,
built and proven against itself, but never connected to any real
character.

## 2. What did the old rig assume?

- A **3D-mesh-adjacent bone/skin pipeline** (bind pose, inverse bind
  matrices as a named future gap), not a 2D cutout/part pipeline.
- A **name-and-topology-first design**: pick a plausible biped hierarchy
  (7 bones, later 24), author plausible offsets, THEN see if a character
  fits it — the inverse of what real HITM's own rig actually is (a real
  per-fighter hierarchy *derived from* that fighter's own design and DNA,
  see section 3).
- **Bind-pose-first authoring**: every bone's `bind_pose_local`
  (`ANIMATION::Skeleton`'s own real type, `Transform2D` = x,y,rotation,
  scale) is a real, required field with no fallback — this system has no
  concept of "a bone that exists in the hierarchy but has no real
  authored position," which is exactly the situation Real HITM's control
  bones are in (see section 4).
- **One shared skeleton per lineage**, not one real, independently-DNA-
  derived rig per fighter — `RigProfile`'s whole migration apparatus
  exists specifically to map one legacy skeleton onto one canonical
  target. Real HITM's actual per-fighter bone counts differ (29 for
  Brooklyn, 26 for Rocket, 26 for Static — see section 8) precisely
  because each fighter's rig is independently derived from that
  fighter's own DNA/design/combat-genome, not a shared template.

## 3. What real source data exists?

Far more than any Track H module has imported so far. Two real files
never previously touched by this track turned out to be the crux of this
audit:

### `data/identity/<fighter>/design.json` — real, authored, human-facing source

Self-labeled: `"_note": "AUTHORED SOURCE. What this character physically
has, and where it lives in assets/sprites/<fighter>.png (normalized
0-1)."` Two sections:
- `core_parts` — the fighter's real, essential anatomy (11 for Brooklyn:
  head/torso/both arm-upper/both arm-lower/both leg-upper/both
  leg-lower/both feet), each a real `rect` + real `pivot`.
- `features` — fighter-specific real extras (10 for Brooklyn:
  hat/jaw/dreadFar/dreadNear/tie/chain/coatFar/coatNear/handFar/
  handNear), each with a real `rect`/`pivot` **and a real `parent`
  field** (`hat.parent = "head"`, `coatFar.parent = "hip"` — note "hip"
  is named here as a real parent even though it owns no drawn part of
  its own), plus the real secondary-motion authoring source
  (`lagBeats`/`maxAngle`/`gravity`/`dnaThreshold`) already imported by
  Module 3's `HitmBoneFollow` — this is where that data actually comes
  from, traced to its origin for the first time in this track.
- Real, disclosed pivot-correction provenance, present for several
  parts: `"_pivot_measured": "rig_validate.py: authored pivot [0.62,
  0.12] landed on transparent pixels; snapped to nearest opaque [0.654,
  0.214]. Measured, not estimated."` — a real human/tool authored a
  first-guess pivot, a real tool checked it against the real atlas
  pixels, and corrected it where it landed on transparency.

### `data/characters/<fighter>/rig_validation.json` — real, tool-generated, measured

Self-labeled: `"_generated": "rig_validate.py — measured from source
pixels"`. For **every** drawn part (22/22 for Brooklyn, confirmed for
Rocket/Static too — see section 8): `bone` (name), **`parent`** (a real
parent bone name for every single part, including `core_parts`, which
`design.json` itself only states a `parent` for `features`), `pivot`,
`pivot_on_opaque: true/false` (a real, measured check that the pivot
lands on non-transparent atlas pixels), `overlap_px`/`overlap_zone`/
`covering_ancestor` (real, measured z-order/covering relationships
between parts), `parent_is_control_bone: true/false` (!) — this file
already, explicitly, distinguishes structural "control" bones (no
drawn part) from real drawn-part bones, the exact distinction the real
FK algorithm needs (see section 4). `_issues: []` for all three
fighters — a clean, fully-validated result, not a partial one.

### `data/characters/<fighter>/rig.json` — real, tool-compiled, complete hierarchy

Self-labeled: `"_generated": "rig_compiler.py — derived from
character_dna + design + combat_genome"`. Already partially imported by
Module 3 (`HitmPartsRig`, parts+pivots) and Module 5B Phase 1
(`HitmRigPlacement`, placement rects) — but its `bones[]` array is real,
complete, and richer than either module currently uses:
- `_bone_count`/`_derivation` — real, explicit provenance: for Brooklyn,
  "core: 19 bones (kinetic chain)" + "design: 10 granted [...]" (the 10
  real `features` from `design.json`). For Rocket: 19 core + 7 granted +
  **2 explicitly denied by DNA** (`[('coatFar', 75), ('coatNear', 75)]`)
  — real, evidenced, DNA-driven per-fighter rig variation, not
  copy-pasted.
- Every bone: real `name`, real `parent` (a complete, real, 29/26/26-deep
  hierarchy — including 5 real **control bones with no drawn part at
  all**: `root`, `hip`, `neck`, `shoulderFar`, `shoulderNear`), real
  `_why` (a genuine, human-authored design rationale per bone, e.g.
  hip: "kinetic chain: where ground force enters the body"), and for
  kinetic-chain segment bones, a `len` field — **real, but uniformly
  `0.1` for every single bone that has it**, across every kinetic-chain
  bone this audit checked. This is a disclosed, verified finding, not an
  assumption: `len` is not a real per-bone measured length, it is a
  placeholder constant. It should not be treated as real geometric data
  if a rig generator is ever built against it.
- `follow{}` blocks — already fully imported by Module 3.

### `data/characters/<fighter>/anim.json` — real, already-imported animation, with one new finding

Already imported by Module 5B Phase 1 (`HitmAnimationSet`). This audit's
new finding: **the real `idle` clip's track keys include `hip`,
`shoulderFar`, and `shoulderNear`** — three of the five real control
bones. These bones are not just structural placeholders in the real
data; they carry real, authored rotation/offset animation of their own,
which must propagate to their children through real forward kinematics
for the animation to look correct. (`root` and `neck` are not
independently animated in any real clip checked.)

### `engine/render/SkeletonSystem.js` — the real, authoritative FK algorithm

Never previously read by this track. This is hitm-engine's own real,
working bone-hierarchy poser — `build(charId, localPose, displayH)` —
and it is the direct, authoritative answer to "what does the real engine
actually do with `rig.json`'s `bones[]` array." Full algorithm, cited
because it is short and every field it touches matters:

```js
for(const name of r.order){
  const b = r.bones[name], lp = localPose[name] || {rot:0,dx:0,dy:0};
  if(!b.parent){
    world[name] = { x:(b.at[0]-0.5)*spriteW + lp.dx*displayH,
                    y:(b.at[1]-1.0)*displayH + lp.dy*displayH,
                    rot: lp.rot*DEG };
    continue;
  }
  const p = world[b.parent], pb = r.bones[b.parent];
  let ox, oy;
  if(!pb.part){
    // parent is a control bone: 'at' is in source-sprite normalized space
    ox = (b.at[0]-0.5)*spriteW - ((pb.at[0]-0.5)*spriteW);
    oy = (b.at[1]-1.0)*displayH - ((pb.at[1]-1.0)*displayH);
  } else {
    const m = r.parts[pb.part], [pw,ph] = partSize(pb.part);
    ox = (b.at[0]-m.pivot[0])*pw;
    oy = (b.at[1]-m.pivot[1])*ph;
  }
  const c = Math.cos(p.rot), s = Math.sin(p.rot);
  world[name] = { x: p.x + ox*c - oy*s + lp.dx*displayH,
                  y: p.y + ox*s + oy*c + lp.dy*displayH,
                  rot: p.rot + lp.rot*DEG };
}
```

Two real fields drive this, and neither appears anywhere in any real
checked-in JSON for any of the three fighters: `b.at` (a real per-bone
anchor, in one of two different normalized spaces depending on the
parent's type — see section 4) and `b.part` (which drawn part, if any,
this bone represents).

This confirms, precisely and for the first time with the actual real
algorithm in hand (not inferred from a header comment), exactly what
Module 5A/5B's own prior finding already said in general terms
(`HitmAnimationSet.h`: "`parts.json`'s real `bones[]` entries carry no
bind-pose anchor (`.at`) and no declared part ownership (`.part`)
field") — now cross-checked against the real consuming algorithm itself,
for all three fighters, not assumed.

## 4. What data is missing?

**Exactly two real fields, both used by `SkeletonSystem.build()`, both
absent from every real `rig.json`/`design.json`/`rig_validation.json`
checked for all three fighters:**

1. **`b.part`** — which drawn part (if any) a bone represents.
2. **`b.at`** — a real, per-bone 2D anchor coordinate. Its meaning
   depends on the bone's real parent's type (per the algorithm above):
   - If the bone's parent is a control bone (no part): `b.at` is
     normalized across the **whole sprite** (the same convention
     `HitmSceneBridge`'s own real anchor already uses for the root:
     `(0.5, 1.0)` = character horizontal center, character base).
   - If the bone's parent owns a drawn part: `b.at` is normalized
     **within that parent part's own local rect** (the same 0..1 space
     `pivot` already uses).

Nothing else is missing. Every other input the real algorithm needs
(`parts[].pivot`, `parts[].normW/normH` — real, imported since Module 3
— `bones[].parent`, `bones[].follow`, `anim.json`'s real tracks,
`sourceSize`/`displayH`/`atlas`) is real, already checked in, and mostly
already imported by earlier, closed Track H modules.

## 5. What can DOMINUS derive?

**`b.part`: fully, mechanically, losslessly derivable — zero fabrication
risk.** Every non-control bone's real `name` in `rig.json`'s `bones[]`
matches a real key in `rig.json`'s own `parts{}` exactly (verified for
all 17 non-control Brooklyn bones, and structurally true by
`rig_compiler.py`'s own real, disclosed derivation for the other two
fighters). The rule `bone.part = bone.name if bone.name in parts else
null` reproduces `rig_validation.json`'s own real, independently-tool-
verified `parent_is_control_bone` flag exactly (a bone with `part=null`
is precisely a "control bone" per that file). This is not a guess about
a possible convention — it is confirmed against two independent real
sources (the name-matching itself, and `rig_validation.json`'s own
already-computed, already-verified control-bone flag) agreeing.

**`b.at`: a real, testable, NOT-YET-PROVEN derivation hypothesis — named
as a hypothesis, not a fact.** For a bone that owns a real part, that
part's own real `rect`+`pivot` (already-imported, real, pixel-verified
data) gives a real candidate value for `.at`, in exactly the two spaces
the algorithm needs:
- **Whole-sprite space** (for a control-bone-parented bone): the part's
  own pivot point converted from "normalized within its own rect" to
  "normalized within the whole sprite" — `at_x = rect_x0 +
  pivot_x*(rect_x1-rect_x0)`, `at_y = rect_y0 + pivot_y*(rect_y1-rect_y0)`
  — using exactly the real `rect`/`pivot` values `design.json`/
  `rig.json`/`rig_validation.json` already, redundantly, agree on.
- **Parent-part-local space** (for a part-parented bone): the same
  whole-sprite pivot point, re-expressed as a fraction of the PARENT
  part's own real rect.

This is a real, disclosed, reproducible arithmetic rule over already-real
inputs — not fabricated data. **It has not been implemented, run, or
checked against the real algorithm's actual output in this audit.** The
honest way to prove or disprove it is Phase R1: implement it, pose a
known real clip/frame, and compare the result against
`HitmSceneBridge`'s already-proven placement (itself already verified
pixel-perfect against the real committed atlas fixture, Track H Phase
5B) or, more directly, against a real rendered frame from the actual
hitm-engine JS runtime if one can be produced for comparison. Until that
comparison exists, this is a hypothesis with real supporting evidence,
not a proven derivation — treat it as exactly that if Phase R1 is
authorized.

**Control bones with no real part of their own (`root`, `hip`, `neck`,
`shoulderFar`, `shoulderNear`) have no `rect`/`pivot` to derive `.at`
from at all — this is the one genuine gap the derivation above does not
close**, addressed next.

## 6. What must be authored?

**Five real numbers per fighter: the `.at` anchor for each fighter's own
real control bones.** For Brooklyn: `root`, `hip`, `neck`, `shoulderFar`,
`shoulderNear`. (Rocket/Static: same 5 real control-bone names, per the
shared 19-bone "core kinetic chain" — see section 8.) These bones exist
purely to give their real children a parent-relative anchor; nothing in
the real, checked-in source data states where they themselves sit.

This is NOT unbounded — a real, disclosed, geometrically-motivated
inference is possible for each (e.g. `hip`'s real children are
`torso`/`legFarU`/`legNearU`/`coatFar`/`coatNear`, all real drawn parts
with real rects; a plausible `hip.at` might be inferred from where those
children's own rects converge) — but that is itself a second, compounding
hypothesis on top of the `.at`-for-part-owning-bones hypothesis above,
and this audit explicitly declines to propose it as anything more than
"a real, later, separately-scoped question," per the instruction not to
invent bind-pose values to close this gap. The honest options for Phase
R1, in order of how much human authoring they require:
1. **Derive control-bone anchors from real children data too** (the
   geometric-inference idea above) — smallest human input, but the least
   proven; needs its own explicit validation before trusting it.
2. **Hand-author exactly 5 real numbers per fighter** (15 total, across
   Brooklyn/Rocket/Static) — the minimum real, honest human-authoring
   requirement if derivation is not trusted. Five real anchor points is a
   small, bounded, reviewable ask — nothing like re-authoring a whole
   rig.
3. **Treat control bones as a Track H-specific extension that never
   needs a real `.at`** — since `HitmSceneBridge`'s own already-proven
   placement convention (Track H Phase 5B) never actually walks the real
   bone hierarchy at all; it places each drawn part directly from its own
   real `rect`+`pivot`+sampled pose, which is already sufficient for
   real, pixel-perfect rendering (proven). Real FK may be valuable for a
   FUTURE goal (a true, generalized 2D rig editor; more accurate
   secondary-motion propagation through control bones) without being a
   prerequisite for anything Track H currently needs. This option is
   named explicitly because it is real and legitimate, not a consolation
   prize — Phase R1 should decide, with the audit's own evidence in
   hand, whether closing the FK gap is worth pursuing at all right now.

## 7. What should the new rig output?

A real artifact this engine already has almost every real type for:

- **Skeleton**: `ANIMATION::Skeleton` (`Bone{name, parent_index,
  bind_pose_local}`) already exists, generic, already proven (used by
  the old system). A real HITM-derived skeleton would populate it with
  real bone names/parents/bind poses derived per sections 5-6 above,
  instead of the old system's fictional ones.
- **Bind pose**: `Bone::bind_pose_local` (a real `Transform2D`) — the
  derived/authored `.at` anchors (section 5/6) converted into this real
  type.
- **Pivots**: already real, already imported (`HitmPartsRig`).
- **Part → bone bindings**: the derived `b.part` mapping (section 5).
- **Transform hierarchy**: `Skeleton::ComputeBindPoseWorld()` already
  exists and already does real parent-to-child composition — it is
  generic, untested against real HITM data, but structurally ready.
- **IK/FK**: FK is what section 5's derivation targets. Real IK
  (`ANIMATION/IK/IKChainLoader.h` exists generically) has no real,
  evidenced HITM source data behind it anywhere this audit found — out
  of scope unless a future audit finds real source for it.
- **Animation bindings**: already real, already imported
  (`HitmAnimationSet`), and already keyed by the same real bone/part
  names this section's skeleton would use — no format change needed.
- **Secondary motion**: already real, already imported
  (`HitmBoneFollow`/Module 3), now traced to its real origin
  (`design.json`'s `features`) for the first time.
- **Collision attachments**: this audit found **no real, evidenced HITM
  source data for hurtboxes/hitboxes/collision volumes anywhere** in the
  files inspected (`ai.json`, `moves.json`, `character.json` were not
  deeply re-examined this pass beyond confirming they exist for all
  three fighters — a real, named gap for the NEXT audit pass to close
  before claiming collision attachment is derivable, not assumed here).

**The honest target artifact, precisely stated**: a real per-fighter
`ANIMATION::Skeleton` + `HitmPartDraw`-compatible part/bone binding table,
built from real `rig.json`+`design.json`+`rig_validation.json`+
`parts.json` data, with every non-control-bone anchor mechanically
derived (section 5, once proven) and every control-bone anchor either
authored (5 real numbers/fighter) or explicitly deferred (option 3
above) — never fabricated, never silently defaulted to zero.

## 8. Can the same process work on all three fighters?

**Yes — checked directly, not assumed.** Every real file this audit
examined for Brooklyn exists, in the same real, tool-generated shape,
for Rocket and Static:

| | Brooklyn | Rocket | Static |
|---|---|---|---|
| `design.json` exists | yes | yes | yes |
| `rig.json` exists, same generator | yes (`rig_compiler.py`) | yes | yes |
| `rig_validation.json` exists, same generator | yes (`rig_validate.py`) | yes | yes |
| Real `_bone_count` | 29 | 26 | 26 |
| Core kinetic-chain bones | 19 | 19 | 19 |
| Design-granted feature bones | 10 | 7 | 7 |
| DNA-denied features (real, disclosed) | 0 | 2 (`coatFar`/`coatNear`, "denied by DNA") | 0 |
| `rig_validation.json` `_issues` | `[]` | `[]` | `[]` |

The same 5 real control-bone names (`root`/`hip`/`neck`/`shoulderFar`/
`shoulderNear`) recur in all three fighters' real `bones[]` — the
"kinetic chain" core is a shared, real, uniform 19-bone structure across
the roster, with each fighter's own real feature bones layered on top
per that fighter's own real DNA/design. **Rocket's real, evidenced
"2 denied by DNA" entry is itself strong evidence this is a genuine,
working per-fighter generation process, not a template blindly copied
three times** — the real compiler made a real, different decision for a
different fighter based on real DNA data.

Nothing in this audit found a Brooklyn-specific hack anywhere in the real
source data or in what a rig generator would need to consume. The one
open question (control-bone `.at` derivation, section 5/6) is not
Brooklyn-specific either — it recurs identically for all three fighters'
same 5 control-bone names.

---

## What this audit deliberately does not do

- Does not implement anything. No `.h`/`.cpp` file was created or
  modified.
- Does not fabricate a value for `.at` on any control bone, or claim the
  part-owning-bone `.at` derivation hypothesis (section 5) is proven —
  it is named, evidenced, and explicitly marked unproven.
- Does not examine hurtbox/hitbox/collision source data deeply enough to
  claim it is or isn't derivable (section 7's own named gap).
- Does not propose retiring `brooklyn.dominus`, `brooklyn_canonical.*`,
  `RIG/CanonicalSkeleton.h`, or any of the existing generic rig/
  acceptance apparatus — that infrastructure is real, tested, and
  unrelated to this finding; whether it has a future role (e.g. as the
  real `ANIMATION::Skeleton` TARGET TYPE a HITM-derived rig populates,
  reusing `RigAuthorityValidator`'s real structural checks against a
  real, HITM-appropriate profile) is a real, separate decision for
  whoever authorizes Phase R1, not decided here.
- Does not touch anything already frozen: Vulkan, `FillTriangle`, the
  combat runtime, Ghost Dash, the application loop, `HitmSpriteDrawData`/
  `HitmSceneBridge`'s own already-proven, already-pixel-verified
  placement logic (this audit's findings are about a DIFFERENT,
  currently-unused code path — real bone-hierarchy FK — not a critique
  of the placement convention Track H Phase 5B already shipped and
  proved correct).

## Proposed next-step candidates (not authorized, for the next checkpoint to choose from)

1. **R1a — prove or disprove the `.at` derivation hypothesis.** Smallest,
   most direct next step: implement section 5's formula for
   part-owning bones only, pose one real clip/frame, and check the
   result against `HitmSceneBridge`'s already-proven output for the same
   frame. Cheap to attempt, directly answers whether section 5's central
   claim is real.
2. **R1b — the control-bone anchor decision.** Requires a human/product
   decision among section 6's three named options before any code
   follows from it.
3. **R1c — a standalone `.part` deriver + `ANIMATION::Skeleton` builder**
   for all three fighters, independent of whether `.at` is solved yet
   (mechanically safe per section 5's first paragraph) — produces a real
   skeleton with correct topology and part bindings, bind poses TBD
   pending R1a/R1b.
4. **A fresh, dedicated audit of `ai.json`/`moves.json`/`character.json`**
   for real hurtbox/hitbox/collision data before claiming section 7's
   "collision attachments" output is achievable at all.

This document does not choose among them — that is explicitly the next
checkpoint's call, per the same discipline every prior Track H audit in
this repository has followed.
