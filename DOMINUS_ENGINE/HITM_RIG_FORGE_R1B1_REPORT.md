# DOMINUS Rig Forge — Phase R1b-1 Report

**Status: implemented, tested, verified. A real, mixed result — not a
clean win, not a clean loss.** Scope set by the checkpoint that
authorized this phase: investigate whether the 5 real control-bone
anchors Phase R1a left un-derived (`root`, `hip`, `neck`, `shoulderFar`,
`shoulderNear`) can be deterministically derived from real children data,
with the same discipline as Phase R1a — hypothesis, derivation, all three
fighters, run through the real FK, quantitative validation. No
hand-authored numbers. No change to `HitmSceneBridge`. No FK replacement.

**The one-line result: `root` and `hip` need no anchor at all — proven,
not derived. `neck`, `shoulderFar`, and `shoulderNear` cannot be
deterministically derived from their one real child this way — proven to
fail, by a real, large, quantified margin, for a real, structural reason
this phase found and explains below.** This is exactly the kind of
"bring back the exact failure" result the checkpoint asked for.

## Finding 1 — `root` and `hip`: not derived, PROVEN UNNECESSARY

Every real bone directly or transitively parented under `hip` — `torso`,
`legFarU`, `legNearU`, plus each fighter's own real extra hip children
(`coatFar`/`coatNear` for Brooklyn, `tail` for Rocket,
`coatStripFar`/`coatStripNear` for Static) — resolves to the **exact same
real absolute position, to floating-point precision, regardless of what
value `root.at` and `hip.at` are given.**

This is not empirical — it is algebraic. The real algorithm's own offset
formula for a bone whose parent is a control bone is
`ox = (b.at-0.5)*spriteW - (pb.at-0.5)*spriteW`. Composing this across a
chain of control bones telescopes exactly:

```
world[hip].x  = world[root].x + (hip.at-0.5)*spriteW - (root.at-0.5)*spriteW
              = (root.at-0.5)*spriteW + (hip.at-0.5)*spriteW - (root.at-0.5)*spriteW
              = (hip.at-0.5)*spriteW                                    -- root.at cancels exactly
world[torso].x = world[hip].x + (torso.at-0.5)*spriteW - (hip.at-0.5)*spriteW
               = (torso.at-0.5)*spriteW                                 -- hip.at cancels exactly too
```

This cancellation happens because `hip`'s own real parent (`root`) is
*also* a control bone, so `hip`'s own resolution uses the SAME branch of
the real algorithm (`if(!pb.part)`) as its children's resolution does —
consistent branch usage throughout, hence exact telescoping. (`neck`,
`shoulderFar`, `shoulderNear` do NOT get this for-free cancellation —
their own real parent, `torso`, owns a drawn part, so their own
resolution uses the *other* branch — see Finding 2.)

**Proven executable, not just on paper**: `HitmRigForgeControlAnchor_
RootAndHipAnchors_AreProvenIrrelevantToEveryDescendant` runs the real,
unmodified `HitmSkeletonFk::ComputeLocalOffset` (Phase R1a's own code,
untouched) with three deliberately, wildly different placeholder
`root.at`/`hip.at` pairs (including negative and >1 values, explicitly
never claimed as real data) for all three fighters, and asserts every
real hip-descendant's fully composed absolute position is identical to
1e-9 across all three. It is.

**Practical consequence**: 2 of the 5 real control-bone anchors per
fighter (6 of 15 total) need neither authoring nor derivation. Any
placeholder — even `(0, 0)` — is provably correct for `root` and `hip`.

## Finding 2 — `neck`/`shoulderFar`/`shoulderNear`: option 1 fails, and why

### The candidate

Each of these three real control bones has **exactly one** real child in
all three fighters — verified directly from each fighter's own real
`rig.json`, not assumed:

| | `neck` | `shoulderFar` | `shoulderNear` |
|---|---|---|---|
| Brooklyn / Rocket / Static | `head` | `armFarU` | `armNearU` |

