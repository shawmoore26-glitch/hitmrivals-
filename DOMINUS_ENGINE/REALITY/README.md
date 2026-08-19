# REALITY -- the Reality Compiler

## Status: EXECUTABLE (Phase 2 complete)

Brooklyn now reaches `EXECUTABLE`, reproducibly. This section replaces
an earlier version of this file that documented a genuine `REJECTED`
result -- kept below, unedited, as the record of what was actually
found and why it mattered.

```
$ dominus-cli reality-compile tests/fixtures
STAGES:
  RIG:           VALIDATED  (certificate_hash=... overall_pass=true)
  VISUALFORGE:   VALIDATED  (certificate_hash=... structurally_sound=true renderable=true)

PIPELINE STATE: EXECUTABLE
ARTIFACT HASH:   4c653add1adf8fa20766ba05fe98be885dcd54b05ba72a166834bad2f7e164c8

EXECUTION CERTIFICATE:
  Authority.RIG:                PASS
  Authority.VisualForge:        PASS
  Execution.Runtime:            PASS  (load, bind, motion graph, collision, serialize->reload->replay)
  Execution.Determinism:        PASS  (identical context_hash/result_hash across two independent runs)
  Registration:                 PASS
  ExecutionProof.ProfileActive: PASS  (RigProfileLifecycle reached ACTIVE through its own unmodified gate)
  EXECUTABLE:                   true

RECONSTRUCT -> REPRODUCE:
  Run A artifact_hash: 4c653add1adf8fa20766ba05fe98be885dcd54b05ba72a166834bad2f7e164c8
  Run B artifact_hash: 4c653add1adf8fa20766ba05fe98be885dcd54b05ba72a166834bad2f7e164c8
  A == B:              true
```

### What closed the gap -- audited before touching anything

Before binding anything, the legacy `brooklyn.dominus`'s real
VisualForge data was traced end to end:

- **What `VisualGenome` already exists:** `brooklyn_visual.json` --
  real, authored `form`/`surface`/`presence` fields (silhouette,
  clothing material, aura, `style_id: "STYLE-URBAN-COMBAT"`). Not a
  stub.
- **`material_genome`:** real -- `brooklyn_jacket_material.json`,
  `MAT-JACKET-001`, urban leather, wear/weather properties.
- **`visual_style_genome`:** real -- `brooklyn_visual_style.json`,
  `STYLE-URBAN-COMBAT`, matching the visual genome's own
  `presence.style_id` exactly (a real cross-reference, not a
  coincidence).
- **Source/artifact identity:** legacy `brooklyn.dominus`'s own
  `provenance` block: `creator: "Shawn"`, `creation_method:
  "hitm-character-forge"`, a real 64-character `creation_hash`.
- **Dependencies:** none of the three files reference a bone name, a
  skeleton convention, or anything RIG's canonical migration touched
  -- confirmed by reading all three files in full, not assumed.
  `VisualGenomeCompiler`/`MaterialGenomeCompiler`/
  `VisualStyleGenomeCompiler` hash pure content; nothing in
  `DependencyGraph`/`RendererPackage` couples to skeleton topology.
- **Can they legitimately attach to the canonical artifact:** yes --
  RIG's own migration certificate already proves `brooklyn_canonical`
  is the same character with zero behavioral drift (bind-pose and
  13/13 animation-clip equivalence, `CharacterAcceptanceHarness`).
  Appearance data that never referenced the old skeleton in the first
  place needs no re-authoring to survive a skeleton migration.
- **Hashes/provenance after binding:** `DependencyGraphForge::Build`
  and `RendererPackageForge::Build` both take `entity_id` as an
  explicit, separate input from genome content -- binding the same
  genome files under `object_id: "brooklyn_canonical"` instead of
  `"brooklyn"` produces a real, *different*, correctly-scoped
  `package_hash`/certificate than the legacy run's -- not a collision,
  not an alias. Confirmed by running `visual-acceptance` against both
  files and comparing certificate hashes.

### What actually changed on disk

`tests/fixtures/brooklyn_canonical.dominus` gained three new top-level
keys -- `visual_genome`, `material_genome`, `visual_style_genome` --
each a `{"ref": "..."}` pointing at the exact same, pre-existing,
already-real files legacy `brooklyn.dominus` already used. This is the
same reference mechanism every other domain in this file already uses
(`skeleton: {ref: ...}`, `animations: [{name, ref}]`) -- not a copy of
data into the file, a binding to an existing artifact by reference.
Nothing was invented: no new `style_id`, no new material, no new
presence values. A `provenance` block was also added, honestly
recording what happened: `creation_method:
"rig_migration_visual_carry_forward"`, `parent_entities: ["brooklyn"]`
-- an explicit lineage record, not a claim that this is a fresh
creative act.

`tests/reality/test_reality_compiler.cpp` was updated to assert the
new, real outcome (`EXECUTABLE`, matching artifact hashes across two
independent `CompileBrooklyn` runs) -- the tests that previously
asserted the honest `REJECTED` outcome were replaced, not weakened;
see git history / the original text below for exactly what they
proved before this phase.

### What's still genuinely unresolved, now that Brooklyn is EXECUTABLE

- **No dependency graph / auto-invalidation yet.**
  `CompilationContext.dependency_identities` records which certificate
  hashes this artifact depends on, but nothing watches those hashes
  for change and triggers a rebuild. Per the directive: this is the
  correct next phase now that the first vertical slice is real, not
  before.
- **Brooklyn only, still.** Nothing here is generalized to a second
  character yet. That should happen once the dependency graph exists,
  so the graph itself is proven on more than one shape.
- **No CI/build-button integration.** `VALIDATION::BuildPipeline`
  discovers and validates every `.dominus` in a project; REALITY
  doesn't plug into that yet.
- **RenderedOutput is still `NOT_DECLARED`.** `EXECUTABLE` here means
  "every domain that must agree, agrees, on one real object, proven
  twice" -- it does not mean pixels exist. There is still no
  rasterizer anywhere in this engine (see `GRAPHICS/README.md`); the
  VisualForge `renderable` claim is exactly as scoped as it always
  was.

---

## Milestone 3: Dependency Sovereignty / Impact Graph

Built directly on top of Milestone 2's `EXECUTABLE` result, per the
explicit sequencing call: no dependency graph while the first subject
was still `REJECTED`. Brooklyn reached `EXECUTABLE` first; this is
what came after.

### The rule, and how this file follows it

"No subsystem should have to manually know what depends on it. The
graph should derive that from actual artifact references and hashes."
Concretely, in `REALITY/DependencyGraph.h` and
`REALITY/RealityCompiler.cpp::BrooklynDependencyEdges`:

- **Every node hash is real**, never invented. Source-file nodes
  (`brooklyn.skeleton`, `brooklyn.animation`, `brooklyn.combat`) are
  `Sha256` of the actual raw bytes of the actual files RIG's own
  `CharacterAcceptanceHarness` functions take as parameters. Genome
  nodes (`brooklyn.visual_genome`, `brooklyn.material_genome`,
  `brooklyn.visual_style_genome`) reuse the exact hashes
  `VISUALFORGE::DependencyGraphForge::Build` already computes via
  `REGISTRY::VisualGenomeCompiler`/`MaterialGenomeCompiler`/
  `VisualStyleGenomeCompiler` -- not recomputed a second way. A test
  (`BrooklynDependencyGraph_VisualGenomeHash_MatchesIndependentGroundTruth`)
  cross-checks this against an entirely separate call to the same
  compiler, so the graph can't silently drift from ground truth.
- **Every edge is a cited fact, not a guessed architecture.**
  `CheckAnimation(..., canonicalSkelPath, ...)` and
  `CheckCombat(canonicalSkelPath, ...)` both take the skeleton as an
  explicit parameter -- that's the `skeleton -> animation` and
  `skeleton -> combat` edges. `CharacterBlueprintForge::Build` takes
  `visual` (required), `material`/`visualStyle` (optional) -- that's
  the three `*_genome -> visualforge_certificate` edges. Every edge in
  `BrooklynDependencyEdges()` has a one-line citation to the real
  function signature that makes it true.
- **`ImpactAnalyzer` knows nothing about Brooklyn.** It's a generic
  reverse-reachability BFS (`AffectedBy`) and a generic hash diff
  (`ChangedNodes`/`ImpactOfChanges`) that operate on any
  `DependencyGraph`. Proven independent of Brooklyn with pure
  synthetic-graph unit tests
  (`ImpactAnalyzer_AffectedBy_TransitiveChain`, etc.) in addition to
  the Brooklyn-specific ones.

### Proven on a real, mutated file -- not asserted

`tests/reality/test_dependency_graph.cpp` makes a real, throwaway copy
of `tests/fixtures/` (never touches the real fixtures), mutates one
real file's actual bytes, rebuilds the graph from the mutated copy,
and checks the reported impact:

```
$ dominus-cli reality-impact tests/fixtures brooklyn.skeleton
IMPACT ANALYSIS -- if 'brooklyn.skeleton' changes:
  [invalidated]  brooklyn.animation
  [invalidated]  brooklyn.combat
  [invalidated]  brooklyn.reality_artifact
  [invalidated]  brooklyn.rig_certificate
  [changed]      brooklyn.skeleton

$ dominus-cli reality-impact tests/fixtures brooklyn.visual_genome
IMPACT ANALYSIS -- if 'brooklyn.visual_genome' changes:
  [invalidated]  brooklyn.reality_artifact
  [invalidated]  brooklyn.visualforge_certificate
  [changed]      brooklyn.visual_genome
```

