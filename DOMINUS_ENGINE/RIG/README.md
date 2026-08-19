# RIG — DOMINUS RIG v1.0

"This is the skeleton contract. Make the asset conform to it" -- the
inverse of "generate a skeleton that fits this asset."

```
CanonicalSkeleton.h       -- the contract: 24 mandatory bones + extension allow-list
RigAuthorityValidator.h   -- the enforcement: checks a real Skeleton against it
```

## What this is, precisely

A real, standalone, hash-free (no REGISTRY dependency needed -- this is
structural validation, not content-addressing) validator that checks a
loaded `animation::Skeleton` against a fixed canonical bone hierarchy
and reports `kRigged` (true) or a list of concrete, named issues
(false).

Of the 12 checks named in the founding directive, **7 are real and
implemented**: unique bone IDs, required bones present, canonical
parent hierarchy, no orphan bones, no duplicate authority, valid bind
pose, retarget mapping validity. **5 are explicitly NOT_DECLARED**,
every time, in every report: inverse bind matrices, skin weights,
weight normalization, socket bindings (as a first-class system, beyond
structural bone checks), deformation test. This engine has no mesh,
skin-weight, or inverse-bind-matrix data model anywhere --
`ANIMATION::Skeleton` is bone transforms only. Faking a PASS for a
check with nothing real behind it would be exactly the fabricated-
evidence failure mode this engine has refused for 20+ phases. The gap
is named, not hidden.

## What this is NOT (yet)

- **Not wired into `RigBinder::Bind`.** Brooklyn's real skeleton
  (`root`/`torso`/`head`/`arm_r`/`arm_l`/`leg_r`/`leg_l`, from the
  prior "bone rig fix" phase) does not conform to this contract --
  different names, no spine subdivision, no
  clavicle/upperarm/forearm/hand breakdown. Making `RigAuthorityValidator`
  a hard gate on `RigBinder::Bind` today would immediately fail
  Brooklyn's own fixture and break every one of the 35+ tests that
  depend on it. This validator is opt-in and standalone until a real
  migration of Brooklyn/`generic_biped` onto the canonical hierarchy
  happens -- a genuine, separate, larger effort (every move/hurtbox/
  hitbox/animation-clip fixture that references the old bone names
  directly would need updating too), not attempted here.
- **Not a multi-DCC export pipeline.** The founding directive's own
  diagram names Unreal/Unity/Godot/Blender/Maya as consumers. This
  engine is a headless C++ simulation core with no connection to any
  of those tools -- the diagram is the aspirational shape of a FUTURE
  retarget/export layer, not something this phase builds or claims to
  build toward with working code.
- **Not `RIG PROFILE`/`MOTION LIBRARY`/multi-character retarget
  authority.** `ANIMATION::RetargetMap` already exists (proven via the
  `generic_biped` retargeting tests) and this validator checks it when
  supplied -- but a full "generate motion once, retarget across
  Brooklyn/Rocket/Static/future fighters" system is real, separate,
  future work.

## The real, verified finding

Running `RigAuthorityValidator::Validate` against Brooklyn's actual,
current skeleton fails, honestly: 24/24 canonical bones missing (none
of Brooklyn's 7 real bone names match the canonical set), plus every
one of Brooklyn's own bone names (`torso`, `arm_r`, `arm_l`, `leg_r`,
`leg_l`, `head`) flagged under "no duplicate authority" for not being a
canonical or recognized extension name. This is the real, verified
parallel to "the rig doesn't conform to a real contract" -- proven
against actual current data, not asserted.

## v1.1: RigProfile — the migration boundary
`is_rigged=false` on its own doesn't tell you what to DO about it.
`RigProfile` (`RIG/RigProfile.h`) is the explicit, authored bridge
between a legacy skeleton and the canonical contract -- a real list of
`legacy_bone -> canonical_bone` pairs, never inferred, never
auto-generated. `RigProfileValidator` (`RIG/RigProfileValidator.h`)
turns the diagnostic from a single boolean into five real categories:

- **mapped** -- legacy bones with a real, resolved canonical
  correspondence.
- **missing** -- canonical bones no mapping ever addresses. Never
  invented to make a report look better -- genuinely absent, named.
- **extra** -- legacy skeleton bones the profile doesn't map to
  anything.
- **hierarchy_conflicts** -- a mapped bone's REAL legacy parent doesn't
  correspond (via the same profile) to its canonical bone's required
  parent.
- **unresolved** -- a mapping entry referencing a legacy bone that
  doesn't exist on the skeleton, or a `canonical_bone` name that isn't
  real.

**`RigProfileLifecycle`**: `UNMAPPED -> MAPPED -> VALIDATED ->
COMPATIBLE -> ACTIVE`. The first two transitions are mechanical
(`AdvanceToMapped` requires at least one real mapping;
`AdvanceToValidated` requires a real `RigProfileReport::valid`).
`COMPATIBLE`/`ACTIVE` are deliberately NOT mechanical -- this engine
has no automated way to prove full behavioral equivalence between a
legacy rig used through a profile and a truly canonical one, so both
require an explicit, non-empty, caller-supplied reason. Same
epistemic-humility pattern `SnapshotLifecycle::Approve` already
established in VisualForge v0.3.

**Proven both directions, with real fixtures:** a fresh 1:1 identity
profile on the already-conformant `canonical_biped` skeleton reaches
the full lifecycle through `ACTIVE`. Brooklyn's real, honestly-authored
profile (mapping each of his 7 simplified bones to their closest
single canonical analog -- `torso->chest`, `arm_r->hand_R`, etc.)
correctly, legitimately stops at `MAPPED`: 17 canonical bones
genuinely missing, 6 real hierarchy conflicts (every mapped bone
except root itself), because his rig really is that much simpler than
the canonical hierarchy. The validator does not paper over this to let
the lifecycle advance -- migrating Brooklyn to actually conform is
real, separate, future work (RIG Phase 4 in the roadmap), not
attempted here.

## Phase 4: Brooklyn migrated (COMPATIBLE, not ACTIVE)

`brooklyn_canonical.skel.json` + 13 migrated clips + migrated
hurtboxes/moves now exist, proven mathematically and numerically
equivalent to Brooklyn's original 7-bone rig at every terminal bone,
across every real clip, at multiple time samples each -- 546 individual
position/rotation comparisons, all matching to floating-point
precision (see `tests/rig/test_brooklyn_migration.cpp` and `dominus-cli
brooklyn-migration`). His `RigProfile` reaches `COMPATIBLE` with a
real, evidence-citing reason. Production `brooklyn.dominus` is
untouched -- `ACTIVE` (cutting production over) is a deliberate,
separate, not-yet-made decision.

## Phase 5: Acceptance harness — Brooklyn is ACTIVE

`RIG/AcceptanceCertificate.h` + `RIG/CharacterAcceptanceHarness.h` run
five real checks (Skeleton/Animation/Combat/Runtime/Determinism) and
generate a certificate that's never hand-typed. The lifecycle now
requires it: `RigProfileLifecycle::AdvanceToAccepted`/`DeclareActive`
both mechanically gate on a real, passing certificate hash -- `ACTIVE`
can no longer be manually flipped. Brooklyn's real certificate passes
all 5 sections (see `dominus-cli brooklyn-acceptance`), and his
`RigProfile` reaches `ACTIVE` from that evidence. Production
`brooklyn.dominus` is still untouched -- `brooklyn_canonical.dominus`
is a new, separately-validated fixture; making it the production
reference is a further, deliberate deployment decision, not made here.