`HitmRigForgeControlAnchor::DeriveControlBoneAnchorFromSingleChild`
constructs a real, physically-motivated candidate: solve for the `.at`
value that makes the control bone's own resolved position (via the real
`else`-branch offset from its real parent part, `torso`) coincide exactly
with that one child's own real, independently-known (rect+pivot-derived,
never `.at`-derived) absolute pivot point. Physical reading: *the
shoulder joint sits exactly where the upper arm's own pivot is.*

### Why it fails when actually run through the real algorithm

The real `SkeletonSystem.build()` reads a control bone's own `.at` value
in **two incompatible coordinate spaces**, whenever that control bone's
own real parent owns a part — exactly `neck`/`shoulderFar`/
`shoulderNear`'s real, evidenced case:

- Resolving the control bone's **own** world position (`b` = the control
  bone, `pb` = `torso`, which owns a part) reads `.at` as normalized
  **within torso's own rect** (the `else` branch — what the candidate
  above solves for).
- Resolving any of **its children's** world position (the control bone
  now playing `pb`, and `!pb.part` is true because it owns no part)
  reads the *same raw stored `.at` value* as normalized in **whole-sprite
  space** (the `if` branch).

This is a real, evidenced consequence of the real algorithm's own
branch-selection rule (`if(!pb.part)`), which depends only on whether the
*parent in that call* owns a part — never on what that parent's own
ancestor looks like. One raw number, two genuinely different, real,
simultaneously-active readings.

**Quantified, for all three fighters** (`HitmRigForgeControlAnchor_
SingleChildBones_CandidateFailsRealFkRoundTrip_ByARealLargeQuantifiedMargin`
— composes the candidate through the real, unmodified `HitmSkeletonFk`,
torso → control bone → child, exactly as the real engine would):

| fighter | bone | candidate `.at` (torso-local) | real child | round-trip deviation (px, displayHeight=225) |
|---|---|---|---|---|
| Brooklyn | neck | (0.5193, 0.0204) | head | (−0.90, **58.50**) |
| Brooklyn | shoulderFar | (0.0442, 0.1705) | armFarU | (21.18, 36.43) |
| Brooklyn | shoulderNear | (0.9722, 0.1231) | armNearU | (−21.94, 43.41) |
| Rocket | neck | (0.5169, 0.0196) | head | (1.50, **71.74**) |
| Rocket | shoulderFar | (0.1340, 0.2114) | armFarU | (19.33, 50.23) |
| Rocket | shoulderNear | (0.8853, 0.2476) | armNearU | (−15.66, 46.17) |
| Static | neck | (0.4550, 0.0513) | head | (5.45, **80.37**) |
| Static | shoulderFar | (0.0433, 0.1121) | armFarU | (41.06, **71.98**) |
| Static | shoulderNear | (0.9468, 0.1757) | armNearU | (−37.09, 63.20) |

**Deviations of 15–80 real pixels on a 225px-tall character** — a
15–36% error, not Phase R1a's own bounded ~1.5%. This is not a marginal
miss; it is a clear, decisive, real failure, consistent across every one
of the 9 real (bone, fighter) cases checked. None converges.

### A corrected solve was also attempted — and rejected for a deeper reason

Before concluding failure, a second candidate was built that solves the
**combined** equation both real readings jointly impose (both branches at
once, algebra in `HitmRigForgeControlAnchor.cpp`'s own history). It
round-trips to exactly zero deviation — by construction. But it is not
usable: because Phase R1a's own convention already defines a child's
`.at` to equal its own real pivot point **exactly** whenever that
child's parent is a control bone (zero error, by definition, for `head`/
`armFarU`/`armNearU`), the combined equation **degenerates into one with
no dependency on the child's own real geometry at all** — it produced the
*identical* number, `(0.500000, 0.418367)` for Brooklyn, for `neck`,
`shoulderFar`, AND `shoulderNear`, despite their three genuinely
different real children. A formula that returns the same answer no
matter which of three different real children it's asked to derive from
is not deriving anything from that child — it was caught and rejected,
not shipped as a false positive.