A skeleton mutation correctly reaches animation/combat/rig_certificate
but never touches the visual branch; a visual_genome mutation
correctly reaches visualforge_certificate but never touches RIG's
branch. Both directions were checked -- a graph that only worked one
way would be a coincidence, not a proof.

New CLI commands: `dominus-cli reality-graph <fixtures_dir>` prints
the full node/edge list; `dominus-cli reality-impact <fixtures_dir>
<node_id>` runs the traversal live. `RealityCompiler::
BuildBrooklynDependencyGraph` builds the graph without paying for a
full RIG+VisualForge acceptance run (useful for fast before/after
diffing) -- its certificate/artifact node hashes stay honestly empty
in that mode, since computing them for real requires the harness run;
`CompileBrooklyn` fills the whole graph, including those, as a normal
part of a full compile.

### What Milestone 3 deliberately does NOT do

Per the directive's own diagram, `CHANGE -> DEPENDENCY GRAPH -> IMPACT
ANALYSIS` is built and proven. `INVALIDATE AFFECTED ARTIFACTS ->
RECOMPILE ONLY WHAT IS REQUIRED -> REVALIDATE -> REREGISTER ->
EXECUTABLE` is NOT -- there is no automatic trigger that watches a
file, notices its hash changed, and re-runs `CompileBrooklyn` for the
affected subset. `ImpactAnalyzer` can tell you what a change would
affect; nothing here yet acts on that answer. That's real, scoped,
separate future work (a "Milestone 4"), and generalizing this graph
past Brooklyn to a second character is the step after that -- in that
order, matching the same "prove one subject first" discipline this
whole file has followed since Milestone 1.

---

## Milestone 4: Selective Recompilation ("I can actually make only those things change")

Built as an explicit, separate adapter on top of Milestone 3, per the
directive's own architectural separation: *Milestone 3 = "I know what
must change." Milestone 4 = "I can actually make only those things
change."* Nothing in Milestone 3 was redesigned to build this --
`REALITY/DependencyGraph.h` (the `DependencyGraph` struct and
`ImpactAnalyzer`) is byte-for-byte unmodified. `ImpactAnalyzer` still
does not execute anything; it is only ever called read-only, the same
as it was in Milestone 3.

```
FILE CHANGE
    |
AUTHORITATIVE HASH CHANGE     (RealityRegistry vs the live filesystem)
    |
ImpactAnalyzer::AffectedBy    (Milestone 3, unmodified, read-only)
    |
EXACT INVALIDATION SET
    |
DEPENDENCY / TOPOLOGICAL ORDER (new, local Kahn's-algorithm helper --
    |                           not added to ImpactAnalyzer)
CANONICAL COMPILER            (the SAME real internal::CompileRig /
    |                          CompileVisualForge / ComputeRealityArtifactHash
    |                          CompileBrooklyn already uses)
