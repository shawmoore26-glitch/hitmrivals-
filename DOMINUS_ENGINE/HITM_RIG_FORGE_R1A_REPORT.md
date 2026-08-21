# DOMINUS Rig Forge — Phase R1a Report

**Status: implemented, tested, verified. Not an audit — real code, real
tests, run against real HITM data for all three fighters.** Scope set by
the checkpoint that authorized this phase, verbatim: implement and
rigorously test the `.at` derivation hypothesis
`HITM_RIG_FORGE_AUDIT.md` section 5 names, for part-owning bones only.
Do not choose the control-bone anchor values (Phase R1b's decision). No
magic offsets: every tolerance below is a real, measured, explained
number, not a value chosen to make a test pass.

## What was built

- **`CHARACTER/HitmBridge/HitmRigForgeAnchor.h/.cpp`** — derives `.part`
  (mechanical, Phase R0-proven) and the candidate `.at` (this phase's own
  hypothesis) for every real bone in a fighter's `rig.json`/`parts.json`,
  from already-imported, real `HitmPartsRig`/`HitmRigPlacement` data.
  Never fabricates a control-bone anchor: a bone with no drawn part gets
  `at_x`/`at_y = std::nullopt`, full stop.
- **`CHARACTER/HitmBridge/HitmSkeletonFk.h/.cpp`** — a faithful, line-for-
  line port of the real, authoritative `engine/render/SkeletonSystem.js`
  `build()` function (quoted in full in the R0 audit), including the one
  real per-axis scale divergence from `HitmSceneBridge` this phase found
  (see below). Two entry points: `ComputeLocalOffset()` (one bone's own
  offset from its parent, the only real quantity Phase R1a can test
  without a control-bone anchor) and `BuildWorldTransforms()` (the full
  recursive walk, real and reusable, but expected — and proven — to fail
  at `root` against a real fighter's Phase R1a-only data).
- **`tests/integration/test_hitm_rig_forge_at.cpp`** — 4 new tests, all
  three real fighters, real fixture data, zero synthetic/fabricated
  numbers except where explicitly labeled as a self-contained unit test
  of software correctness.

## Finding 1 — the topology is a hard, total blocker, not an edge case

Every real bone whose own real parent is a control bone was enumerated
directly from each fighter's real `rig.json` (not assumed):

| | control bones (shared) | real drawn parts | blocked (parent = control bone) | testable (parent = a part) |
|---|---|---|---|---|
| Brooklyn | root, hip, neck, shoulderFar, shoulderNear | 22 | **8**: torso, head, armFarU, armNearU, legFarU, legNearU, coatFar, coatNear | 14 |
| Rocket | root, hip, neck, shoulderFar, shoulderNear | 19 | **7**: torso, head, armFarU, armNearU, legFarU, legNearU, tail | 12 |
| Static | root, hip, neck, shoulderFar, shoulderNear | 19 | **8**: torso, head, armFarU, armNearU, legFarU, legNearU, coatStripFar, coatStripNear | 11 |

The same 5 real control-bone names recur identically across all three
fighters (the shared 19-bone kinetic-chain core the R0 audit already
found) — this is not a Brooklyn-specific pattern. **23 of the 60 real
drawn parts across the whole roster (torso, head, and every limb's own
upper segment, plus each fighter's own torso-attached feature) sit
*directly* on a control bone.** Because `root` itself is always first in
real topological order and never owns a part, `HitmSkeletonFk::
BuildWorldTransforms()` run against any real fighter's Phase R1a-only
bones (no control-bone `.at`) fails immediately, and the test
(`HitmSkeletonFk_BuildWorldTransforms_FailsAtRootWithoutControlBoneAnchors`)
proves it does, for all three fighters, naming `root` explicitly in the
failure message rather than defaulting anything to zero.

**This directly answers the question Phase R1b was deferred to ask**: if
DOMINUS ever wants real, absolute, recursive FK posing (not
`HitmSceneBridge`'s existing, already-proven, non-hierarchical
placement), control-bone anchors are not an optional refinement — they
block the majority of the character's own torso/head/limb roots, not
just a cosmetic detail on the extremities.

## Finding 2 — the core hypothesis: real, evidenced, small, bounded convergence

For the 37 real (child, parent) bone pairs across all three fighters
where BOTH bones own a drawn part (the only pairs Finding 1 leaves
testable), `HitmSkeletonFk::ComputeLocalOffset()` was run with each
child's Phase R1a-derived `.at`, and compared against the real
pivot-to-pivot delta computed **directly from `rect`+`pivot`, with no
detour through `.at` at all** — an independent ground truth, not a
circular check.

| fighter | pairs tested | max \|deviation\| (px, displayHeight=225) |
|---|---|---|
| Brooklyn | 14 | 2.88 px (Y), 1.94 px (X) |
| Rocket | 12 | 3.43 px (Y), 1.30 px (X) |
| Static | 11 | 2.79 px (Y), 2.13 px (X) |
| **All 3** | **37** | **3.43 px max, ~1.5% of a 225px-tall character** |

The deviation is real and explained, not noise: `rig.json`'s real
placement `rect` and `parts.json`'s real atlas-measured `normW`/`normH`
disagree by a real, uniform, per-fighter constant —

```
normW - rectWidth  =  0.0388 (Brooklyn) / 0.0328 (Rocket) / 0.0241 (Static)   -- uniform across every part of that fighter
normH - rectHeight =  0.01667 ( = 1/60 exactly)                              -- uniform across ALL parts of ALL 3 fighters
```

— confirmed identical (to floating-point noise) across every one of the
22/19/19 real parts checked per fighter. This is intentional
render/placement slack, the exact phenomenon `rig_validation.json`'s own
real, tool-measured `fill_pct`/`slack_px` fields already document (e.g.
Brooklyn's torso: `fill_pct: 79.0`, meaning the drawn art already
legitimately extends past its own placement `rect` on purpose, to allow
real z-order overlap with neighboring parts — `rig_validation.json`'s own
`overlap_px`/`covering_ancestor` fields for that exact part). The
`.at`-derivation formula uses `rect` (the placement source); the real FK
offset formula scales by `normW`/`normH` (the atlas-measured source) —
so this slack constant leaks into the reconstructed offset by
construction, not by a bug in either formula. Both are real; they simply
answer different questions ("where is this part *placed*" vs "how big is
its atlas-measured cutout"), and the real engine's own algorithm mixes
them.

**Verdict: the hypothesis converges.** A ~1–3.5 pixel deviation on a
225px-tall character (bounded, explained, uniform-per-fighter, never
exceeding ~1.5% of character height) is what "the geometry is derivable"
looks like when the real source data isn't perfectly self-consistent
between its own placement and atlas-measurement conventions — not a
disproof. `HitmRigForgeAt_PartToPartOffset_ConvergesOnRealPivotDeltaWithinEvidencedBound`
encodes this exact, measured bound (4.0px, a small margin over the
observed 3.43px max) — not a number picked to force a pass.

## Finding 3 — a second, independent, real architectural divergence (not part of the hypothesis, disclosed anyway)

Comparing the FK offset directly against the real, unmodified
`HitmSceneBridge::BuildHitmSceneEntities` (called exactly as-is, per the
checkpoint's explicit "do not modify the sprite bridge" instruction)
surfaces a **second, much larger, entirely separate** real divergence,
present even before Finding 2's own small residual is added:

1. **Anchor point.** `HitmSceneBridge` positions each part at its own
   `rect` **center** (`place + normW/2`). The real FK algorithm's
   `world[boneName]` is the part's own **pivot** point (torso's real
   pivot, `[0.5, 0.1]`, sits near the top of its rect — nowhere near the
   center `[0.5, 0.5]`). These are two different, real, legitimate
   reference points for two different, real, legitimate purposes
   (`HitmSceneBridge`'s own header comment already documents its choice
   is deliberate); they simply are not the same point.
2. **Axis scale.** `HitmSceneBridge` scales both axes uniformly by
   `displayHeight` (`H`) — a deliberate, disclosed, already-proven choice
   for *that* module (see `HitmSceneBridge.h`'s own header comment). The
   real `SkeletonSystem.js` scales the X axis by
   `spriteW = sourceWidth/sourceHeight*displayHeight` — a real,
   per-fighter, non-uniform constant (`spriteW/H` = 0.429 Brooklyn / 0.508
   Rocket / 0.692 Static, since no real fighter's sprite sheet is
   square). Neither module is wrong; they are two independently-real,
   independently-disclosed, mutually inconsistent conventions that have
   never been compared against each other until this phase.

`HitmRigForgeAt_RawGapAgainstRealHitmSceneBridge_IsExactlyExplainedByTwoNamedRealCauses`
proves this is the WHOLE explanation, not a hand-wave: for all 37 real
pairs, `(FK offset) − (real HitmSceneBridge delta)` was checked
numerically against `(pivot-space delta) − (rect-center-space delta)` —
i.e. exactly the two named causes above, computed independently, with no
free parameter — and the residual after subtracting them is Finding 2's
own already-bounded ≤3.43px, not a new, unexplained gap. Nothing was
tuned to make this converge; the two causes were identified first, from
reading both real formulas, then checked against real numbers.

**This is not a defect in either system.** `HitmSceneBridge`'s own
placement convention is unchanged, still proven, still pixel-verified
against the real committed atlas fixture, and this phase did not touch
it. It is a real, previously-unknown fact worth recording: **if a future
phase ever renders through real recursive FK instead of
`HitmSceneBridge`'s current convention, character proportions and part
alignment will visibly differ** — a genuine, disclosed consequence of
adopting real FK, not a hidden cost.

## What this phase did NOT do

- Did not derive, guess, or hand-author a value for any control bone
  (`root`, `hip`, `neck`, `shoulderFar`, `shoulderNear`, any fighter).
  `HitmRigForgeAt_DeriveBoneAnchors_NeverFabricatesControlBoneAnchors`
  checks this directly, for all three fighters.
- Did not modify `HitmSceneBridge.h/.cpp` — it is called, read-only, as
  an oracle, exactly as the checkpoint required.
- Did not introduce a correction constant anywhere to force any of the
  above numbers toward zero. Every tolerance in
  `test_hitm_rig_forge_at.cpp` is a real, measured, explained bound.
- Did not touch Vulkan, `FillTriangle`, the combat runtime, Ghost Dash,
  the application loop, or any already-frozen module.

## What Phase R1b now has, that it didn't before this phase

1. **A real, quantified answer to "do control bones matter for what
   Track H actually ships today?"** No — `HitmSceneBridge`'s existing,
   already-proven convention never walks the bone hierarchy at all, and
   this phase changed nothing about that. Track H's shipping renderer
   remains fully correct and untouched.
2. **A real, quantified answer to "do control bones matter for real FK at
   all?"** Yes, completely — 23 of 60 real drawn parts across the roster
   sit directly on one, and `root` itself blocks the entire recursive
   walk immediately. There is no partial-credit path; option 1 from the
   R0 audit's section 6 (derive control-bone anchors from their real
   children) would need to be a real, separately-proven derivation of its
   own before real FK could ever produce an absolute position for any
   part on any fighter.
3. **A real number for what adopting real FK would visually cost**,
   independent of whether the anchors get authored or derived: Finding 3's
   pivot-vs-center and spriteW-vs-H divergences apply regardless.

None of this chooses for R1b. It gives R1b real numbers to choose with.

## Verification

- 4 new tests (`HitmRigForgeAt_DeriveBoneAnchors_NeverFabricatesControlBoneAnchors`,
  `HitmRigForgeAt_PartToPartOffset_ConvergesOnRealPivotDeltaWithinEvidencedBound`,
  `HitmSkeletonFk_BuildWorldTransforms_FailsAtRootWithoutControlBoneAnchors`,
  `HitmRigForgeAt_RawGapAgainstRealHitmSceneBridge_IsExactlyExplainedByTwoNamedRealCauses`),
  all real fixture data, all three fighters, 37 real bone-pairs exercised.
- **949/949** total, clean under Release.
- AddressSanitizer+UndefinedBehaviorSanitizer, 2 runs, clean.
- Fresh-clone verified before push.
- `git status --porcelain` confirmed only this phase's own files changed
  before commit.