**Conclusion: there is no single real number that is both (a)
physically motivated by the child's real position and (b)
self-consistent with the real algorithm's own two-space reading of a
control bone's `.at`.** Option 1 does not work for `neck`/`shoulderFar`/
`shoulderNear`, for a real, structural, now-documented reason — not
because the search wasn't thorough.

### A real, secondary, non-redemptive signal

The FAILED candidate above is still worth a second look, not as evidence
it should be trusted, but as evidence the *construction* itself is not
nonsensical: `shoulderFar.at_x + shoulderNear.at_x` lands within
1.6–1.9% of exact bilateral mirror symmetry (1.0163 / 1.0193 / 0.9901 for
Brooklyn / Rocket / Static) despite the two candidates being solved
completely independently, each from its own arm's own real geometry, with
no symmetry assumption anywhere in the formula. `neck` lands near torso's
own horizontal center and top edge in all three fighters, as a real
anatomical joint should. This is real, quantitative, and unplanned — but
it is evidence the candidate isn't garbage, not evidence it is correct.
It is checked and reported honestly as exactly that, not used to
overturn Finding 2's own failure.

## What this phase did NOT do

- Did not hand-author any of the 15 real control-bone numbers.
- Did not change `HitmSceneBridge.h/.cpp` — untouched.
- Did not replace or modify `HitmSkeletonFk`'s own Phase R1a code — the
  new module calls it, read-only, as the validation oracle.
- Did not paper over the two-space finding by silently picking one
  reading and ignoring the other, or by averaging the two candidates, or
  by introducing a correction constant to force convergence.
- Did not touch collision, new fighter mechanics, or any already-frozen
  module.

## What Phase R1b-2 (or whatever comes next) now has

1. **6 of 15 real control-bone anchors are settled, permanently, with
   certainty**: `root` and `hip`, for all three fighters, need no value
   at all. Any real Rig Forge skeleton builder can hard-code a
   placeholder for these two bones with a comment citing this proof,
   never a TODO.
2. **9 of 15 remain genuinely open**, with a real, well-understood reason
   why "derive from children" does not close them: the real algorithm's
   own two-space `.at` reading for a control bone whose parent owns a
   part. Three honest paths forward, none chosen here:
   - **Hand-author the 9 real numbers** (3 bones × 3 fighters) — the
     original Phase R1 option 2, now scoped down from 15 to 9 by
     Finding 1's own proof.
   - **A different derivation strategy** that doesn't reduce to either of
     the two equations tried here — e.g. using `rig_validation.json`'s
     own real `safe_rotation_deg`/pivot-on-opaque data, or a real
     constraint from a SECOND independent source this phase didn't
     examine (`ai.json`/`character.json` were not touched).
   - **Conclude real recursive FK is not the right target representation
     for these three joints at all**, and design Rig Forge's own output
     format to sidestep the two-space quirk entirely (e.g. storing a
     single canonical position per control bone and having Rig Forge's
     OWN poser read it consistently, rather than reproducing
     `SkeletonSystem.js`'s exact two-branch behavior byte for byte).

None of these is chosen here — that is explicitly the next checkpoint's
call, per this phase's own scope.

## Verification

- 4 new tests (`HitmRigForgeControlAnchor_RootAndHipAnchors_
  AreProvenIrrelevantToEveryDescendant`, `HitmRigForgeControlAnchor_
  SingleChildDerivation_RefusesABoneWithSeveralRealChildren`,
  `HitmRigForgeControlAnchor_SingleChildBones_
  CandidateFailsRealFkRoundTrip_ByARealLargeQuantifiedMargin`,
  `HitmRigForgeControlAnchor_FailedCandidate_
  IsStillApproximatelyMirrorSymmetric_AcrossAllThreeFighters`), all real
  fixture data, all three fighters.
- **953/953** total, clean under Release.
- AddressSanitizer+UndefinedBehaviorSanitizer, 2 runs, clean.
- Fresh-clone verification before push.
- `git status --porcelain` confirmed only this phase's own files changed
  before commit.