VALIDATION                    (each node's own real pass/fail)
    |
REGISTRY UPDATE               (RealityRegistry::Save -- only the nodes
                                actually recompiled are touched)
```

### New files

- `REALITY/BrooklynDomainCompilers.h/.cpp` -- a pure extraction.
  `CompileRig`/`CompileVisualForge`/`ComputeRealityArtifactHash` moved
  out of `RealityCompiler.cpp`'s anonymous namespace into a shared,
  internal header so `RealityRebuilder` calls the *exact same* real
  compilers `CompileBrooklyn` does -- never a second, possibly
  diverging copy. Verified behavior-preserving: `CompileBrooklyn`'s
  output (artifact hash, certificate hashes, everything) was
  byte-identical before and after this move, confirmed by the full
  pre-existing test suite passing unmodified.
- `REALITY/RealityRegistry.h/.cpp` -- a small, real, on-disk JSON
  ledger (`CORE::json::Value`, the same round-trip machinery
  `DominusSerializer` already trusts) of the last known-good hash per
  node. This is the "REGISTRY UPDATE" step, made real.
- `REALITY/RealityRebuilder.h/.cpp` -- the adapter itself.
  `RebuildFromChange(fixtureDir, registryPath, nodeId)`.

### The rule, followed literally

`nodeId` must name a real **source-file** node (skeleton, animation,
combat, or one of the three genomes) -- a certificate or artifact node
cannot itself be "told" it changed; nothing produces those except a
real compiler run, so `RealityRebuilder` refuses that input outright
(`RealityRebuilder_RefusesToTreatACertificateNodeAsTheOriginOfAChange`).
The node's *live* hash (freshly read from disk, via
`RealityCompiler::BuildBrooklynDependencyGraph`) is compared against
the registry's last known-good value for it -- a real diff, not an
assumption that naming a node means it changed. If they're equal:
`RealityRebuilder` reports a real no-op and recompiles nothing.

When they differ, the invalidation set comes from `ImpactAnalyzer::
AffectedBy` verbatim. Each node in that set is recompiled by exactly
one real function -- source nodes are re-hashed from disk (there is no
"compiler" for a raw source); `brooklyn.rig_certificate` calls
`internal::CompileRig`; `brooklyn.visualforge_certificate` calls
`internal::CompileVisualForge`; `brooklyn.reality_artifact` recomputes
the artifact hash from whichever certificate hashes are current (freshly
recompiled this pass, or trusted from the registry if untouched -- the
same "trust unaffected dependencies" discipline `VISUALFORGE::
ProductionSnapshotForge` already established). The loop stops
immediately, honestly, the moment any one node fails its own real
validation -- nothing downstream is attempted. A final check confirms
the set actually processed exactly equals `ImpactAnalyzer`'s own
invalidation set before anything is reported as success -- catching an
implementation bug that skipped or added a node, not just trusting the
loop ran correctly.

### The acceptance test, proven both directions, live from the CLI

```
$ dominus-cli reality-rebuild tests/fixtures /tmp/registry.json brooklyn.skeleton
  (after mutating brooklyn_canonical.skel.json)
RECOMPILED, IN TOPOLOGICAL ORDER:
  brooklyn.skeleton                 PASS
  brooklyn.animation                PASS
  brooklyn.combat                   PASS
  brooklyn.rig_certificate          PASS
  brooklyn.reality_artifact         PASS
[result] OK -- rebuilt 5 node(s) from 'brooklyn.skeleton', all passed

$ dominus-cli reality-rebuild tests/fixtures /tmp/registry.json brooklyn.visual_genome
  (after mutating brooklyn_visual.json)
RECOMPILED, IN TOPOLOGICAL ORDER:
  brooklyn.visual_genome            PASS
  brooklyn.visualforge_certificate  PASS
  brooklyn.reality_artifact         PASS
[result] OK -- rebuilt 3 node(s) from 'brooklyn.visual_genome', all passed
```

A skeleton mutation never touches `visualforge_certificate` or any
genome node -- proven not by a comment but by the fact that
`internal::CompileVisualForge` is never called (its node id never
appears in `recompiled_order`) when the invalidation set doesn't
include it. Same in reverse for a visual_genome mutation: `internal::
CompileRig` is never called. `tests/reality/test_reality_rebuilder.cpp`
also proves: bootstrap with no prior registry does one real full
compile to establish a genuine baseline (there's no way to know a
certificate's real hash without running its real check); a truly
unreadable source stops the rebuild at the very first node, before
any certificate compiler is even reached; and topological order is
actually respected (skeleton is processed before anything that depends
on it).

### What Milestone 4 still deliberately does not do

No automatic file watcher -- `reality-rebuild` is an explicit command,
exactly as scoped. No CI/build-button integration. No generalization
past Brooklyn. Any of those would be automation stacked on a still-only-
one-subject foundation; the directive's own sequencing rule (prove one
subject, then automate, then generalize) applies here the same way it
did for Milestones 1-3.

---

## Milestone 5: Transactional Trustworthiness ("can the registry survive reality?")

A harder question than "does selective recompilation work" -- does
`RealityRegistry` ever falsely claim an artifact is current? Answered
by construction plus real, adversarial tests, not by assertion. No
watcher was built this phase, on purpose -- automating a primitive
before proving it's trustworthy under real interruption would have
been exactly the wrong order.

### What changed

`RealityRegistry::Save` is now atomic: it writes to a sibling
`<path>.tmp` file and `rename()`s it over the real path, rather than
truncating the real file in place. A failure or interruption at any
point before the rename leaves whatever was already at the real path
completely untouched -- never a half-written or corrupted registry
masquerading as current. `RealityRebuilder` itself needed no changes:
it was already only ever calling `Save()` once, at the very end, after
every node in the invalidation set had already passed -- so a rebuild
that fails partway was already, by construction, never going to write
anything.

### Proven, in `tests/reality/test_reality_transactional.cpp`

- **No partial write on failure.** A real, isolated VisualForge-domain
  failure (see below) is triggered, and the on-disk registry is
  byte-compared before and after the failed attempt -- identical.
- **Never falsely claims current.** After a failed rebuild, a second,
  completely fresh call for the same node still reports
  `change_detected=true` -- the registry doesn't "forget" a pending
  change just because a prior attempt failed. Fixing the underlying
  issue and calling the exact same primitive a third time succeeds.
- **Order-independence.** The same two real mutations (a skeleton byte
  change, a visual_genome semantic content change) applied to two
  separate fixture copies, rebuilt skeleton-then-visual on one and
  visual-then-skeleton on the other -- the two final registries are
  byte-identical. Two independent branches converging to the same
  fixed point regardless of order is the real commutativity proof;
  getting it right in only one order would have been a coincidence.
- **No cross-branch corruption.** A skeleton-branch rebuild succeeds
  and persists; a *later*, unrelated visual-branch rebuild is then
  made to fail for real. The already-persisted rig_certificate and
  reality_artifact hashes are confirmed unchanged by the later
  failure -- a partial multi-branch mutation state doesn't let one
  branch's failure leak into another's already-correct record.
- **"Process restart."** Every `RebuildFromChange` call is already a
  pure function of (fixture dir, registry path, node id) with zero
  static or cached state -- proven explicitly with scoped, fully
  independent call sequences standing in for separate process
  invocations.
- **Atomic `Save()`, proven adversarially.** The `.tmp` sibling path is
  blocked by creating a real directory at that exact path, forcing the
  internal write step to genuinely fail before any `rename()` is
  attempted. The real registry file is confirmed byte-identical to
  before the failed `Save()` call.

### A real discovery, not a bug: the isolated-failure trigger

Finding a failure mode that breaks *only* the VisualForge branch --
without also breaking `RIG`'s `rig_certificate` -- took real
investigation, not a guess. `CharacterAcceptanceHarness::CheckRuntime`
calls the full `RigBinder::Bind`, and a genuinely broken visual-genome
*ref* (a missing file) makes `Bind` fail outright -- which fails
`CheckRuntime`, which fails `rig_certificate` too. That's a real,
previously undocumented coupling: RIG's own Runtime check depends on
every bound ref resolving, including the visual ones, not just the
skeleton/animation/combat ones `REALITY/DependencyGraph.h`'s edges
currently name. (Flagged here, not silently fixed -- see "still
unresolved" below; changing Milestone 3's edge model was explicitly
out of scope for this phase.)

The failure trigger these tests actually use instead is a genuine,
pre-existing engine gate that doesn't touch ref resolution at all:
`BlueprintValidator`'s `style_reference_matches` check, which fails
hard if `VisualGenome.presence.style_id` doesn't match the attached
`VisualStyleGenome.style_id`. Both files still load fine (`RigBinder::
Bind` succeeds, `rig_certificate` stays genuinely passing) -- only
`RendererPackageForge::Build`'s validation gate fails, a real, cleanly
isolated VisualForge-only failure.

### Genuinely unresolved, flagged not hidden

- **The RIG/VisualForge ref-resolution coupling found above.** RIG's
  `rig_certificate` can fail for a visual-domain reason (a broken
  visual/material/style ref), which `REALITY/DependencyGraph.h`'s
  current edge list doesn't represent. Correcting this would mean
  adding edges from the three genome nodes to `rig_certificate` too --
  real, scoped future work, deliberately not done as a side effect of
  a transactional-trustworthiness phase.
- **Still no automatic trigger.** Everything above proves the registry
  is trustworthy to build a watcher on top of, later. It still doesn't
  build one.
- **Still Brooklyn only.**

---

## Milestone 6: Formalizing the Runtime Coupling

Milestone 5 found the RIG/VisualForge coupling above and deliberately
left it unfixed -- correcting Milestone 3's edge model was out of
scope for a transactional-trustworthiness phase. This phase closes it,
per explicit direction: "formalize the dependency graph so it
represents the actual runtime coupling."

### The fix

Three edges added to `BrooklynDependencyEdges()` in `REALITY/
RealityCompiler.cpp`:

```
brooklyn.visual_genome       -> brooklyn.rig_certificate
brooklyn.material_genome     -> brooklyn.rig_certificate
brooklyn.visual_style_genome -> brooklyn.rig_certificate
```

Each cites the exact real call that makes it true:
`RIG::CharacterAcceptanceHarness::CheckRuntime` and `CheckDeterminism`
both call the FULL `character::RigBinder::Bind(obj, baseDir)` on
`brooklyn_canonical.dominus` -- and `Bind` resolves every bound ref on
that object, including the three genome refs, not just skeleton/
animation/combat. If `Bind` fails to resolve any one of them, the
section reports `"RigBinder failed: ..."` and fails outright, which
fails `rig_certificate.overall_pass` -- regardless of whether
Skeleton/Animation/Combat individually still pass.

This is intentionally conservative, matching how any real build system
treats a changed input: `ImpactAnalyzer` has no way to know in advance
whether a *specific* genome edit will trip `Bind`'s ref-resolution or
not (a content-only edit, like a style_id change, won't; a broken ref
will), so it must not assume a given edit is safe. "This input changed"
is grounds to re-verify the step that reads it, full stop.

### What this changes about the graph's shape

The coupling is asymmetric, matching the real code exactly:
skeleton/animation/combat still have zero real coupling into the
visual branch (nothing in VisualForge's build path reads them). But
visual_genome/material_genome/visual_style_genome now correctly reach
**both** `visualforge_certificate` and `rig_certificate`, because
`RigBinder::Bind` is one function that resolves everything at once.

Live, before and after this phase:

```
$ dominus-cli reality-impact tests/fixtures brooklyn.visual_genome
  (before)                                (after)
  [changed]      visual_genome            [changed]      visual_genome
  [invalidated]  visualforge_certificate  [invalidated]  visualforge_certificate
  [invalidated]  reality_artifact         [invalidated]  rig_certificate
                                           [invalidated]  reality_artifact
```

`dominus-cli reality-rebuild` now genuinely calls `internal::
CompileRig` for a visual_genome mutation -- confirmed live: `brooklyn.
rig_certificate PASS` appears in the recompiled output where it never
did before this phase.

### Tests corrected, not weakened

`BrooklynDependencyGraph_VisualGenomeAffectsVisualForgeCertificateOnly`
asserted an isolation claim that was never actually true -- renamed to
`...AffectsBothVisualForgeAndRigCertificate` and rewritten to assert
the real coupling. Same for the matching `ImpactAnalyzer_Real
VisualGenomeMutation_*` and `RealityRebuilder_VisualGenomeMutation_*`
tests. What stayed exactly the same, unmodified, and still passes:
`RealityRebuilder_SkeletonMutation_...VisualBranchNeverExecutes` --
skeleton really doesn't reach into the visual branch, so that
isolation claim was always true and remains true after this phase.

Milestone 5's own transactional/order-independence tests were **not**
touched and still pass against the new graph, verified rather than
assumed -- real evidence the atomicity and no-partial-write guarantees
don't depend on the specific shape of the edge list; they hold for
whatever graph `BuildBrooklynDependencyGraph` produces.

Full clean `cmake`+`make` rebuild, zero warnings. 549/549 tests
passing (same count as before this phase -- three tests were rewritten
to assert the corrected coupling rather than added or removed).

### Still genuinely unresolved

- The graph's source-level modeling still can't distinguish "a genome
  edit that breaks ref resolution" from "a genome edit that only
  changes content" -- both are treated identically conservatively.
  Representing that distinction would need richer node/edge semantics
  than a plain hash-diff graph; real, scoped, future work.
- Still Brooklyn only. Still no watcher. Still no CI integration.

---

## Milestone 7: Evidence-Derived Graph Primitives

A new, richer graph model, per explicit direction -- not a further
edit to Milestone 3/6's `DependencyGraph`/`ImpactAnalyzer` (untouched,
still exactly as they were) or to `RealityRebuilder` (Milestone 4/5,
also untouched). `REALITY/EvidenceGraph.h` and `REALITY/
BrooklynEvidenceGraphBuilder.h/.cpp` are new, standalone files.
Migrating `RealityRebuilder` onto this graph is real, separate, future
work -- deliberately not done here, so this phase could be judged on
its own, without risking the 549 tests already resting on the old
graph's exact shape.

### The primitives, exactly as specified

```cpp
enum class AuthorityType { kSourceFile, kCertificate, kArtifact };
enum class EdgeType { kAuthority, kInput, kDerived, kRuntime, kEvidence, kProvenance };

struct EvidenceNode { node_id, artifact_id, artifact_hash, authority_type, state; };
struct EvidenceEdge { from, to, edge_type, reason, evidence; };
struct UndeclaredDependency { from, to, detail; };

class EvidenceGraph {
    AddNode();  AddEdge();  Validate();
    DependentsOf();  DependenciesOf();  ImpactedBy();  TopologicalOrder();
    GraphHash();
};
```

`AUTHORITY`/`EVIDENCE`/`PROVENANCE` are declared but genuinely unused
by Brooklyn's graph -- stated plainly in the header rather than filled
with a placeholder edge, since an edge that doesn't cite real evidence
would be exactly the fabrication this file exists to prevent.

### The rule, enforced by construction, not convention

`AddNode` refuses a duplicate `node_id` outright. `AddEdge` refuses a
self-dependency and refuses an edge whose `from`/`to` isn't already a
real node -- in both refusal cases, the attempt is recorded in
`undeclared` instead of silently becoming a dangling edge or a phantom
node. This is the literal mechanism behind "if runtime code establishes
a dependency that isn't represented in artifact metadata, report it as
undeclared, don't silently insert it as graph truth" -- and it caught a
real bug during construction (see below).

### Evidence-derivation, not a hand-maintained list

`BrooklynEvidenceGraphBuilder` parses `brooklyn_canonical.dominus`
structurally (`CORE::json`, the same parser `DominusSerializer`
already uses) and discovers every ref-bearing artifact generically --
it does not hardcode `"visual_genome"`/`"material_genome"` as a fixed
list. Three real JSON shapes this schema actually uses are handled:
a top-level `{"ref": "..."}` object, a top-level array of `{"name",
"ref"}` objects (`animations`, `moves`), and a nested `"<label>_ref"`
string field (`physics_rules.hurtbox_ref`). A future `{"ref": ...}`
block added to the schema is picked up automatically by the first
shape, without a code change here.

12 real nodes result: `skeleton`, `animations`, `hurtbox`, `moves`,
`motion_graph`, `combat_dna`, `visual_genome`, `material_genome`,
`visual_style_genome`, `rig_certificate`, `visualforge_certificate`,
`reality_artifact` -- richer than Milestone 3/6's 9, because this
builder discovered `motion_graph` and `combat_dna` as real, separate
artifacts the old hand-picked node list never named at all.

### Edges, each cited to a real function signature or binder behavior

**INPUT** (a named function parameter): `skeleton`/`animations`/
`hurtbox`/`moves` -> `rig_certificate` (`CharacterAcceptanceHarness.h`'s
own `CheckSkeleton`/`CheckAnimation`/`CheckCombat` parameter lists);
`visual_genome`/`material_genome`/`visual_style_genome` ->
`visualforge_certificate` (`CharacterBlueprintForge::Build`'s
parameter list).

**RUNTIME** (only reachable through a whole-object bind, never a named
parameter): `visual_genome`/`material_genome`/`visual_style_genome` ->
`rig_certificate` -- Milestone 6's coupling, now with its own graph
model instead of three edges dropped into the old one;
`motion_graph`/`combat_dna`/`moves` -> `rig_certificate` -- two edges
Milestone 3/6 never modeled at all, found by reading `RigBinder.cpp`
and `CombatBinder.cpp` directly (`MotionGraphRefComponent`,
`CombatDnaRefComponent`, `MoveRefListComponent` are all resolved by
the full `Bind()` calls `CheckRuntime` performs, with no direct
`CheckX` parameter naming them).

**DERIVED**: `rig_certificate`/`visualforge_certificate` ->
`reality_artifact`, citing `ComputeRealityArtifactHash`'s own formula.

### A real bug the `undeclared` mechanism caught during construction

The first draft's citation for the hurtbox edge used
`"brooklyn.physics_rules.hurtbox_ref"` -- but the discovery logic
actually produces the node id `"brooklyn.hurtbox"` (stripping the
`_ref` suffix and the `physics_rules.` prefix, per the third ref
shape). The mismatch meant that edge silently never got added -- and,
worse, the `addInput`/`addRuntime` helper lambdas were originally
written to just `return` on a `FindNode` miss, discarding the mismatch
instead of surfacing it. Both were fixed: the citation now uses the
correct id, and the helpers now push a real `UndeclaredDependency`
entry any time a citation doesn't resolve, so the *next* typo like
this fails a test instead of silently vanishing.

### Tests, matching the required list

`tests/reality/test_evidence_graph.cpp`, 20 new tests. Ten prove
`EvidenceGraph` itself is generic and correct with small synthetic
graphs that have never heard of Brooklyn: duplicate-node refusal,
dangling-edge refusal (recorded as `undeclared`), self-dependency
refusal, cycle detection, topological order, `ImpactedBy` transitive
closure vs. `DependentsOf`/`DependenciesOf` direct-only, and
`GraphHash` determinism -- proven specifically by building the same
graph with nodes/edges inserted in the *opposite* order and checking
the hashes match.

Ten more prove Brooklyn's real graph: builds cleanly with zero
`undeclared` citations; every node has a real, non-empty hash; every
edge's `reason`/`evidence` are non-empty and both endpoints are real
nodes; `Validate()` passes with zero issues; topological order
succeeds; changing `visual_genome` now correctly impacts BOTH
`visualforge_certificate` AND `rig_certificate` (the corrected,
evidence-derived answer -- distinct from what the old graph could ever
say, since RUNTIME edges didn't exist there as a category);
`motion_graph`/`combat_dna` are asserted to be RUNTIME edges, never
INPUT (a real, checkable claim about the evidence, not a description);
an unrelated artifact never falsely reaches the visual branch; two
independent builds are hash-identical; and a missing `.dominus`
refuses honestly (empty graph, non-empty `undeclared`) rather than
returning a misleadingly "complete" empty result.

Live from the CLI, matching the directive's own requested output
format:

```
$ dominus-cli evidence-graph tests/fixtures
DEPENDENCY GRAPH -- brooklyn
Nodes: 12   Edges: 15
Duplicate nodes: NONE   Dangling edges: NONE
Self-dependencies: NONE   Cycles: NONE
Undeclared citations: NONE
Determinism (2 builds): PASS   Topological order: PASS
...
Graph hash: 6b447b85a0682594e83c9f7071d896b79460cecab44091a26d7370f687a3ba88
```

Full clean `cmake`+`make` rebuild, zero warnings. 570/570 tests
passing across the whole engine (was 549 before this phase -- Milestone
6 unchanged, +20 new, +1 new file pair, zero regressions).

### A disclosed design trade-off, not hidden

This graph hashes every source node from **raw file bytes**, uniformly
across every discovered ref shape. Milestone 3/6's graph hashes the
three genome nodes from their **parsed content** (via `REGISTRY::
VisualGenomeCompiler` etc.), which is blind to formatting-only edits.
Neither is "more correct" -- they answer different questions ("did the
bytes change" vs. "did the parsed meaning change") -- but a caller
mixing hashes from the two graphs would get nonsense, since they are
not the same values for the same artifact. Documented here explicitly
so nobody discovers this by an assertion failure six months from now.

### Genuinely unresolved

- **Not wired into any consumer.** `RealityRebuilder` still uses
  Milestone 3/6's graph. Migrating it here is real, separate work --
  and would need to resolve the raw-bytes-vs-parsed-content hash
  difference above first, since `RealityRebuilder`'s registry currently
  stores Milestone 3/6-shaped hashes.
- **`moves[]` is aggregated into one node.** `CheckCombat` only takes
  `jab` directly; the other five moves are RUNTIME-only. Modeling one
  node per move (rather than one aggregate `brooklyn.moves` node)
  would make the INPUT/RUNTIME split exact instead of coarse for that
  one artifact group -- flagged, not fixed, real future work.
- **`AUTHORITY`/`EVIDENCE`/`PROVENANCE` edge types are unused.** No
  Brooklyn edge currently needs them; they exist in the enum because
  the directive specified them, not because this phase found a real
  use. Using one without real evidence to back it would be exactly the
  fabrication this file was built to prevent.
- **Still Brooklyn only. Still no watcher, no CI, no auto-repair.**

---

## Milestone 8: Adversarial Multi-Artifact Mutation

The next serious test, per explicit direction: mutate multiple real
authority artifacts *simultaneously*, construct the graph
independently in different orders, calculate impact, rebuild, and
verify the resulting registry and artifact hashes are identical. Not a
new production feature -- one new, small, generic addition to
`EvidenceGraph` (needed to even ask the question), and one real test
file that puts the whole stack under adversarial load at once.

### The one addition: snapshot comparison for `EvidenceGraph`

`EvidenceGraph::ChangedNodes(before, after)` and `::ImpactOfChanges(before,
after)` -- static, pure, generic, mirroring the exact discipline
`REALITY/DependencyGraph.h`'s `ImpactAnalyzer` already established for
the older graph. `ImpactedBy`, `TopologicalOrder`, `Validate`,
`AddNode`, `AddEdge`, `GraphHash` are all unmodified. This is not the
"content vs. reference" heuristic explicitly ruled out -- it's a plain
diff between two hash snapshots, the same kind of comparison
`GraphHash`'s own determinism tests already relied on.

### Two real artifacts, mutated at once -- verified empirically, not assumed

`brooklyn_canonical.skel.json` (a raw-byte edit) and `brooklyn_visual.json`
(a real content edit, `"chaotic"` -> `"grim"`) mutated together, in the
same fixture copy. Before writing a single assertion, the actual
`ChangedNodes`/`ImpactOfChanges` output was captured and read, not
guessed at. It surfaced something genuinely interesting:

```
ChangedNodes:      skeleton, visual_genome, visualforge_certificate, reality_artifact
ImpactOfChanges:   + rig_certificate
```

`rig_certificate`'s own hash does **not** change from the whitespace-only
skeleton edit -- `CheckSkeleton`/`CheckAnimation`/`CheckCombat`/
`CheckRuntime`/`CheckDeterminism` all re-derive identical pass/fail and
detail text from parsed data that a trailing space doesn't touch. But
it still shows up in `ImpactOfChanges`, because skeleton structurally
feeds it and the graph re-verifies on principle, not on a guess about
whether this particular edit was safe. This is the directive's own
"this dependency changed, therefore conservatively re-verify the
dependent" stated as running code, not aspiration -- and it emerged
from real behavior, not a contrived example.

### The full pipeline, both directions, registry and artifact hash identical

Two independent fixture copies, identically mutated, rebuilt through
the real `RealityRebuilder` in **opposite** order (skeleton-then-visual
on one, visual-then-skeleton on the other):

```
$ # Copy A: skeleton first, then visual
$ # Copy B: visual first, then skeleton
registry(A) == registry(B)                     byte-identical
registry(A).reality_artifact == registry(B).reality_artifact
```

### A cross-model guarantee, proven rather than assumed

The old graph (Milestone 3/6, driving `RealityRebuilder`) and the new
`EvidenceGraph` (Milestone 7) were built independently, with different
node/edge shapes -- but both ultimately call the exact same
`internal::CompileRig`/`CompileVisualForge`/`ComputeRealityArtifactHash`
functions for the certificate/artifact nodes. `CrossModel_
OldGraphRebuiltArtifactHash_MatchesNewGraphIndependentlyComputedHash`
rebuilds through the old graph's pipeline, then independently
reconstructs `brooklyn.reality_artifact` from the same files via the
new graph, and checks they match. They do -- a real, structural
guarantee that was never explicitly tested until now.

All 5 new tests passed on the first real run against real fixtures --
notable given the number of independent moving parts (two graph
models, a registry, atomic writes, topological ordering, two real
compilers) that all had to agree. Full clean `cmake`+`make` rebuild,
zero warnings. 579/579 tests passing across the whole engine (was 574
before this phase; +5 new, +1 addition to `EvidenceGraph.h`).

### What this does and doesn't prove

Proven: the graph and the full rebuild pipeline survive two
*independent* branches mutated at once, in either order, with
byte-identical convergence. Not proven, and not attempted: three or
more simultaneous mutations; mutations to nodes that share a common
downstream dependent in more complex ways than this graph's current
shape has; or anything resembling a real "concurrent processes racing
to rebuild the same registry" scenario (Milestone 5's atomic `Save()`
makes that safe against corruption, but two truly concurrent
`RebuildFromChange` calls racing on the same registry file was never
tested here -- real, separate, future work).

---

## Milestone 9: Concurrent Registry Safety

The remaining boundary Milestone 5 (crash safety for one process) and
Milestone 8 (deterministic sequential propagation) didn't touch: what
happens when two independent processes try to rebuild and persist the
same registry at the same time.

### Established first, not assumed

Before writing any fix, the actual current behavior was measured: two
real `std::thread`s (each opening its own file descriptor -- the exact
property that makes this a genuine stand-in for two separate OS
processes, since nothing about `RealityRebuilder::RebuildFromChange`
shares in-process state across calls) racing `RebuildFromChange`
against the same registry, one rebuilding `brooklyn.skeleton`, the
other `brooklyn.visual_genome` (Milestone 8's own scenario). Result: a
lost update in **40 out of 40** real trials.

The mechanism, found by inspecting the actual persisted bytes, not
guessed: each call loaded the registry once at the start and only ever
wrote its own branch's keys into its in-memory copy. Whichever call's
`Save()` happened to land *last* wrote its own stale, load-time copy
of the *other* branch's keys back over the top -- silently reverting
them. Confirmed precisely: `brooklyn.skeleton`'s persisted hash
reverted to its pre-mutation value in the race, even though the thread
responsible for that branch reported `ok=true`.

### The fix: a real OS-level lock, not an in-process mutex

`REALITY/FileLock.h/.cpp` -- a small RAII wrapper around POSIX
`flock()` on a sibling `<registryPath>.lock` file. `flock()` was
chosen specifically because its lock lifetime is tied to the *open
file description*, not the process: a crashed holder's lock is
released by the kernel the moment its descriptor closes, so an
abandoned lock from a dead process can never deadlock a future caller
-- no manual cleanup, no stale-lock-file heuristics.

`RealityRebuilder::RebuildFromChange` now acquires this lock around
the **entire** Load -> compute -> Save cycle, with `Load()` happening
fresh, *inside* the lock -- not before it. That's the actual fix: a
caller that had to wait for the lock always sees the true latest state
once it gets in, never a snapshot from before it started waiting.
Re-running the identical 40-trial reproduction after the fix: **0/40
mismatches.**

### CONFLICT -> REJECT, not last-write-wins -- and what "conflict" means here concretely

Per explicit direction: a caller that cannot acquire the lock within a
bounded timeout (`RebuildFromChange` now takes an optional
`lockTimeout`, default 5s) refuses outright --
`RebuildReport.lock_contention = true`, `ok = false`, nothing read or
written -- rather than proceeding unsynchronized.

One honest scoping note: in a general system, "conflict" often means
"two different, individually-valid final states are competing, and
neither should silently win." That scenario doesn't actually exist in
*this* domain -- Dominus's correct final state is always a pure
function of the current files on disk, recomputed identically by
whoever asks, at any time (Milestone 8 proved this: independent
reconstructions converge byte-for-byte regardless of order). So here,
concretely, CONFLICT -> REJECT is implemented as "refuse to proceed
without exclusive access" -- lock contention -- not as "detect two
divergent commits and pick neither," because the latter case cannot
occur. Two threads racing to rebuild the *same* target don't produce
competing answers; whichever gets the lock second simply re-reads
fresh state and correctly finds nothing left to do.

### All 9 required scenarios, tested against real threads and a real lock

`tests/reality/test_concurrent_registry_safety.cpp`, 10 new tests:

1. **Same target, both racing** -- both report `ok=true`; the second
   (once it gets the lock) re-reads fresh and finds the first already
   did the work.
2. **Different mutations, overlapping downstream** (skeleton and
   visual_genome, which both real-feed `rig_certificate` per
   Milestone 6/7's RUNTIME edges) -- the exact 40/40-broken scenario,
   now byte-identical to a sequential ground-truth rebuild.
3. **One succeeds while the other genuinely fails** -- a real,
   isolated VisualForge-only failure (Milestone 5's
   `style_reference_matches` trigger) racing a genuinely valid
   skeleton mutation; the failure never touches the successful
   branch's persisted result.
4. **A "crash" during persistence** -- a `FileLock` acquired and then
   abandoned (scope exit, no write) standing in for a process that
   died mid-critical-section; a later caller proceeds normally, no
   deadlock, no corruption.
5. **Final registry never syntactically corrupt** -- 15 rounds of real
   concurrent racing, JSON-parsed and checked valid after every round.
6. **No false success on stale state** -- an externally-held lock
   forces a real, short-timeout contention; the attempt refuses
   (`lock_contention=true`) and the registry is byte-identical to
   before the refused attempt; releasing the external lock lets the
   identical call succeed normally.
7. **Every outcome is either committed or an explicit, named conflict**
   -- never a third, silent state; checked structurally (`ok` XOR a
   real `lock_contention`/`summary` explanation).
8. **Deterministic convergence after a race** -- both branches asked
   again after racing report no further change, and the file doesn't
   move.
9. **`FileLock` itself, isolated** -- a second `TryAcquire()` fails
   while the first holds it and succeeds the instant it's released;
   `Acquire()` with a timeout genuinely returns false within a bounded
   window rather than hanging.

Full clean `cmake`+`make` rebuild, zero warnings. 589/589 tests
passing across the whole engine (was 579 before this phase; +10 new),
confirmed stable across 5 repeated full runs (a real, not rhetorical,
check against test flakiness given the threading involved).

### Genuinely unresolved

- **Whole-registry locking, not row-level.** Two rebuilds targeting
  completely unrelated subjects (if this engine ever has more than
  Brooklyn) would still serialize through the same lock. Real,
  scoped, future work once a second subject exists to make row-level
  locking meaningful.
- **No distributed locking.** `flock()` is local-filesystem-only; a
  registry on a network filesystem shared across machines is out of
  scope for this phase's guarantee.
- **The watcher is still not built.** This was the explicit
  prerequisite for it being worth building at all -- the graph and
  registry are now provably safe under concurrent access for an
  autonomous process to depend on, but no autonomous process exists
  yet.

---

## Milestone 10: Change Event Normalization & Transaction Boundaries

The constitutional rule this milestone exists to enforce: **a
filesystem event is never authoritative evidence of a Reality change.
Only a verified change in canonical source state can enter the Reality
Compiler.** Built explicitly *before* any actual filesystem watcher --
per direction, this is the boundary that keeps a future watcher dumb
and the Reality Compiler authoritative, not the watcher itself.

### The pipeline, as specified

```
OS events
    |
Event Collector        (ChangeEventNormalizer::Normalize accepts a
    |                    RawFileEvent batch -- from any source)
Debounce / coalesce     (multiple events, same real file -> one candidate)
    |
Canonical Source Identity (BrooklynEvidenceGraphBuilder::DiscoverArtifactFiles
    |                    -- the SAME real, evidence-derived discovery
    |                    Milestone 7 built, reused not reimplemented)
Content/hash verification (re-reads CURRENT disk state, compares
    |                    against RealityRebuilder's own last-known-good
    |                    hash -- never trusts the event's claimed kind)
Logical ChangeSet
    |
RealityRebuilder        (Milestone 4-9, completely unmodified)
```

### What this file is not

Not a filesystem watcher -- no inotify, no polling loop. `Normalize()`
is a pure function of whatever `RawFileEvent` batch it's handed; it
never touches the registry (proven by a dedicated test). Not a second
dependency system -- it never decides "file X changed, therefore
rebuild Y" itself. `ImpactAnalyzer::AffectedBy` (Milestone 3, called
inside `RealityRebuilder`) still owns that decision entirely; this
file's only job is deciding whether a raw observation is even real
evidence worth handing to `RealityRebuilder` at all.

### Every required scenario, against real files

`tests/reality/test_change_event_normalizer.cpp`, 9 new tests:

- **Duplicate events** -- 6 raw events for the same file collapse to 1
  verified change.
- **Rename/create/delete sequences** -- a real editor-style
  delete→create→modify sequence for one logical edit still collapses
  to exactly 1 change.
- **Temp-file / non-authoritative events** -- `.swp`, `~`, an unrelated
  README, even `/etc/passwd` -- all rejected with a named reason, never
  silently dropped and never silently trusted.
- **Bursts of mixed real and noise** -- a batch naming five files where
  only one actually changed; the other four correctly land in
  `unchanged`, not `verified`.
- **A real artifact RealityRebuilder has no node for** -- `motion_graph`
  and `combat_dna` (Milestone 6/7's own discovery) are genuine,
  declared artifacts, so they are never `rejected` -- but
  `RealityRebuilder`'s graph has no node for them, so they land in a
  distinct `unrepresented` bucket instead of being silently dropped or
  force-mapped onto an unrelated node.
- **Never trusts event claims** -- an event claims a file was
  `DELETED`; the file is actually untouched on disk; verification
  correctly reports `unchanged`, proving the claimed event kind never
  influences the outcome, only the real current state does.
- **Full pipeline** -- `ProcessEvents` on a 5-event mixed batch (2 real
  changes, a duplicate, noise, and an unrepresented artifact) drives
  exactly 2 real `RealityRebuilder` calls -- not 5, not 4.
- **Empty batch** -- produces an empty ChangeSet, never a fabricated
  change.
- **Read-only `Normalize()`** -- byte-identical registry before and
  after, confirmed directly.

Live from the CLI:

```
$ dominus-cli change-events fixtures/ registry.json skel.json skel.json swapfile.swp motion_graph.json material.json
raw_events_received:        5
distinct_paths_after_dedup: 4
REJECTED (1):    swapfile.swp -- not a declared authoritative artifact reference
UNCHANGED (1):   brooklyn.material_genome
UNREPRESENTED (1): brooklyn.motion_graph -- real artifact, no RealityRebuilder node
VERIFIED (1):    brooklyn.skeleton -- live hash differs from registry
--- driving RealityRebuilder for each verified change ---
  brooklyn.skeleton: OK -- rebuilt 5 node(s), all passed
```

### The translation table -- an honest, disclosed bridge, not a second discovery mechanism

`EvidenceToRebuilderNodeMap()` is the one genuinely hand-written table
in this file: it maps Milestone 7's evidence-derived node space (12
nodes, richer) onto `RealityRebuilder`'s coarser node space (9 nodes,
Milestone 3/6). It is *not* a second ref-discovery mechanism --
`DiscoverArtifactFiles` already did the real discovery of which files
exist and what they're called; this table only says which of
`RealityRebuilder`'s buckets each discovered group falls into. Two
entries are deliberately absent (`motion_graph`, `combat_dna`) -- the
real gap this milestone surfaces rather than hides.

### Genuinely unresolved

- **No actual watcher.** This was explicit and intentional -- Milestone
  10 proves event *correctness*, not event *collection*. A real
  inotify-backed collector that feeds `RawFileEvent` batches into this
  pipeline is real, separate, future work, and per the directive,
  should come *after* this.
- **The motion_graph/combat_dna gap is surfaced, not fixed.** A real
  change to either is honestly reported as `unrepresented` and cannot
  drive a rebuild today. Extending `RealityRebuilder`'s graph to cover
  them is real, separate work.
- **No true debounce *timing*.** "Debounce/coalesce" here means
  same-batch deduplication by filename, not a time-windowed wait for a
  burst to settle before processing -- a real watcher would likely add
  a short timing window before calling `Normalize()`; that's a
  property of the (not-yet-built) collector, not this pipeline.

---

## Milestone 11: Reality Watcher

The first genuinely end-to-end autonomous loop. Edit reality -> DOMINUS
notices -> DOMINUS proves what changed -> DOMINUS determines
consequences -> DOMINUS rebuilds -> DOMINUS persists the new reality.

### Deliberately thin, per direction -- checked against the actual file

`REALITY/RealityWatcher.h/.cpp`, and nothing else changed. This class
contains no dependency logic, no rebuild logic, no artifact
classification, no authority decisions, no registry mutation logic, no
Brooklyn special-casing, and no hash system of its own -- every one of
those already exists in Milestones 1-10 and is called, not
reimplemented. What `RealityWatcher` actually does, in full:

1. Opens a real inotify file descriptor and registers a watch on one
   directory (non-recursive -- Brooklyn's fixtures are flat), in the
   constructor, synchronously -- so a caller never has to guess when
   the watch is "ready."
2. Blocks on `poll()` for either a real inotify event or a `Stop()`
   signal (a self-pipe -- no periodic polling, no busy loop).
3. On a real event, reads whatever the kernel already batched into one
   `read()`, translates each raw `inotify_event` into a `RawFileEvent`
   (path + one of four kinds -- the *only* classification this file
   performs), and hands the whole batch to
   `ChangeEventNormalizer::ProcessEvents` in one call.
4. That one call is the entirety of this class's "intelligence" --
   canonicalization, deduplication, verification, impact analysis,
   selective recompilation, locking, and atomic persistence are all
   Milestone 4-10, completely unmodified.

`IN_CLOSE_WRITE` (not `IN_MODIFY`) is the primary watched event --
it fires once when a writer actually closes the file, which is what "a
save happened" really means; `IN_MODIFY` fires once per `write()`
syscall and would manufacture burst noise before normalization even
gets involved. `IN_CREATE`/`IN_DELETE`/`IN_MOVED_TO`/`IN_MOVED_FROM`
are also watched, feeding the exact rename/create/delete sequences
Milestone 10 already proved it can canonicalize correctly.

### Proven live, with real inotify, before any test was written

Verified `inotify_init1`/`inotify_add_watch` work in this environment,
then verified a real file write produces a real, observable
`IN_CLOSE_WRITE` event, *before* writing `RealityWatcher` itself. Then,
before writing the permanent test suite, ran the actual end-to-end
loop by hand: a real background thread running `Run()`, a real file
save from the main thread, a bounded wait, `Stop()`, and inspected the
result:

```
history size: 1
  brooklyn.skeleton: OK -- rebuilt 5 node(s) from 'brooklyn.skeleton', all passed
registry changed on disk: 1
```

Save file -> watcher observes -> event normalizes -> source verifies
-> graph computes impact -> rebuild executes -> registry atomically
commits. All real, all on the first attempt.

### Tests, all against real inotify and real files, no mocks

`tests/reality/test_reality_watcher.cpp`, 7 new tests: the core loop
(save -> real rebuild -> registry changes on disk -> a subsequent
direct check finds nothing pending); a non-authoritative file save
never reaches `RealityRebuilder` (registry byte-identical after);
multiple real sequential saves each produce their own real rebuild;
`Stop()` called before `Run()` never hangs; `Stop()` while idle returns
promptly; watching a nonexistent directory throws immediately rather
than silently watching nothing; and two rapid real saves to the same
file converge to the correct final state regardless of whether the
kernel delivered them as one batch or two.

Live from the CLI: `dominus-cli reality-watch <fixtures_dir>
<registry_path>` blocks, watching for real saves, until the process is
killed -- confirmed end-to-end by starting the actual watcher process,
editing a real file from another shell, and observing the registry was
correctly updated by the time the watcher process exited.

Full clean `cmake`+`make` rebuild, zero warnings. 605/605 tests passing
across the whole engine (was 598 before this phase; +7 new), confirmed
stable across 5 repeated full runs given the real threading and OS
event timing involved.

### Genuinely unresolved, explicitly by design

- **No recursive directory watching.** One flat directory only,
  matching Brooklyn's actual fixture layout. A real multi-directory
  project would need this extended -- real, separate, future work.
- **No parallel rebuild workers.** Events are processed one batch at a
  time, in the single thread that calls `Run()`. Each individual
  `RebuildFromChange` call is already safe under concurrency
  (Milestone 9), but this watcher doesn't exploit that by running
  multiple rebuilds in parallel -- "deliberately boring," per
  direction.
- **No process supervision.** If the process hosting `RealityWatcher`
  crashes, nothing restarts it. `dominus-cli reality-watch` is a
  foreground process today, not a daemon.
- **No hot-reload of the watch target.** If `brooklyn_canonical.dominus`
  itself changes to declare a new artifact, the watcher doesn't need to
  restart to see files written to the same directory -- but it also
  doesn't re-derive anything about the schema itself; that's
  `ChangeEventNormalizer`'s job on every call, already handled.
- **This is still Brooklyn-only**, same as every milestone before it.

---

## Milestone 12: Recovery / Reconciliation on Restart

The one fundamental weakness of an event-driven watcher: filesystem
events are ephemeral, source state is persistent. If DOMINUS was
stopped -- crashed, restarted, or simply never running yet -- while an
authoritative file changed, no `inotify` event for that change will
ever arrive. Without this milestone, that change stays permanently
unnoticed until something else happens to touch the same file again.

### Not a new authority system -- a different event source for the same pipeline

`REALITY/Reconciler.h/.cpp`. The entire mechanism: enumerate every
declared authoritative artifact file (`BrooklynEvidenceGraphBuilder::
DiscoverArtifactFiles` -- the same real, evidence-derived discovery
Milestone 7/10 already built), synthesize one `RawFileEvent` per file
(`kind = kUnknown`, deliberately -- a reconciliation scan has no real
information about *how* a file changed while nobody was watching, only
that it should be checked; downstream verification never branches on
kind anyway), and hand the whole batch to `ChangeEventNormalizer::
Normalize`/`ProcessEvents` -- Milestone 10's pipeline, completely
unmodified. A full scan instead of a live stream, feeding the identical
canonicalize -> dedupe -> verify -> rebuild path. Files remain
authoritative. The registry remains derived evidence. The graph
remains dependency authority. This file remains merely a second way of
asking "what does the registry not yet know about."

### Proven live, exactly matching the target scenario, before any test was written

```
baseline established
two real offline mutations applied, no watcher was running

Scan() (read-only) found:
  VERIFIED: brooklyn.skeleton
  VERIFIED: brooklyn.visual_genome

Reconcile() drove 2 real rebuild(s):
  brooklyn.skeleton: OK -- rebuilt 5 node(s), all passed
  brooklyn.visual_genome: OK -- rebuilt 4 node(s), all passed

re-check skeleton change_detected: 0
re-check visual change_detected: 0
```

Two real, independent files mutated with genuinely nothing running to
observe them, then a fresh reconciliation call found and rebuilt both
correctly, converging to a stable fixed point -- the exact scenario
named in the directive, reproduced for real before a single test file
existed.

### `RealityWatcher` integration -- one new parameter, nothing else changed

`Run(bool reconcileOnStart = true)`. When true (the default), the
*first* thing a (re)started watcher does -- before registering any
interest in live `inotify` events -- is call `Reconciler::Reconcile()`.
Its results land in `History()` exactly like a live event's would;
there is no separate code path or separate kind of `RebuildReport` for
a reconciled change. Skipped entirely if `Stop()` was already called
before `Run()` even started, so a caller that wants to stop immediately
doesn't have to wait through a full scan first.

### Tests, all against real files, no mocks

`tests/reality/test_reconciler.cpp`, 8 new tests: `Scan()` is
read-only (byte-identical registry before/after); the core scenario
(one real offline mutation, zero events ever generated, reconciliation
finds and fixes it, converges to a stable fixed point); multiple
independent offline mutations recovered in one pass, cross-checked
byte-for-byte against Milestone 8's sequential ground truth; no offline
mutation produces zero rebuilds (a real, honest no-op); no registry yet
falls through to `RealityRebuilder`'s own real bootstrap path,
unmodified; a byte-identical rewrite (mtime touched, content
unchanged) produces zero rebuilds -- proving reconciliation trusts
content hashes, never timestamps; full `RealityWatcher` integration --
an offline mutation with literally no watcher object in existence is
recovered the moment a new one starts, before any live event could
possibly fire; and `reconcileOnStart=false` proves reconciliation is a
real, optional, separable step, not unconditionally fused into the
event loop.

Live from the CLI: `dominus-cli reality-reconcile <fixtures_dir>
<registry_path>` -- a full scan and reconcile, usable standalone or as
what a restarted watcher runs automatically.

Full clean `cmake`+`make` rebuild, zero warnings. 613/613 tests passing
across the whole engine (was 605 before this phase; +8 new), stable
across 5 repeated full runs.

### Genuinely unresolved

- **Reconciliation cost scales with the number of declared artifacts**,
  not with what actually changed -- every real file gets re-hashed on
  every reconciliation pass (this is also true of `Scan()`'s read-only
  variant). Fine at Brooklyn's scale; a real concern for a much larger
  project, and real, separate, future work (e.g. mtime as a cheap
  pre-filter before content hashing, never as a substitute for it).
- **No periodic reconciliation while running** -- only at `Run()`
  startup. A network filesystem or another process bypassing `inotify`
  entirely (rare, but real) between reconciliation passes would still
  be missed until the next restart. Real, separate, future work if it
  ever matters.
- **Still Brooklyn only**, same as every milestone before it.

---

## Dependency Graph Migration: RealityRebuilder onto EvidenceGraph

`RealityRebuilder` (Milestone 4-9's selective-recompilation engine)
migrated from `REALITY/DependencyGraph.h`'s `DependencyGraph`/
`ImpactAnalyzer` (Milestone 3/6, 9 hand-authored nodes) onto
`REALITY/EvidenceGraph.h`'s `EvidenceGraph` (Milestone 7, 12
evidence-derived nodes) as its sole dependency authority. Per explicit
direction: `EvidenceGraph` was not extended with a second, parallel
"DependencyGraph-under-a-new-name" -- gaps it genuinely lacked were
either filled with a real capability addition or solved by reusing
what already existed.

### The one real capability gap, filled honestly

`BrooklynEvidenceGraphBuilder::Build()` always ran the full, expensive
RIG+VisualForge compile to get real certificate/artifact hashes.
`RealityRebuilder` needs a FAST graph (source hashes only) purely for
change-detection -- paying the full compile cost on every single call,
even ones that find nothing changed, would have been a real
performance regression the old `RealityCompiler::
BuildBrooklynDependencyGraph` never had. Fixed by adding
`BrooklynEvidenceGraphBuilder::BuildStructure()`, sharing one real
implementation with `Build()` (`BuildStructureCommon`/
`AddAllRealEdges`) so the two can never drift apart -- not a
duplicate ref-discovery mechanism, a real, disclosed fast path.

The other gap -- `EvidenceGraph::TopologicalOrder()` only orders the
WHOLE graph, but `RealityRebuilder` needs to order just the
invalidation subset -- did NOT need a new method on `EvidenceGraph`
at all. A filtered subsequence of a valid whole-graph topological order
is provably a valid topological order of any induced subgraph (proof
in `RealityRebuilder.cpp`'s `TopologicalOrderOfSubset`); the old local
Kahn's-algorithm-on-a-subgraph helper is retired, not reimplemented
under a new name.

### Two real, verified behavior changes -- surfaced, not hidden

1. **Bootstrap now compiles 12 nodes, not 9.** `EvidenceGraph`
   discovered `brooklyn.motion_graph` and `brooklyn.combat_dna` as
   real artifacts (Milestone 6/7's own finding) the old graph never
   modeled at all -- they are now genuinely tracked, verified, and
   rebuildable.
2. **A skeleton-only change's invalidation set shrank from 5 nodes to
   3** (`{skeleton, rig_certificate, reality_artifact}`), verified
   empirically before a single test was edited. The old graph had
   hand-modeled edges `skeleton -> animation` and `skeleton -> combat`
   with no cited function-parameter or runtime-bind evidence behind
   them -- unlike every other edge in `EvidenceGraph`.
   `brooklyn.animations`'/`brooklyn.hurtbox`'s/`brooklyn.moves`' own
   artifact hashes are pure functions of their own file bytes,
   genuinely independent of skeleton content, and `internal::
   CompileRig` re-verifies all of them together regardless (it has no
   "check only skeleton" mode) -- so re-hashing them from disk on a
   skeleton-only change was real work with no real purpose. This is a
   correction, not a weakening: the smaller invalidation set is the
   accurate one, and the relevant test
   (`RealityRebuilder_SkeletonMutation_RecompilesRigCertificateOnly_VisualBranchNeverExecutes`)
   was updated to assert it, with the reasoning documented inline.

### A genuine, remaining architectural fork -- not silently resolved

`RealityCompiler::CompileBrooklyn`/`BuildBrooklynDependencyGraph`
(Milestone 1/2's full-compile entry point) and the CLI's
`reality-compile`/`reality-graph`/`reality-impact` commands remain on
the OLD `DependencyGraph` -- a real, legitimate, still-functioning
production consumer, verified live and unaffected by this migration.
This was **explicitly out of scope** for this phase (only
`RealityRebuilder` was named as the production consumer to migrate),
so `REALITY/DependencyGraph.h` was NOT removed -- it still has a real
consumer.

The honest, user-visible consequence: `dominus-cli reality-impact
tests/fixtures brooklyn.skeleton` (old graph) still reports a 5-node
impact set; `dominus-cli reality-rebuild` (migrated `RealityRebuilder`)
now correctly acts on a 3-node one. Two graphs answering the same
question differently, depending which command you ask, is exactly the
"parallel dependency authority" state this whole migration exists to
eliminate -- it is reduced to ONE real production consumer
(`RealityCompiler`) instead of two, but not to zero. Migrating
`RealityCompiler` itself onto `EvidenceGraph` is real, legitimate,
separate future work.

### Dead code found and removed

`RealityRegistry::FromGraph(const DependencyGraph&)` had zero callers
anywhere in the repository -- dead code from before Milestone 4's own
bootstrap redesign (bootstrap has computed its baseline from real
compiled hashes directly since that fix, never via `FromGraph`).
Removed outright, along with `RealityRegistry.h`'s now-unnecessary
`#include "REALITY/DependencyGraph.h"` -- `RealityRegistry` was never
conceptually coupled to a specific graph type; it only ever stored a
plain `node_id -> hash` map.

Full clean `cmake`+`make` rebuild, zero warnings. 622/622 tests
passing (no net change in count -- fixes and one obsolete-premise
test rewrite balanced against one new positive test proving
`motion_graph`/`combat_dna` are now real, actionable changes), stable
across 5 repeated full runs.

---

## Dependency Graph Migration, Phase C: RealityCompiler + CLI, and full retirement of the old graph

Direct continuation of the RealityRebuilder migration above (Phase B).
`RealityCompiler` (Milestone 1/2's full-compile entry point) was the
one remaining real production consumer of the old
`REALITY/DependencyGraph.h`. It has now migrated onto `EvidenceGraph`
too -- the SAME authority `RealityRebuilder` uses -- and the old graph
has been fully removed from the repository.

### What migrated, concretely

`RealityCompiler::CompileBrooklyn`'s internal `AssembleGraph` helper
now builds its result via `BrooklynEvidenceGraphBuilder::BuildStructure`
(the exact same real discovery/edge logic `RealityRebuilder` already
uses) and fills in the cert/artifact node hashes from values
`CompileBrooklyn` already computed from its own real `internal::
CompileRig`/`CompileVisualForge` calls -- never re-running those real,
expensive checks a second time just to populate a graph. This removed
roughly 130 lines of duplicated source-hashing and edge-citation logic
(`ComputeSkeletonSourceHash`, `ComputeAnimationSourceHash`,
`ComputeCombatSourceHash`, `BrooklynClipPairs`,
`BrooklynDependencyEdges`) that had been drifting in parallel with
`BrooklynEvidenceGraphBuilder`'s own copies since Milestone 7.

`RealityCompiler::BuildBrooklynDependencyGraph` -- the standalone,
fast-path graph builder -- was removed outright rather than kept as a
thin wrapper: it was 100% redundant with `BrooklynEvidenceGraphBuilder::
BuildStructure`, which already does exactly what it did. Callers
(`reality-graph`, `reality-impact`) now call `BuildStructure` directly.

`RealityCompilationResult.dependency_graph`'s type changed from
`DependencyGraph` to `EvidenceGraph`.

### Baseline behavior recorded before migration, verified unchanged after

```
reality-compile: EXECUTABLE, RECONSTRUCT -> REPRODUCE A == B: true
  (artifact_hash identical before and after this migration:
   4c653add1adf8fa20766ba05fe98be885dcd54b05ba72a166834bad2f7e164c8)
```

`reality-compile`'s real, end-to-end result -- reaching `EXECUTABLE`
and reproducing identically twice -- is byte-for-byte unchanged by
this migration, confirmed by running it before and after.

### The fork is closed -- verified live, not assumed

Before this phase: `reality-impact` (old graph) reported a **5-node**
impact set for `brooklyn.skeleton`; `reality-rebuild` (already migrated
in Phase B) reported **3**. After this phase, both commands agree:

```
$ dominus-cli reality-impact tests/fixtures brooklyn.skeleton
  [invalidated]  brooklyn.reality_artifact
  [invalidated]  brooklyn.rig_certificate
  [changed]      brooklyn.skeleton
[result] 3 node(s) affected

$ dominus-cli reality-rebuild tests/fixtures registry.json brooklyn.skeleton
  ... established a real baseline by compiling all 12 nodes ...
```

`reality-compile`, `reality-graph`, `reality-impact`, and
`reality-rebuild` now all derive their dependency/impact information
from the same `EvidenceGraph` authority. There is exactly one
production dependency model for this domain.

### The old graph, fully removed

`tests/reality/test_dependency_graph.cpp` (13 tests exercising the old
graph directly) was removed -- every property it tested (real node
hashes, cross-model ground truth, determinism, transitive impact
isolation, unknown-node handling, transitive chains, changed-node
detection) already has a direct equivalent in `tests/reality/
test_evidence_graph.cpp`, confirmed by comparing both test lists
before deleting anything -- no coverage was lost.

`REALITY/DependencyGraph.h` itself was deleted. A full repository
search for `DependencyGraph`/`ImpactAnalyzer` before deletion found
zero remaining real `#include`s or type usages anywhere -- every
remaining match was a historical prose comment, updated to reflect the
retirement rather than left stale.

Full clean `cmake`+`make` rebuild, zero warnings. 609/609 tests passing
(622 before this phase, minus 13 removed obsolete tests), confirmed
deterministic across 5 repeated full runs.

### Final authority state

```
                    EvidenceGraph
                   /      |       \
                  /       |        \
                 ▼        ▼         ▼
       RealityRebuilder  RealityCompiler  CLI (reality-graph,
                                              reality-impact,
                                              reality-compile,
                                              reality-rebuild,
                                              change-events,
                                              reality-watch,
                                              reality-reconcile)
```

`REALITY::DependencyGraph`/`ImpactAnalyzer` no longer exist in this
repository. `GenomeRegistry`/`RealityRegistry` remain untouched, per
explicit instruction -- a separate architectural question, not
resolved here.

---

## Milestone 1 record (superseded above, kept for the trail)



```
brooklyn source
      |
RealityCompiler
      |
   RIG (CharacterAcceptanceHarness -- Skeleton/Animation/Combat/Runtime/Determinism)
      |
   VISUALFORGE (RendererPackageAcceptanceHarness -- BlueprintValidity/DependencyIntegrity/
      |          PackageDeterminism/ProvenanceCompleteness/Renderable/RenderDeterminism)
      |
   VALIDATE  (every stage must have validated -- REJECTED otherwise)
      |
   REGISTER  (artifact_hash = Sha256(entity, compiler_identity, rig_cert_hash, visual_cert_hash))
      |
   EXECUTE   (drives the real RigProfileLifecycle to ACTIVE, gated on the real certificate)
      |
   RECONSTRUCT -> REPRODUCE  (runs the whole pipeline twice, independently, compares artifact_hash)
```

## What this file coordinates, and does not duplicate

`RealityCompiler::CompileBrooklyn` calls exactly the same entry points
`dominus-cli brooklyn-acceptance` and `dominus-cli visual-acceptance`
already call: `RIG::CharacterAcceptanceHarness` and
`VISUALFORGE::RendererPackageAcceptanceHarness`. It does not
reimplement a skeleton check, a collision check, a render check, or an
authority validator. RIG and VISUALFORGE remain the sole authority for
whether their own domain's data is actually correct; REALITY only asks
"did every domain that must agree actually agree, on the same object,
and can that be proven twice."

`PipelineState` is exactly the seven states the directive named:
`NOT_DECLARED, DECLARED, COMPILING, VALIDATED, REJECTED, REGISTERED,
EXECUTABLE`. `PipelineLifecycle`'s transitions are as mechanically
gated as `RigProfileLifecycle`'s own -- no method can advance a stage
without a real fact already recorded on the context to justify it, and
`Reject` cannot fire once a context has already reached
`REGISTERED`/`EXECUTABLE`.

## The real, current result -- and why it's REJECTED, not EXECUTABLE

Building this coordinator surfaced a genuine gap that no single
existing CLI command exposed on its own, because each one was proven
against its own convenient fixture:

- `dominus-cli brooklyn-acceptance` proves RIG's migration against
  `brooklyn_canonical.dominus` (object_id `brooklyn_canonical`) --
  and it genuinely passes, all 5 sections.
- `dominus-cli visual-acceptance` was, until now, only ever run
  against `brooklyn.dominus` (the *legacy* object, object_id
  `brooklyn`) in the ROADMAP's own worked example -- because that is
  the only fixture that actually carries a `visual_genome` block.
  `brooklyn_canonical.dominus`, RIG's real migrated artifact, has
  **no visual_genome data at all**.

So the two domains have real, passing evidence -- for two *different*
objects. There is no single canonical Brooklyn artifact today that
both RIG and VisualForge have proven correct. `RealityCompiler`
refuses to paper over this by quietly pointing VisualForge at
`brooklyn.dominus` while RIG stays on `brooklyn_canonical.dominus` --
that would fabricate a unified identity that doesn't exist. Instead it
points both stages at the same object (`brooklyn_canonical.dominus`,
RIG's real artifact) and lets VisualForge's own package builder refuse
honestly:

```
$ dominus-cli reality-compile tests/fixtures
STAGES:
  RIG:           VALIDATED  (certificate_hash=... overall_pass=true)
  VISUALFORGE:   REJECTED   (package build refused: brooklyn_canonical.dominus ... has no bound VisualGenome)

PIPELINE STATE: REJECTED
EXECUTABLE:      false
RECONSTRUCT -> REPRODUCE:  A == B: false  (nothing registered yet to reconstruct)
```

This is the directive's own dependency-graph idea working before the
dependency graph itself is even built: "what exactly does this
artifact depend on" surfaced a real answer -- *two incompatible
things* -- the moment one pipeline had to ask both domains about the
same object at once.

## The real fix this points to (not done here, on purpose)

Bind `VisualGenome`/`MaterialGenome`/`VisualStyleGenome` onto
`brooklyn_canonical.dominus` -- either by adding the genome blocks to
that fixture directly, or by running the same kind of migration RIG's
own skeleton/animation/combat migration already went through for
visual data. That is real, scoped, separate work (and arguably
overdue regardless of REALITY -- README.md's own "Next" list has
flagged "bind MaterialGenome/VisualStyleGenome to entities via
RigBinder" as an open gap since before this phase). Once it lands,
`RealityCompiler_CompileBrooklyn_RejectsBecauseCanonicalObjectHasNoBoundVisualGenome`
in `tests/reality/test_reality_compiler.cpp` will start failing --
correctly -- and needs to flip to asserting `EXECUTABLE` instead.

## What's genuinely unresolved, flagged not hidden

- **No dependency graph / auto-invalidation.** `CompilationContext.
  dependency_identities` records which certificate hashes an artifact
  depended on, but nothing watches those hashes for change and
  triggers a rebuild. The directive's steps 3-4 (a real `Brooklyn ->
  Skeleton -> RigProfile -> ...` graph that can answer "what must be
  rebuilt") is not built -- Milestone 1 was one subject, proven
  honestly, first.
- **Brooklyn only.** Nothing here is generalized to a second
  character. Generalizing before a second real subject exists to
  prove the shape isn't Brooklyn-specific would repeat the exact
  mistake this engine's own culture has flagged before (see ROADMAP's
  "don't generalize prematurely" notes elsewhere).
- **RECONSTRUCT -> REPRODUCE only has something to prove once
  REGISTERED is reachable.** Today it correctly, honestly returns
  `false` (no artifact exists yet). Once the VisualForge gap above is
  closed, this becomes the directive's real `A == B` proof, executed
  twice, not asserted.
- **No CI/build-button integration.** `VALIDATION::BuildPipeline`
  discovers and validates every `.dominus` in a project; REALITY does
  not plug into that yet. A real, separate wiring task once a second
  subject exists to make "discover every character, reality-compile
  each" meaningful.
