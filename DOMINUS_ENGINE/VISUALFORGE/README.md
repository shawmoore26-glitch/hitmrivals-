# VISUALFORGE

A bridge system between DOMINUS's authoritative visual description
layers and future asset-creation/rendering systems.

```
VisualGenome
MaterialGenome
VisualStyleGenome
WorldHistory (via CHARACTER::VisualMemoryDeriver)
      |
      v
VISUAL FORGE
      |
      +-- CharacterBlueprint      (built)
      +-- AssetSpecification      (built, derived from CharacterBlueprint)
      +-- AnimationSpecification  (built, from real MotionGraph/AnimationSet data)
      +-- RendererPackage         (built, hash-addressed bundle of the above three)
      +-- EnvironmentBlueprint    (NOT built -- see below)
      |
      v  (v0.2)
      +-- BlueprintValidator      (built -- Valid/Invalid before a package is created)
      +-- DependencyGraph         (built -- real per-genome hashes + change detection)
      +-- ProductionSnapshot      (built -- versioned, hash-addressed, reproducible builds)
```

## v0.2: Validator, Dependency Graph, Versioned Snapshots

- **`BlueprintValidator`** (`VISUALFORGE/BlueprintValidator.h`) -- runs
  BEFORE a `RendererPackage` gets built. Two severities: `error` blocks
  (`valid=false`), `warning` doesn't -- a missing `MaterialGenome` is a
  warning (an entity genuinely having none is valid data, see Phase
  4's own "VisualGenome alone" case), a set-but-disagreeing
  `presence.style_id` is an error (real data-integrity defect). Also
  owns `IsWellFormedHash` -- the one shared definition of "looks like a
  real SHA-256 digest," reused by `DependencyGraph`'s own validation.
- **`DependencyGraph`** (`VISUALFORGE/DependencyGraph.h`) -- real
  per-genome hashes produced by REGISTRY's OWN existing compilers
  (`VisualGenomeCompiler`/`MaterialGenomeCompiler`/
  `VisualStyleGenomeCompiler`), not a second parallel hash scheme.
  `ChangedDependencies(old, new)` is a real, field-by-field comparison
  -- "if anything changes, Dominus knows what needs rebuilding" as
  actual code, not narration.
- **`ProductionSnapshot`** (`VISUALFORGE/ProductionSnapshot.h`) -- a
  named, versioned, hash-addressed build (e.g.
  `BROOKLYN_VISUAL_BUILD_001`). Version counters
  (`visual_version`/`material_version`/`animation_version`) are
  mechanically bumped by `CreateNext` ONLY when that dependency's real
  hash changed from the previous snapshot -- never guessed, never
  all-or-nothing (this engine has no way to judge whether a change is
  "big" or "small," same epistemic honesty
  `GameDesignCoherenceChecker`/`VisualStyleGenome`'s own "no auto-
  combine" discipline already established). `style_label` stays
  free-form, caller-declared text (e.g. `"HITM City v1"`) -- no
  version-bumping authority is invented for it.

## v0.3: The Gate, Provenance Chain, Lifecycle, Rebuild Authority

**LAW — Visual Package Integrity.** A `RendererPackage` cannot exist
unless its source `CharacterBlueprint` has passed `BlueprintValidator`.
`RendererPackageForge::Build` now validates FIRST, internally, and
returns a `RendererPackageResult` -- there is no code path back to a
`RendererPackage` without `result.ok` having been true first. Invalid
blueprint -> no package. Not a package with a warning.

- **`BlueprintValidationArtifact`** (`VISUALFORGE/
  BlueprintValidationArtifact.h`) -- the durable version of
  `ValidationResult`: named checks (`VisualGenome`/`MaterialGenome`/
  `StyleReference`/`History`), each `PASS`/`FAIL`, hash-addressed. "The
  package remembers this was validated" -- literally, via
  `validation_artifact_hash`.
- **`RendererPackage.depends_on`** -- the full `DependencyGraph`
  embedded directly on the package: real per-genome hashes, not
  re-derived. "Every visual artifact has ancestry" -- checkable.
- **`SnapshotState`** (`VISUALFORGE/ProductionSnapshot.h`) -- `Created
  -> Validated -> Approved -> Released`, same shape as the Entity
  lifecycle. Two transitions are mechanical (`AdvanceToValidated` reads
  a fact already on the snapshot); two are deliberately NOT
  (`Approve`/`Release` require an explicit human decision -- this
  engine has no authority to decide a build is creatively ready, same
  epistemic boundary `GameDesignCoherenceChecker` already draws).
- **`DependencyGraphForge::PlanRebuild`** -- a real `RebuildPlan` over
  Visual Forge's own actual outputs (`RendererPackage`/
  `AssetSpecification`/`AnimationSpecification`) -- no fabricated
  "material cache" or other system that doesn't exist in this engine.

Visual Forge does not replace the renderer. It converts validated
reality descriptions into structured packages a future graphics
system, procedural generator, artist, or runtime engine can consume.
`RendererPackage` is inert data with zero consumer today -- same
honesty GRAPHICS/README.md itself already states ("this directory is a
placeholder until its phase gate opens").

## Dependency direction

`VISUALFORGE` depends on `CHARACTER/Genome` only (`VisualGenome`,
`MaterialGenome`, `VisualStyleGenome`, `VisualMemorySummary`) and, for
`AnimationSpecification`, on the same `CHARACTER::MotionGraphComponent`/
`AnimationSetComponent`/`ANIMATION::MotionGraph` chain `RigBinder`
already established -- never `GRAPHICS` (an ungated placeholder per
Law 6; Visual Forge is explicitly NOT graphics implementation, so it
does not live inside `GRAPHICS/` and does not touch that gate). For
hashing (`RendererPackage`), it reuses `REGISTRY::Hash::Sha256`
directly -- the same narrow, pure-utility borrowing `COMBAT::Provenance`
already established, not a dependency on `REGISTRY`'s higher-level
genome-specific types.

## Why EnvironmentBlueprint is not built

The directive's own diagram names `Environment Blueprint` as one of
five outputs. It genuinely cannot be built honestly today: there is no
`EnvironmentGenome`, no environment-scoped `VisualGenome`/
`MaterialGenome`/`VisualStyleGenome` binding, and no real visual
description of an environment anywhere in this engine.
`COMBAT::DestructionZone`/`EnvironmentBounds` exist, but they describe
gameplay collision geometry (name/x/y/radius), not visual identity --
using them to fabricate an `EnvironmentBlueprint` would mean inventing
schema with no real backing, exactly the kind of fake system this
engine has declined to build for 20+ phases running (no fabricated
Health component, no fake ecosystem generator, no invented Style
Fusion percentage). Flagged here, not hidden, not worked around with
placeholder data.

## Visual Acceptance Harness (the visual authority model)

`VISUALFORGE/AcceptanceCertificate.h` + `VISUALFORGE/
RendererPackageAcceptanceHarness.h` -- the same real, generated
certificate discipline RIG's `CharacterAcceptanceHarness` proved for
Brooklyn, applied honestly to `RendererPackage`. Four real checks
(`BlueprintValidity`/`DependencyIntegrity`/`PackageDeterminism`/
`ProvenanceCompleteness`), each reusing or cross-checking real,
already-existing VisualForge machinery -- plus, now that `GRAPHICS`
exists (see below), two REAL rendering checks (`Renderable`/
`RenderDeterminism`) and a `RenderedOutput` section that stays
`NOT_DECLARED` because there is still no rasterizer. `AcceptanceCertificate`
tracks `structurally_sound` and `renderable` as two SEPARATE booleans
-- proven independent of each other directly, not just documented as
different. See `dominus-cli visual-acceptance` and the roadmap's own
entry for why `VisualProfile` (an external-format migration layer,
mirroring `RigProfile`) was deliberately not built either: there's no
real legacy visual asset in this engine to migrate from, unlike
Brooklyn's real legacy skeleton.

## ProductionSnapshot Acceptance Lifecycle

`ProductionSnapshot`'s own lifecycle now has a real `Accepted` state,
gated exactly like RIG's `RigProfileLifecycle::AdvanceToAccepted`: a
snapshot must be `Approved`, then `SnapshotLifecycle::AdvanceToAccepted`
requires a real `AcceptanceCertificate` whose `structurally_sound` AND
`renderable` are BOTH true. **There is no `Active` state reachable
anywhere in this codebase** -- `SnapshotState::kActive` exists in the
enum for the real, future day a rasterizer earns it, but
`SnapshotLifecycle` has no method of any name that produces it. This
is the directive's own critical rule ("`STRUCTURALLY_SOUND` must never
become `ACTIVE`"), enforced by the type system rather than a comment.

## GRAPHICS (real, minimal, gate opened)

See `GRAPHICS/README.md` for the full, honest scope: a deterministic
logical frame compiler (`Scene`/`Camera`/`FrameCompiler`/`Frame`), no
rasterizer, no pixels, no GPU. `VISUALFORGE -> GRAPHICS` is the only
dependency direction -- `GRAPHICS` has zero knowledge of any
`VISUALFORGE` type.

## Future pipeline

```
Visual Forge -> Graphics Engine -> Final Pixels
```

The first arrow now has a real, if deliberately minimal, second half:
`RendererPackage -> Scene -> Frame` (logical, deterministic). `Final
Pixels` -- an actual rasterizer -- remains the real, separate, still-
unbuilt piece.
