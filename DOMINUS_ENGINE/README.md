# DOMINUS ENGINE — v0.1

The first Meta-Bin reality creation engine. Start here, then:

1. `DOMINUS_ENGINE_CONSTITUTION.md` — mission, laws, forbidden shortcuts.
2. `docs/ARCHITECTURE_v0.1.md` — concept model, system architecture, module
   boundaries, API surface, data schema, testing strategy, Phase 1 exit
   criteria.
3. `ROADMAP.md` — the five build phases and their gates.
4. `schemas/dominus_object.schema.json` — the canonical `.dominus` object
   shape.

**Phase 2.5 / DOMINUS MOTION INTELLIGENCE SYSTEM — complete:**
- `ANIMATION/AnimationGraph/` — `MotionGraph`/`MotionGraphLoader` (state
  machine data, loaded via `.dominus`'s `motion_graph` ref),
  `MotionGraphEvaluator` (runtime: current state, `Trigger()`-driven blend
  transitions, auto-on-complete transitions), `AnimationLayerStack`
  (masked/weighted compositing of multiple evaluators)
- `ANIMATION/IK/` — `TwoBoneIK` (analytic solver), `IKChain`/
  `IKChainLoader` (loaded via `.dominus`'s new `ik_chains` ref list)
- `ANIMATION/Retargeting/` — `RetargetMap`/`RetargetMapLoader` (loaded via
  `.dominus`'s new `retarget_map` ref), `Retarget` (renames a clip's
  tracks onto a differently-named skeleton)
- `ANIMATION/ProceduralMotion/ProceduralHooks.h` — a `PoseModifier`
  pipeline (breathing sway and look-at shipped as reference hooks)
- `dominus-cli graph` — new command, drives the live state machine and
  prints every state transition with its timestamp
- Genuinely unresolved: retargeting is name-mapping only (no proportional
  rescaling); IK is two-bone analytic only (no iterative solver for longer
  chains); no 2D blend spaces, single-clip states only. See `ROADMAP.md`
  Phase 2.5 for the full status.

**Phase 3 / DOMINUS COMBAT SYSTEM — foundation complete:**
- `CHARACTER/Genome/CombatIdentity` — style/range/pressure/counter/
  mobility/risk (LAW C002/C003), loaded via `.dominus`'s `combat_dna` ref
- `COMBAT/HitSystem/` — `MoveDef` (frame data, intent, hitboxes),
  `Hurtbox`, `CombatBinder` (loads `combat_dna`/`moves`/
  `physics_rules.hurtbox_ref`), `CollisionEvaluator` (world-space hitbox
  vs hurtbox overlap against a live Pose)
- `COMBAT/ComboSystem/ComboEngine` — cancel/followup legality off real
  move data, `COMBAT/PhysicsCombat/ClashSystem` — attack-vs-attack
  resolution, `COMBAT/ReactionSystem/ReactionSystem` — hit power →
  reaction type → motion-graph trigger
- `COMBAT/CombatController` — the combat state machine, driving the
  **same** `MotionGraphEvaluator` from Phase 2.5 (LAW C012 — never a
  separate animation system)
- `COMBAT/AnimeSpeedSystem` — velocity integrator + afterimage trail
- `AI/Agents/` — `BehaviorTree` + `CombatAI` (opponent pattern tracking →
  counter/block/attack)
- `dominus-cli fight` — new command, runs the full LAW C001 pipeline live
  (Intent → Combat Logic → Motion Request → Skeleton Execution → Collision
  Evaluation → Reaction State)
- Brooklyn's combat genome (LAW C015): style `psycho_drunken_martial_arts`,
  four real moves (`jab`, `dodge`, `counter`, `combo_starter`). Only `jab`
  has a wired motion-graph trigger — the others load correctly but
  `CombatController` honestly refuses to start them rather than faking it
  (LAW C014)
- Genuinely unresolved (as of end of Phase 3): no stylish-action style
  rating, no cinematic clash staging, wall/ground impact reactions aliased
  to hitstun, no transformation system, `CombatIdentityComponent` loaded
  but not read. **All closed in Phase 3.5 below.**

**Phase 3.5 / COMBAT SOUL + CINEMATIC BATTLE LAYER — complete:**
- `CHARACTER/Genome/GenomeDecoder` — decodes `CombatIdentity` into
  numeric `DecisionWeights` (aggression, risk_tolerance, unpredictability,
  counter_bias, defense_bias), closing the LAW C003 gap: wired into
  `ClashSystem::SkillFromWeights` and `ReactionSystem`'s new
  `defense_bias` field (defaults preserve Phase 3's exact old thresholds
  — zero regression)
- `COMBAT/MoveSelector` — scores available moves by genome weights;
  aggressive/risk-tolerant vs cautious fighters demonstrably pick
  differently between the same two real moves
- `COMBAT/ComboSystem/StyleRankSystem` — D through SSS from variety,
  aggression, risk, creativity, and damage avoidance
- `COMBAT/CinematicDirector` — Gameplay → Special Event → Camera → Physics
  → Return Gameplay as a real state machine, optionally hooked into
  `CombatController` (a knockdown auto-triggers a finisher event when a
  director is attached; identical behavior to Phase 3 when it isn't)
- `COMBAT/TransformationSystem` — LAW C010's full genome swap, for real:
  Combat Identity + Motion Graph + Abilities all replaced atomically from
  a `.dominus`-loaded `TransformationDef` (all-or-nothing — a failed load
  leaves the object untouched). Brooklyn's `beast_mode` is the first proof
- `COMBAT/Environment.h` — `EnvironmentBounds`/`DestructionZone` +
  `ApplyEnvironment`, which upgrades a plain reaction into
  `kWallImpact`/`kGroundImpact` when the predicted trajectory crosses a
  boundary — closes the gap Phase 3's `CombatController` flagged as
  aliased-to-hitstun. Feeds through a new `ApplyPrecomputedReaction` seam
  (refactored from `ApplyHit`, verified zero-regression)
- `dominus-cli transform` — new command, runs a live genome swap end to
  end
- Genuinely unresolved (as of end of Phase 3.5): `MoveSelector`/
  `DecisionWeights` not wired into `CombatAI`; `StyleMetrics` no automatic
  collector; transformation only swaps genome/graph/moves; destruction
  zones detected but nothing fires from them. **All closed in Phase 3.75
  below.**

**Phase 3.75 / DOMINUS COMBAT INTEGRATION LAYER — complete:**
- **Priority 1 (AI uses genome)** — `AI/Agents/CombatAI` now carries
  `DecisionWeights` and uses them twice: biasing its own behavior-tree
  branches (a poor counter-fighter, `counter_bias < 0.3`, never attempts
  the counter branch; a highly aggressive fighter, `aggression > 0.8`,
  presses forward instead of blocking) and picking the actual move via
  `COMBAT::MoveSelector` (`CombatAI::DecideMoveName`). Proven: two
  genomes facing the *identical* opponent pattern decide differently —
  the full Player Action → CombatAI → GenomeDecoder → DecisionWeights →
  MoveSelector → Motion Graph → Skeleton Runtime pipeline is verified
  against real Brooklyn data and live via `dominus-cli ai`.
- **Priority 2 (automatic style telemetry)** —
  `COMBAT/ComboSystem/StyleCollector` auto-populates hit/variety/risk/
  damage metrics from real `MoveDef` data instead of hand-typed
  `StyleMetrics`, wired into `CombatController` via `ApplyHit` (auto
  damage-taken) and a new `RecordMoveLanded()`. Dodge/parry/air-time
  recording API exists but is honestly flagged unwired — no detection
  systems exist in-engine yet.
- **Priority 3 (complete transformations)** —
  `COMBAT/TransformationSystem` expanded to swap `COMBAT/Profiles.h`'s
  full five: AI, Physics, Audio, Visual, Camera, each optional (omitted
  profiles leave prior values untouched, verified by test). `AIProfile`
  has a real consumer (`CombatAI::ApplyProfile`); the rest are genuinely
  loaded/attached but flagged inert (no physics/audio/render system
  exists in this engine).
- **Priority 4 (environmental world events)** —
  `COMBAT/WorldEventSystem` bridges `Environment`-upgraded wall/ground
  impacts and destruction-zone hits into `WorldEvent` descriptors
  (destruction/particle/audio/camera tags) — the seam into a future World
  Engine, not an implementation of one.
- `dominus-cli ai` — new command, runs the full genome-driven decision
  pipeline live
- Every change verified zero-regression against every prior phase's
  tests before moving on. 159/159 tests passing (was 133 at end of
  Phase 3.5).
- Genuinely unresolved (as of end of Phase 3.75): dodge/parry/air-time
  telemetry has no automatic trigger; Physics/Audio/Visual/Camera
  profiles have zero consumers; `WorldEvent` has no consumer either; the
  `counter` move's motion-graph state was *still* not wired. **The last
  one — the single most-repeated flagged gap — is closed in Phase 3.9
  below.**

**Phase 3.9 / MOTION LIBRARY COMPLETION — complete:**
- `brooklyn_motion_graph.json` expanded from 2 states/2 transitions to
  **13 states / 47 transitions** — generated programmatically to keep the
  mechanical volume error-free, with the original `idle`⇄`attack`
  transitions preserved byte-identical (verified by diff, not memory)
  so every existing timing-dependent test needed zero behavioral change.
- New states for `counter`, `dodge`, `launcher`, `air_combo`,
  `knockdown_recovery`, and `transformation` — plus `combo_starter`,
  `stagger`, `knockback`, `block_impact`, which had been producing
  triggers since Phase 3 with nowhere to land.
- Two new moves, `launcher` and `air_combo`, complete the combo chain
  `jab → combo_starter → launcher → air_combo`, each a real cancel now.
- `knockdown` genuinely auto-chains through `knockdown_recovery` back to
  `idle` — closes the "aliased to hitstun" gap flagged since Phase 3.
- `COMBAT::AssetValidation` + `dominus-cli validate-motion` — reports
  missing motion-graph coverage before runtime, not after a silent
  refusal in production. Verified: all 6 of Brooklyn's real moves resolve
  with zero gaps.
- Four tests needed updating because the fixture they depend on
  intentionally grew (not regressions) — identified by diffing the
  exact failure set before fixing anything: two exact-count assertions,
  one refusal test rewritten to use a synthetic unwired move (keeps
  proving the LAW C014 refusal behavior generically), and one combo-cancel
  test that flipped from proving a refusal to proving a real cancel.
- 165/165 tests passing (was 159 at end of Phase 3.75).
- Genuinely unresolved: `TransformationSystem::Apply` doesn't
  auto-trigger the `transformation` state before swapping components yet
  (reachable and tested, not wired into the transformation flow itself);
  reaction states aren't chainable from each other (can't be knocked
  down again mid-stagger — `ReactionSystem`'s existing "already
  staggered → launch" escalation handles repeat hits instead); all new
  clips are hand-authored minimal fixtures.

**Architectural correction, before Phase 4:** COMBAT had become the
engine's implicit identity across Phases 3–3.9. That was caught and
corrected — DOMINUS is a universal world/reality substrate; combat is one
domain extension running on it. `COMBAT`/`CHARACTER`/`ANIMATION` are not
physically relocated (moving ~90 files' worth of includes this deep is
real risk for no functional gain) — what actually needed fixing was the
*dependency direction*, and that's what's proven below, not asserted.

**Phase 4.0 / UNIVERSAL WORLD KERNEL — Module 1 complete:**
- `WORLD/Core/SpatialComponent.h` (WORLD LAW 001 — Dimension
  Independence) — one component type serves 2D top-down, 2.5D side-view,
  and 3D third-person, proven by mixing all three in one `World` with
  zero special-casing.
- `WORLD/Core/EntityRegistry.h` (WORLD LAW 002 — Everything Is An Entity)
  — the multi-entity container. `CORE::MetaBinObject` (Phase 1) was
  *already* "Entity + Components"; the actual gap was somewhere to hold
  many together with basic queries, not a new entity type.
- `WORLD/Core/WorldTick.h` + `World.h` (WORLD LAW 003 — Simulation Before
  Rendering) — an ordered list of named systems ticked headlessly. Zero
  render calls anywhere in `WORLD/Core` — verified by grep before any
  test was written.
- **The milestone proof**: Brooklyn — full `.dominus` load, real combat
  genome — runs as a plain world entity. The combat system is built and
  registered as a `WorldSystemFn` from *outside* `WORLD/Core` (proving
  it's a plugin, not a built-in world feature) and drives Brooklyn's real
  `MotionGraphEvaluator` through a full attack cycle, entirely headless.
  Live via `dominus-cli world`.
- Scale evidence: 10,000 entities across 60 ticks, correct and fast — a
  real data point toward the 100k target, not a claim of it.
- 182/182 tests passing (was 165 at end of Phase 3.9).
- Genuinely unresolved (as of end of Phase 4.0): Modules 2–6 (Physics/
  Terrain/Streaming/NPC expansion/ProcGen) not started; entity queries a
  linear scan. **Physics (the first of those) is closed in Phase 4.1
  below.**

**Phase 4.1 / UNIVERSAL PHYSICS LAYER — Module 2 complete:**
- `PHYSICS/RigidBody.h`, `Collider.h`, `PhysicsSystem.h` (gravity + force
  integration), `CollisionSystem.h` (circle-circle detection + layer/mask
  filtering + resolution), `ConstraintSolver.h` (distance constraints) —
  same rule as `WORLD`: verified by grepping actual `#include` lines
  (caught one false-positive from prose-in-comments before it mattered)
  that `PHYSICS` depends on `WORLD/Core` and the standard library only.
- **The milestone proof, deliberately generalized past "Brooklyn punched
  a wall"**: three entities with nothing in common — a real Brooklyn
  (full combat genome), a `Crate` (bare tag, zero combat data), a
  `Vehicle` (unrelated stand-in component) — share only `SpatialComponent`
  + `Collider` + `RigidBody`. Run through `World.Tick()` →
  `PhysicsSystem` → `CollisionSystem`: the fighter's motion pushes the
  crate, all three keep their unrelated identity components completely
  untouched by `PHYSICS`. Live via `dominus-cli physics`.
- 193/193 tests passing (was 182 at end of Phase 4.0).
- Genuinely unresolved: box-box/circle-box collision not implemented
  (`Collider` has the `kBox` shape as data; nothing tests it yet);
  resolution is positional-separation-plus-damping, not a real impulse/
  restitution solver; `CollisionSystem::Detect` is O(n²), same open item
  as `EntityRegistry`'s linear scan; `ConstraintSolver` supports one
  constraint type; `PHYSICS` and `COMBAT` remain fully decoupled — no
  wiring from collision results into `COMBAT`'s own `HitSystem` yet
  (deliberate, to keep the independence proof clean).

**Phase 7 / PACKAGE VALIDATOR — complete:** Not part of the Phase 4.x
World Engine sequence — cross-cutting quality-gate infrastructure,
numbered "Phase 7" to match its source: a "DOMINUS ENGINE v0.1 Vision"
document whose 9-phase character-creation pipeline maps heavily onto
what already exists under different names (its "Compiler"/`.dominus`
package is exactly `CORE::DominusSerializer`, already built since
Phase 1). Its Validator step was the one genuine gap.
- `VALIDATION/PackageValidator` — a new top-level module (terminal
  consumer, depends on everything, nothing depends on it) with four
  checks: identity completeness, asset ownership (every ref path exists
  on disk), naming consistency (`object_id` snake_case + a genuinely new
  catch: move/clip ref-list keys matching their file's own internal
  `name` field), and deterministic rebuild + semver validation.
- **Found a real bug on the first real run, not a staged one**:
  `brooklyn.dominus` had carried a dead `mesh` reference since Phase 1
  (parsed into a component, never consumed by anything since `GRAPHICS`
  doesn't exist, never checked until now). Fixed by removing it.
- `dominus-cli validate-package` — new command, runs the full pipeline.
- 209/209 tests passing (was 193 at end of Phase 4.1).
- Genuinely unresolved: only checks fields the schema actually stores
  (the source vision names ~20 identity fields that don't exist in the
  schema at all); no hash/signature-based package integrity (`.dominus`
  files are loaded as text, never compiled into a hashable binary
  bundle); naming consistency doesn't extend to IK chains/retarget maps/
  transformations; no combat balance checking (would require a balance
  spec that doesn't exist anywhere in this engine).

**Registry Prototype / HASH + IMMUTABLE ARTIFACTS — complete, scoped to
`CombatGenome` only:** A bounded proof-of-concept for one piece of a much
larger proposed architecture (deterministic compilation, content-
addressed immutable artifacts, a registry, version lineage, runtime
snapshots isolated from authored data — 20 ideas in total, roughly an
order of magnitude bigger than any phase before it). Proved on one
genome type before any decision to generalize, per explicit directive.
- `REGISTRY/` — `Sha256.h` (hand-rolled, verified against Python
  `hashlib` ground truth including a multi-block input), `CanonicalSerializer`
  (deterministic, source-formatting-independent bytes), `GenomeCompiler`
  (Validator → Serializer → Hash → `ImmutableArtifact`), `GenomeRegistry`
  (content-addressed, per-entity version lineage), `RuntimeSnapshot`/
  `SnapshotBuilder` (structurally provable — the only way to build one is
  through a compiled artifact, no overload accepts raw source data).
  Depends on `CHARACTER/Genome` only — verified by grep, same discipline
  as every other module boundary here.
- **All 5 claims from the directive independently proven against real
  Brooklyn data**: deterministic compilation, stable/content-addressed
  hashing, registry lookup, version lineage (v1 → v2, parent-linked, v1
  untouched after v2 exists), and runtime never touching authored data.
- `dominus-cli genome-compile` — new command, runs the whole pipeline
  live; the v1 hash it produces is the exact same digest independently
  verified against Python before any C++ ran — an unplanned but genuine
  cross-check that the pipeline is correct end to end.
- 229/229 tests passing (was 209 at end of Phase 7).
- **What matters more than what was built**: the rest of the engine is
  completely unchanged — every other binder still loads directly with no
  hashing/immutability/registry, and no decision has been made to
  generalize this. The prototype answers "does the mechanism work", not
  "should the whole engine work this way".

**Society Phase 0 / WORLD PERSISTENCE LAYER — complete:** A third
sweeping vision document (a "Living Worlds Engine" spanning persistent
economies, AI-driven society, procedural civilizations) explicitly
invoked Q-WEAVE by name — whose own rule is "don't guess and FORGE
blind" on unclassified intent. Assessed against reality (~5% of that
document maps to anything built), then narrowed to the one piece
everyone agreed was the actual prerequisite: **nothing survives process
exit.**
- `WORLD/Core/SourceRefComponent.h` (which `.dominus` file an entity came
  from), `WorldHistory.h` (append-only generic event log), `WorldPersistence.h/.cpp`
  (real `Save()`/`Load()` against disk — `world.json`, `entities/<id>.json`,
  `history/timeline.json`). Reuses `CORE::MiniJson` and `CORE::VoidResult`
  — zero new JSON parser, zero new result type.
- `WORLD/Core` — including persistence — still depends on `CORE` only,
  verified by grep. Reconstructing a fully bound entity on load is
  external orchestration code's job, re-running the *existing*
  `DominusSerializer`/`RigBinder`/`CombatBinder` pipeline — never
  something `WorldPersistence` itself does.
- **The full directive proven as one test**: a real Brooklyn is bound,
  ticked, given history, and saved. That `World` is destroyed entirely.
  A brand-new `World` reloads it, rebinds it, and the reloaded Brooklyn
  *genuinely fights again* — genome correctly picks `counter` against a
  repeated jab pattern, `CombatController` accepts it. Caught and fixed
  one wrong test assumption along the way (expected `jab`, correct
  behavior is `counter` — same logic Phase 3.9 already established).
- `dominus-cli world-save` / `world-load` — new commands, run the full
  cycle live.
- 239/239 tests passing (was 229 at end of the Registry Prototype).
- Genuinely unresolved, stated plainly: no `economy/` or
  `registry/hashes.json` in the actual output (writing empty
  placeholders for systems that don't exist yet would be exactly the
  fake-system pattern this project has avoided throughout — Economy
  doesn't exist, so nothing writes there); no bound-runtime-state
  persistence (a reload always resumes at neutral/idle, not mid-combo);
  no incremental saves, no versioning/migration, no autosave.

**Society Phase 1 / UNIVERSAL ENTITY MODEL — complete:** Evolving the
schema from "Character" to "Existence" — `entity_type`, `provenance`
(the birth certificate), and a real `social_genome`. Scoped against an
8-point proposal: Economy-as-entity and the presentation-layer
abstraction were **not** built (need a real Economy Engine and
`GRAPHICS`, neither of which exist), and Q-WEAVE-as-orchestration is
already what the `q-weave` skill *is* — not duplicated as engine code.
- `CORE::EntityTypeComponent`/`ProvenanceComponent` (free-form tag +
  birth certificate), `CHARACTER::SocialGenome` (personality +
  relationships, same pattern as `CombatIdentity`), `WorldHistory`
  extended with `consequences` + `EventsForEntity()`.
- **A genuine cross-system connection, not staged**: Brooklyn's
  `provenance.creation_hash` is set to the exact SHA-256 digest the
  Registry Prototype's `GenomeCompiler` produces for his real combat
  genome — two systems built in separate phases, designed to compose.
- `dominus-cli social` — new command, prints the fully resolved genome;
  `inspect` now surfaces `entity_type`/`provenance`/the social ref.
- 249/249 tests passing (was 239 at end of Society Phase 0).
- Genuinely unresolved: the data does nothing yet (no AI reads
  trust/aggression/loyalty, no faction system aggregates relationship
  strength) — same honest state `CombatIdentity` was in before
  `GenomeDecoder` existed; relationships are one-directional as stored,
  no reciprocity enforcement; `parent_entities`/`source_assets` aren't
  validated references.

**MONSTERFORGE Phase 1 / CREATUREGENOME SCHEMA — complete:** A
"MonsterForge" document arrived mostly as stat blocks and status
readouts ("Intelligence: 94%", "Threat Level: OMEGA") rather than an
engineering spec. Explicit direction: build the real schema first
(deterministic, strictly validated, separate from generation logic),
layer creative content on top separately — and never embed fictional
"ONLINE" engine status in docs unless a real system produced it.
- `CHARACTER::CreatureGenome` — 12 sections, pure data, same discipline
  as `CombatIdentity`/`SocialGenome`: no number claims to be output
  nothing computed. `CreatureGenomeLoader` — genuinely strict validation
  (required `species_name`, every `[0,1]`-range field checked, *every*
  violation collected, not just the first).
- Wired into the real pipeline — `RigBinder` bind, not a parallel
  system. Found and fixed a real gap in the same session:
  `PackageValidator` was missing `creature_genome` from its
  asset-ownership check.
- `REGISTRY::CanonicalSerializer`/`CreatureGenomeCompiler` — extends the
  hash pipeline to a genome with ~4x `CombatIdentity`'s field count.
  Real evidence, not assertion, that the determinism/hashing discipline
  generalizes; `ImmutableArtifact`'s baked `DecisionWeights` still don't
  (no `CreatureGenome` decoder exists), so the compiler honestly returns
  hash + canonical bytes only rather than faking an artifact.
- The **Flare Stalker** — an original creature tied to HITM CITY's
  existing Long Flare lore, not touching named fighters or the
  flagged-sensitive Rocket thread — plus a deliberately-broken fixture
  proving the validator actually rejects bad data.
- `dominus-cli creature` — new command. 260/260 tests passing (was 249).
- Genuinely unresolved — most of the source proposal, by design: no
  evolution simulation (a real genetic algorithm wasn't attempted), no
  ecosystem generation (the "87,000 species" claim had no algorithm
  behind it), no `CreatureAI` decoder yet, no "Unknown Entity Generator"
  (that was narrative content, not a spec — a fake generator producing
  invented "threat level" percentages would be exactly the placeholder
  pattern this engine has refused throughout).

**MONSTERFORGE Phase 1.5 / SEMANTIC VALIDATION — complete:** "Think like
a compiler" — a genuinely separate second validation stage checking
cross-field biological consistency, distinct from structural range
checks. Explicitly did NOT build the proposed 7-genome decomposition
(`BehaviorGenome`/`CombatGenome`/`EcologyGenome`/etc.) or the "Life
Compiler" mega-vision this pass — real, valuable future work, not
rushed alongside the validator.
- `CreatureGenomeSemanticValidator` — five checks, each mapped to the
  source document's own examples: flight vs. wing area (a new field,
  added so the check is real numeric logic), locomotion vs. limb count,
  diet vs. preferred prey (another new field), growth reaching maturity,
  aquatic locomotion vs. respiration — deliberately a **warning**, not
  an error, since air-breathing aquatic life is real (dolphins).
- A fixture built specifically to prove the point: `impossible_beast`
  passes structural validation cleanly while being biologically
  incoherent in four independent ways — proving structural checks alone
  can't catch what semantic validation is for.
- 12 new tests, 272/272 passing (was 260). Recomputed Flare Stalker's
  hash after the schema grew by three fields — same "computed once,
  pasted in" discipline as every hash in this engine's docs.
- Genuinely unresolved: the 7-genome split, cross-genome validation
  (meaningless until that split happens), multiple registry artifacts
  per creature, and the Life Compiler — all explicitly deferred, not
  attempted.

**MASTER OF COMBAT Phase 1 / COMBATSTYLEGENOME SCHEMA — complete:** Same
"system prompt for an LLM persona" shape as the MonsterForge documents
— applied the same scoping decision without re-litigating it: real
hashable schema, creative-authoring done by hand as content, Simulation
Engine explicitly deferred (large, real, unscoped).
- `CombatIdentity` (LAW C002/C003, embedded since Phase 3) was **not**
  touched — `CombatStyleGenome` is additive, giving
  `CombatIdentity.style`'s previously-undefined string a real,
  structured referent. `ancestry` (parent style names) makes the source
  document's own "combine underlying combat DNA, don't mix names"
  principle literal, structured data.
- `CombatStyleGenomeLoader` — same strict-validation discipline as every
  other loader; `CanonicalSerializer`/`CombatStyleGenomeCompiler` — a
  third data point for the hash-pipeline-generalization question.
- The style itself (`psycho_drunken_martial_arts`) was hand-authored,
  not procedurally generated — same as Flare Stalker.
- **A real, verified connection**: a test proves Brooklyn's actual
  `combat_dna` file's `style` field and this genome's `style_name` are
  equal, loaded from two independent files through two independent
  loaders — not just named alike by coincidence.
- `dominus-cli combat-style` — new command. 8 new tests, 280/280 passing
  (was 272). Caught and fixed a real bug mid-session: a stray duplicate
  code block left over from an earlier edit, caught by compiling
  immediately rather than assuming the edit was clean.
- Genuinely unresolved: no decoder reads the weights into runtime
  behavior yet; no semantic validation for styles; no ancestry
  cross-reference validation; no Combat Simulation Engine; no Non-Human
  Combat Generator (legitimate creative content on request, not a
  "compiler" to fake).

**MASTER OF COMBAT Phase 2 / COMBATPHYSICSGENOME + IMPACTSOLVER —
complete:** Three chained documents ending in a fictional "✅" checklist
claiming things this codebase doesn't have. The assessment mattered more
than usual here: most of the proposals (a Combat State Machine, a
Decision Engine, fighting-game frame data) **restate systems already
built and tested** — `CombatController`'s real 13-state motion-graph
machine, `CombatAI`'s real genome-weighted behavior tree, `MoveDef`'s
real frame data. Building parallel versions would fragment
decision-making, not extend it. **None of them were built.**
- The one genuinely new, non-duplicative gap: `ClashSystem` already
  resolves attack-vs-attack; nothing resolved attack-vs-**body**.
- `CHARACTER::CombatPhysicsGenome` (body/energy/impact, strict
  validation) + `COMBAT::ImpactSolver` — real math (`F=m·v`, threshold-
  based severity, proportional armor reduction, a real per-body-part
  damage multiplier table), deliberately standalone — not wired into
  the live, tested hit-resolution pipeline yet.
- A real chain: Brooklyn's actual physics genome run through a full
  attacker-vs-Brooklyn impact, every number checked by hand against the
  live output before being written into docs (720=90×8, 648=720×0.9,
  1296=648×2.0, ...).
- `REGISTRY` extended to a fourth genome type. `dominus-cli impact` —
  new command. 17 new tests, 297/297 passing (was 280).
- Genuinely unresolved: not wired into `ReactionSystem`/
  `CombatController`; no `RigBinder` entity-binding; no stamina
  depletion over time; no environmental physics; no procedural
  animation generation (what exists plays authored clips through a real
  state machine, it doesn't generate new movement).

**DOMINUS ERA I / GAMEDESIGNGENOME + COHERENCE CHECKER — complete:**
Three-part proposal: `ERA II` (a full commercial runtime — physics,
renderer, networking, GPU pipeline, editor, console exports, VR, cloud,
everything) and `ERA III` (autonomous end-to-end game generation) were
**not attempted** — not tokenized, not partially started. `ERA II` is
an AAA team's multi-year output, not "large but scoped"; `ERA III` is
legitimate creative-collaboration content on request, not something to
fake as a deterministic compiler.
- The genuinely novel part — a **Game Design Genome** cascading
  constraints into every other genome — was taken seriously and built,
  honestly scoped: cut the source document's 20-dimension list to 4
  fields with actual engine hooks (`genre`, `core_loop`, `difficulty`,
  `risk_reward_balance`); the other 16 have no system to constrain yet.
- **Built as advisory, not a hard validator** — a deliberate, important
  distinction from every other checker in this engine. Unlike wing-area-
  vs-flight (real physics), "is this soulslike enough" has no objective
  ground truth. Real criteria only for the two genres the source
  document actually specified; every other genre produces zero notes,
  not a guessed opinion.
- **The real proof, computed not staged**: Brooklyn's actual existing
  style + physics data run against both a soulslike and an arcade
  design — genuinely different results (0 notes vs. 1 note) from the
  exact same fighter, purely from the declared genre.
- `REGISTRY` extended to a fifth genome type. `dominus-cli
  design-coherence` — new command. 13 new tests, 310/310 passing (was
  297).
- Genuinely unresolved: 16 of 20 dimensions unmodeled (no engine hooks);
  only 2 genres have real criteria; Experience Genome and The Dominus
  Constitution are one/two abstraction levels above anything with real
  hooks to constrain, not attempted; `ERA II`/`ERA III` remain entirely
  unbuilt.

**THE DOMINUS PIPELINE / BUILDPIPELINE (LAYERS 2 + 9) — complete:** A
10-layer proposal, mostly already built under different names (Layer 1
= `MetaBinObject`+`EntityTypeComponent`; Layers 4-5 = WORLD LAW 002,
proven since Phase 4.0; Layer 6 = a real, tested `JobSystem` that's
existed since Phase 1, just unused so far) or explicitly out of scope
for reasons already stated this session (Layer 7 needs `GRAPHICS`,
Layer 8 needs a whole GUI app, Layer 3's full binary-compiled-format
claim is the same "should Registry become the load path" decision
flagged as open three times already).
- **A real, small Layer 2 fix**: the document's own literal example —
  "Combat Style → No Weakness → FAIL" — was true friction:
  `CombatStyleGenomeLoader` never actually required non-empty
  `weaknesses`. Fixed.
- **`VALIDATION::BuildPipeline`** — pure orchestration of the already-
  real `PackageValidator`: discovers every `.dominus` file under a
  project, validates each, reports one `BUILD PASSED`/`BUILD FAILED`
  verdict. `dominus-cli build` — new command.
- **Found real signal on the first live run, not staged**: run against
  this engine's own `tests/fixtures`, it found 2 genuine failures out
  of 7 real files — including `missing_identity.dominus`, a
  pre-existing fixture the pipeline discovered and correctly flagged
  without it being specifically targeted going in.
- 5 new tests, 315/315 passing (was 310) — including a test proving the
  pipeline isn't hardcoded to always fail (a cleaned copy of the
  fixtures directory reports a genuine `BUILD PASSED`).
- Genuinely unresolved: no genome-JSON auto-discovery (no self-
  describing type marker the way `.dominus` files have `entity_type`);
  no compiled binary output; `JobSystem` remains real but unused in the
  live tick pipeline.

**DORRE / VISUALGENOME + REAL MEMORY CONNECTION — complete:** A full
rendering-engine proposal (geometry compilation, adaptive materials,
neural lighting, style fusion, cinematic camera AI, GPU compute
pipeline). Almost entirely not attempted, same reasoning as `ERA II` —
`GRAPHICS` is still an empty placeholder, and everything past basic
data description needs an actual renderer. Two more items declined on
epistemic grounds specifically: **Style Fusion** (no objective criteria
to validate a "50% Samurai + 20% Cyberpunk" percentage against) and
**Entity Importance Engine** (nothing exists yet to consume an
importance score).
- What was genuinely just data — `CHARACTER::VisualGenome`
  (form/skin/clothing/presence) — got built, sixth genome type through
  the `REGISTRY` hash pipeline.
- **"Objects remember" was NOT built as a second fabricated history
  system** — `VisualMemoryDeriver` connects to the real
  `WORLD::WorldHistory` (Society Phase 0) instead, proven to correctly
  filter one entity's events from another's, not just sum the whole
  log.
- Brooklyn's real visual genome, hand-authored to match his established
  aesthetic. `dominus-cli visual` — new command, shows the genome, its
  hash, and a real derived memory summary.
- 9 new tests, 324/324 passing (was 315).
- Genuinely unresolved: no rendering exists at all (this describes what
  SHOULD eventually render); memory summaries are thin (count + first/
  last timestamp) since `WorldHistory` events are free-form strings, not
  structured data richer stats would need; not bound to an entity via
  `RigBinder` yet, same state as the three prior standalone genomes.

**REALITY DESCRIPTION FOUNDATION / VISUALSTYLEGENOME + MATERIALGENOME +
ENTITY BINDING — complete:** A revised rendering document that first
validated the previous phase's biggest call (WorldHistory over a fake
memory system), then asked for three correctly-scoped follow-ups.
- **Closed `VisualGenome`'s "not bound yet" gap** — wired through
  `RigBinder` exactly like `SocialGenome`/`CreatureGenome`. Proactively
  added `visual_genome` to `PackageValidator`'s asset check *before*
  anyone found the gap, learning from missing `creature_genome` there
  last time. Brooklyn's real flagship fixture now carries the ref,
  proven end-to-end with a full regression rerun.
- **`MaterialGenome` + `MaterialWearDeriver`** — the document's own
  jacket example, plus a second proof that deriving state from real
  `WorldHistory` (not fabricating it) generalizes beyond the first case.
- **A real bug caught and fixed on the spot**: the first wear-deriver
  implementation matched event types by substring, which incorrectly
  counted a test event named `"non_damage_event"` as a damage event.
  Fixed to exact-match; not glossed over.
- **`VisualStyleGenome`** — art style as comparable, inheritable data
  only, explicitly not an auto-combine algorithm (same epistemic
  reasoning as declining Style Fusion last phase, applied consistently).
- **A third real cross-check**: added `VisualGenome.presence.style_id`
  specifically so it could be proven to agree with
  `VisualStyleGenome.style_id`, same shape as the
  `CombatStyleGenome`↔`CombatIdentity` connection, proven on Brooklyn's
  real fixtures.
- `dominus-cli material` and `dominus-cli visual-style` — new commands.
  31 new tests, 343/343 passing (was 324).
- Genuinely unresolved: `MaterialGenome`/`VisualStyleGenome` not bound
  to an entity yet; no cross-reference validation on `influences`; wear
  heuristic stays simple until `WorldHistory` carries structured
  payloads; no Visual Compiler or Graphics Engine (both explicitly
  "future"/"much later" even in the source document).

## What's actually built right now

**Phase 1 / CORE — complete:**
- `CORE/Memory/Allocator.h` — arena allocator
- `CORE/MetaBin/MetaBinObject.h` — component-based object graph node
- `CORE/Serialization/` — `.dominus` load/save/validate (dependency-free
  hand-rolled JSON in `MiniJson.h`)
- `CORE/Runtime/` — `Application`, `JobSystem`, `EcsWorld`

**Phase 2 / CHARACTER SYSTEM — complete (2D skeletal, see note below):**
- `ANIMATION/SkeletonSystem/` — `Skeleton`, `Bone`, `Transform2D`,
  `AnimationClip`, loaders for both
- `ANIMATION/ProceduralMotion/AnimationPlayer.h` — samples a clip against a
  skeleton, composes world-space poses through the bone hierarchy
- `CHARACTER/Rig/RigBinder.h/.cpp` — resolves the generic refs CORE carries
  into real components on a `.dominus` object (skeleton, animations,
  motion graph, IK chains, retarget map — everything below)

**Tooling:**
- `TOOLS/Editor/dominus_cli.cpp` — headless CLI: `inspect`, `validate`,
  `play`, `graph`, `fight`, `transform`, `ai`, `validate-motion`, `world`,
  `physics`, `validate-package`, `genome-compile`, `world-save`,
  `world-load`, `social`, `creature`, `combat-style`, `impact`,
  `design-coherence`, `build`, `visual`, `material`, `visual-style`

**Content:**
- `tests/fixtures/brooklyn.dominus` — the flagship HITM CITY fixture:
  full skeleton, animations, motion graph (13 states/47 transitions),
  combat genome (6 moves), one transformation, runnable as a `WORLD`
  entity with real `PHYSICS` components attached, validated clean by
  `PackageValidator`, its `combat_dna` compilable through the `REGISTRY`
  hash pipeline, genuinely survivable across a full save/destroy/reload
  cycle, and now carrying `entity_type`/`provenance`/a real
  `social_genome`
- `tests/fixtures/brooklyn_social.json` — Brooklyn's social genome: a
  rivalry and an alliance, real relationship data
- `tests/fixtures/psycho_drunken_martial_arts_style.json` — Brooklyn's
  combat style itself, structured and hashable, verified to match his
  `combat_dna` file's `style` string exactly
- `tests/fixtures/brooklyn_combat_physics.json` — Brooklyn's real body/
  energy/impact data, run through a full attacker-vs-Brooklyn impact
  chain with every result number hand-verified
- `tests/fixtures/design_soulslike.json` + `design_arcade.json` — the
  same Brooklyn style+physics data run against both, proving a real,
  computed cascade (0 notes vs. 1 note) from the same fighter
- `tests/fixtures/flare_stalker.dominus` + `flare_stalker_creature.json`
  — an original HITM CITY creature (Long Flare lore, not touching any
  named fighter), full `CreatureGenome`, compilable through the same
  hash pipeline
- `tests/fixtures/broken_creature_genome.json` — deliberately broken
  (missing species_name, out-of-range weights), proving strict
  validation actually rejects bad data
- `tests/fixtures/impossible_beast.dominus` +
  `semantically_broken_creature.json` — structurally clean, biologically
  incoherent in four independent ways, proving semantic validation
  catches what structural checks can't
- `tests/fixtures/broken_combat_style.json` — deliberately broken
  (missing style_name, out-of-range weights), same strict-validation
  proof for combat styles
- `tests/fixtures/broken_combat_physics.json` — deliberately broken
  (negative mass, out-of-range armor, zero stamina, negative durability),
  same strict-validation proof for combat physics
- `tests/fixtures/broken_game_design.json` — deliberately broken
  (missing genre, out-of-range difficulty/risk_reward_balance), same
  strict-validation proof for game design
- `tests/fixtures/brooklyn_visual.json` — Brooklyn's real visual genome,
  hand-authored to match his established HITM CITY aesthetic
- `tests/fixtures/broken_visual_genome.json` — deliberately broken
  (five independent out-of-range values), same strict-validation proof
  for visual data
- `tests/fixtures/brooklyn_jacket_material.json` — Brooklyn's jacket,
  matching the source document's own worked example (`MAT-JACKET-001`)
- `tests/fixtures/broken_material_genome.json` — deliberately broken
  (missing material_id, negative age, out-of-range wear_state)
- `tests/fixtures/brooklyn_visual_style.json` — Brooklyn's visual style
  ("Urban Combat"), verified to match his real `visual_genome`'s
  `style_id` exactly
- `tests/fixtures/broken_visual_style.json` — deliberately broken
  (missing style_id and name)
- `tests/fixtures/ik_test_rig.dominus` — isolated 3-bone
  shoulder/elbow/wrist chain proving the IK solver
- `tests/fixtures/generic_biped.dominus` — a differently-named skeleton
  proving Brooklyn's clips retarget correctly
- `tests/fixtures/broken_missing_skeleton.dominus` — deliberately broken,
  proving the validator degrades to a report instead of crashing

`tests/` — 343 unit tests covering allocator, object graph, serializer
round-trip/validation, the application frame loop, skeleton loading and
hierarchy composition, clip sampling/looping/clamping, rig binding and
playback, the motion graph state machine and blend transitions, layered
compositing, two-bone IK (both the pure solver and the full `.dominus`
pipeline), retargeting, procedural hooks, one end-to-end motion
integration test, the full Phase 3 combat suite (move/hurtbox loading,
collision detection, combo cancel legality, clash resolution, reaction
determination, the combat controller integrated with the real motion
graph, anime speed simulation, behavior-tree combat AI), the Phase 3.5
soul layer (genome decoding, genome-driven clash/reaction/move-selection,
style ranking, the cinematic director, environmental reaction upgrades,
and the transformation system's full genome-swap proof), the Phase 3.75
integration layer (genome-driven AI decisions proven against identical
opponent patterns, automatic style telemetry, the five-profile
transformation expansion, and the wall/ground-impact-to-world-event
bridge), the Phase 3.9 motion library completion (asset validation
coverage checks, the full jab→combo_starter→launcher→air_combo combo
chain, knockdown→recovery→idle, and the counter move's complete
decision-to-skeleton pipeline), the Phase 4.0 world kernel (entity
registry, headless world tick, dimension-independent spatial components,
and Brooklyn running as a real world entity with combat wired in as an
external plugin system), the Phase 4.1 universal physics layer
(gravity/force integration, collision detection/resolution, distance
constraints, and the three-unrelated-entities proof that physics never
learns any of their identities), the Phase 7 package validator
(identity/asset-ownership/naming/integrity checks, each proven against
both a failure case and a pass case, plus Brooklyn's real fixture
validated clean end to end), the Registry prototype (hand-rolled SHA-256
verified against independently-computed ground truth, and all five
deterministic-compilation/stable-hashing/registry-lookup/version-
lineage/runtime-isolation claims proven against real Brooklyn genome
data), Society Phase 0's world persistence layer (save/load
round-trips for entities, spatial data, elapsed time, and history, plus
the full save-destroy-reload-refight proof against a real Brooklyn),
Society Phase 1's Universal Entity Model (`entity_type`/`provenance`/a
real social genome, loaded and bound against Brooklyn's actual fixture
data, plus `WorldHistory`'s consequences and entity-filtered queries),
MONSTERFORGE Phase 1's `CreatureGenome` schema (strict validation
against both a real creature and a deliberately-broken one, canonical
serialization and hashing extended to a second, much richer genome
type, and full `RigBinder` integration), MONSTERFORGE Phase 1.5's
semantic validator (five real cross-field biological consistency
checks, each proven positive and negative, plus a fixture built
specifically to prove structural validation alone can't catch what
semantic validation catches — four planted contradictions found in one
pass), MASTER OF COMBAT Phase 1's `CombatStyleGenome` schema (strict
validation, canonical serialization and hashing extended to a third
genome type, and a real cross-check proving Brooklyn's actual
`combat_dna` file and this new style genome agree on the style name,
loaded through two independent code paths), MASTER OF COMBAT Phase 2's
`CombatPhysicsGenome` + `ImpactSolver` (strict validation, a fourth
genome type through the hash pipeline, and nine tests proving the real
force/severity/armor/damage math — including the full chain against
Brooklyn's actual physics data with every result hand-verified),
DOMINUS ERA I's `GameDesignGenome` + `GameDesignCoherenceChecker` (a
fifth genome type through the hash pipeline, and a real cross-genre
proof that Brooklyn's existing style+physics data produces genuinely
different, computed advisory results depending purely on a declared
genre — 0 notes for soulslike, 1 for arcade), The Dominus Pipeline's
`BuildPipeline` (real orchestration of `PackageValidator` across an
entire project directory, proven against the engine's own fixtures —
including a genuine unstaged discovery of a pre-existing broken fixture
— and proven NOT hardcoded to always fail via a cleaned copy that
correctly reports BUILD PASSED), DORRE's `VisualGenome` +
`VisualMemoryDeriver` (a sixth genome type through the hash pipeline,
and a real connection to `WorldHistory` proven to correctly filter one
entity's events from another's rather than fabricating "battles: 400+"
out of nothing), and Reality Description Foundation's `VisualGenome`
entity binding + `MaterialGenome`/`MaterialWearDeriver` (a second real
WorldHistory-derivation proof, including a genuine substring-matching
bug caught and fixed during testing) + `VisualStyleGenome` (a third
real cross-check, verified against Brooklyn's actual fixtures).

All of the above has been compiled and run in this environment — this is
not pseudocode. See "Build & Test" below to reproduce.

`GRAPHICS` remains a placeholder directory with a `README.md` explaining
its phase gate. `WORLD/{Terrain,NPC,Simulation}` are likewise still
placeholders — only `WORLD/Core` (Module 1, now including persistence)
and `PHYSICS` (Module 2) are built; see `ROADMAP.md` Phase 4.1 for
exactly what's deferred and why. Per the Constitution's Law 6, do not
build ahead of the gate — Phase 4.2 (Terrain) is next.

## Build & Test

CMake is the intended build system:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

This environment didn't have `cmake` installed, so the scaffold was
verified directly with g++ instead — reproduce with:

```
g++ -std=c++20 -Wall -Wextra -I. -o dominus_core_tests \
  CORE/Serialization/DominusSerializer.cpp \
  ANIMATION/SkeletonSystem/SkeletonLoader.cpp \
  ANIMATION/SkeletonSystem/AnimationClipLoader.cpp \
  ANIMATION/AnimationGraph/MotionGraphLoader.cpp \
  ANIMATION/IK/IKChainLoader.cpp \
  ANIMATION/Retargeting/RetargetMapLoader.cpp \
  CHARACTER/Rig/RigBinder.cpp \
  CHARACTER/Genome/CombatIdentityLoader.cpp \
  CHARACTER/Genome/SocialGenomeLoader.cpp \
  CHARACTER/Genome/CreatureGenomeLoader.cpp \
  CHARACTER/Genome/CombatStyleGenomeLoader.cpp \
  CHARACTER/Genome/CombatPhysicsGenomeLoader.cpp \
  CHARACTER/Genome/GameDesignGenomeLoader.cpp \
  CHARACTER/Genome/VisualGenomeLoader.cpp \
  CHARACTER/Genome/MaterialGenomeLoader.cpp \
  CHARACTER/Genome/VisualStyleGenomeLoader.cpp \
  COMBAT/HitSystem/MoveLoader.cpp \
  COMBAT/HitSystem/HurtboxLoader.cpp \
  COMBAT/HitSystem/CombatBinder.cpp \
  COMBAT/TransformationSystem.cpp \
  VALIDATION/PackageValidator.cpp \
  VALIDATION/BuildPipeline.cpp \
  WORLD/Core/WorldPersistence.cpp \
  tests/main.cpp tests/core/*.cpp tests/animation/*.cpp \
  tests/character/*.cpp tests/motion/*.cpp tests/combat/*.cpp \
  tests/soul/*.cpp tests/integration/*.cpp tests/world/*.cpp \
  tests/physics/*.cpp tests/validation/*.cpp tests/registry/*.cpp \
  tests/genome/*.cpp \
  -lpthread
./dominus_core_tests
```

```
g++ -std=c++20 -Wall -Wextra -I. -o dominus-cli \
  CORE/Serialization/DominusSerializer.cpp \
  ANIMATION/SkeletonSystem/SkeletonLoader.cpp \
  ANIMATION/SkeletonSystem/AnimationClipLoader.cpp \
  ANIMATION/AnimationGraph/MotionGraphLoader.cpp \
  ANIMATION/IK/IKChainLoader.cpp \
  ANIMATION/Retargeting/RetargetMapLoader.cpp \
  CHARACTER/Rig/RigBinder.cpp \
  CHARACTER/Genome/CombatIdentityLoader.cpp \
  CHARACTER/Genome/SocialGenomeLoader.cpp \
  CHARACTER/Genome/CreatureGenomeLoader.cpp \
  CHARACTER/Genome/CombatStyleGenomeLoader.cpp \
  CHARACTER/Genome/CombatPhysicsGenomeLoader.cpp \
  CHARACTER/Genome/GameDesignGenomeLoader.cpp \
  CHARACTER/Genome/VisualGenomeLoader.cpp \
  CHARACTER/Genome/MaterialGenomeLoader.cpp \
  CHARACTER/Genome/VisualStyleGenomeLoader.cpp \
  COMBAT/HitSystem/MoveLoader.cpp \
  COMBAT/HitSystem/HurtboxLoader.cpp \
  COMBAT/HitSystem/CombatBinder.cpp \
  COMBAT/TransformationSystem.cpp \
  VALIDATION/PackageValidator.cpp \
  VALIDATION/BuildPipeline.cpp \
  WORLD/Core/WorldPersistence.cpp \
  TOOLS/Editor/dominus_cli.cpp -lpthread
./dominus-cli ai tests/fixtures/brooklyn.dominus
```

Last verified run: 343/343 tests passed, zero compiler warnings under
`-Wall -Wextra -Wpedantic`. Live CLI output for the command above:

```
[genome] style='psycho_drunken_martial_arts'
[weights] aggression=0.9 risk_tolerance=0.5 counter_bias=0.9
[player action] opponent has thrown 3x 'jab'
[ai decision] category='counter' chosen_move='counter'
[motion] StartMove('counter') -> accepted
[state] entering Counter
[animation] counter.anim
[result] combat pipeline complete
```

The combat loop is fully closed as of Phase 3.9 — Brooklyn's genome
decides to counter, and that decision now genuinely reaches the skeleton
runtime. Verify the whole roster at once with `dominus-cli validate-motion`:

```
./dominus-cli validate-motion tests/fixtures/brooklyn.dominus
checked 6 move(s) against 47 motion graph transition(s)
OK: every move resolves to a valid motion state.
```

Phase 4.0's own milestone, `dominus-cli world` — Brooklyn running as a
headless world entity with combat wired in as an external system, not a
built-in world feature:

```
./dominus-cli world tests/fixtures/brooklyn.dominus
[world] entity 'brooklyn' created, dimension=2.5D
[world] registered system 'combat_extension' (1 total)
[world] ticking headless, no rendering...
[world] elapsed=1s state='idle'
[result] brooklyn ran as a world entity; COMBAT never modified WORLD/Core
```

Phase 4.1's milestone, `dominus-cli physics` — a real fighter and an
unrelated crate resolved through generic physics alone:

```
./dominus-cli physics tests/fixtures/brooklyn.dominus
[physics] entities: 'brooklyn' (Fighter, has CombatIdentity) and 'crate_001' (Crate, no combat data)
[physics] registered systems: 2 (physics never includes COMBAT/CHARACTER headers)
[physics] crate.x: 1.5 -> 1.79167 (moved=true)
[result] fighter and crate resolved through PHYSICS alone -- physics never knew either identity
```

Phase 7's own check, `dominus-cli validate-package` — the same command
that found and helped fix a real dead-reference bug in Brooklyn's fixture
during development:

```
./dominus-cli validate-package tests/fixtures/brooklyn.dominus
[validate-package] tests/fixtures/brooklyn.dominus
  errors=0 warnings=0
[result] package passed validation
```

The Registry Prototype's own proof, `dominus-cli genome-compile` — the
full source-to-snapshot pipeline, live, against real Brooklyn combat
genome data (base form v1, `beast_mode` v2 as its child):

```
./dominus-cli genome-compile tests/fixtures
[source] brooklyn_combat.json -> style='psycho_drunken_martial_arts'
[compile] v1 hash=10bbf5ea81f2f87e2e3ad66a0d66da7c5eb28465209c0024f3d9baf8dbc616b5 (canonical: style=psycho_drunken_martial_arts;range=close;pressure=relentless;counter=expert;mobility=unpredictable;risk=medium)
[registry] registered v1, artifact_count=1
[determinism] recompiled v1 -> hash=10bbf5ea81f2f87e2e3ad66a0d66da7c5eb28465209c0024f3d9baf8dbc616b5 (matches=true), artifact_count still=1
[compile] v2 hash=23ec30e15d52243174cb39dbcdc180f3093a1bae57645a80f16a0dd4545e5a92 parent=10bbf5ea81f2f87e2e3ad66a0d66da7c5eb28465209c0024f3d9baf8dbc616b5
[lineage] brooklyn: 2 version(s)
  v1: 10bbf5ea81f2f87e2e3ad66a0d66da7c5eb28465209c0024f3d9baf8dbc616b5
  v2: 23ec30e15d52243174cb39dbcdc180f3093a1bae57645a80f16a0dd4545e5a92
[snapshot] v1 aggression=0.9 | latest(v2) aggression=1
[result] deterministic compilation + stable hashing + registry lookup + version lineage + runtime-never-touches-authored-data all proven against real Brooklyn combat genome data
```

Society Phase 0's own proof, `dominus-cli world-save` / `world-load` —
the complete "creator leaves, creator returns, world continues" cycle:

```
./dominus-cli world-save tests/fixtures/brooklyn.dominus /tmp/dominus_world_state_demo
[save] entity='brooklyn' elapsed=60s history_events=2
[save] wrote /tmp/dominus_world_state_demo/world.json, entities/brooklyn.json, history/timeline.json
[result] world saved -- creator can leave now

./dominus-cli world-load /tmp/dominus_world_state_demo tests/fixtures
[load] read 1 entity record(s), 2 history event(s), elapsed=60s
[rebind] 'brooklyn' <- brooklyn.dominus (fully bound again)
[history] t=0s entity_created (brooklyn): brooklyn enters the city
[history] t=60s battle_won (brooklyn): Defeated the gang leader
[result] world continues -- 1 entity(ies), elapsed=60s, 2 remembered event(s)
```

Society Phase 1's own proof, `dominus-cli social` — the fully resolved
genome, not just a ref path:

```
./dominus-cli social tests/fixtures/brooklyn.dominus
[social] 'brooklyn' personality: trust=0.3 aggression=0.8 loyalty=0.7
[social] 2 relationship(s):
  iron_wolves_leader: rival (strength=-85)
  static: ally (strength=60)
[result] social genome resolved -- brooklyn is not a quest marker, it has relationships
```

MONSTERFORGE Phase 1's own proof, `dominus-cli creature` — a real
schema, hashed, not a stat block:

```
./dominus-cli creature tests/fixtures/flare_stalker.dominus
[creature] Flare Stalker (flare_stalker)
[creature] taxonomy: apex_predator, quadrupedal_hybrid, 6 limbs
[creature] cognition: tier=2 problem_solving=0.55
[creature] combat_profile: aggression=0.75 speed=0.7 durability=0.6 range=close
[creature] ecology: apex_predator in HITM City undercity tunnels
[creature] genome_hash=2d88579e3cc5abbdd7f642f465f644a9410571719ee5aa34f9e87ea5cca5efdb
[creature] semantic validation: 0 issue(s)
[result] creature genome resolved and hashed -- a real schema, not a stat block
```

MONSTERFORGE Phase 1.5's own proof — structurally clean, biologically
incoherent in four independent ways, all caught in one pass:

```
./dominus-cli creature tests/fixtures/impossible_beast.dominus
[creature] Impossible Beast (impossible_beast)
[creature] taxonomy: apex_predator, biped, 2 limbs
[creature] semantic validation: 4 issue(s)
  [ERROR] flight_wing_area: wing_area_m2 (0.200000) is too small to support weight_kg (400.000000) for declared flight -- exceeds the 50.000000 kg/m^2 heuristic
  [ERROR] locomotion_limb_count: locomotion.primary_mode is quadrupedal but taxonomy.limb_count is 2 (need >= 4)
  [ERROR] diet_vs_prey: ecology.diet_type is herbivore but preferred_prey is non-empty
  [ERROR] growth_maturity: growth.maturity_age_years (12.000000) >= lifespan_years (5.000000) -- this creature never reaches reproductive maturity
[result] creature genome resolved and hashed -- a real schema, not a stat block
```

MASTER OF COMBAT Phase 1's own proof, `dominus-cli combat-style` — a
real, hashable style definition, not a name mixed with adjectives:

```
./dominus-cli combat-style tests/fixtures/psycho_drunken_martial_arts_style.json
[combat-style] psycho_drunken_martial_arts
[combat-style] ancestry: drunken_boxing, capoeira, catch_wrestling, silat
[combat-style] aggression=0.85 defense=0.4 mobility=0.9 deception=0.95 adaptability=0.9
[combat-style] philosophy: predictability is death; let chaos read the opponent's timing before they read yours
[combat-style] 3 known weakness(es)
[combat-style] genome_hash=84cf2659b573d041e2d9479dadad026ac58a3563fe6ef1b2af1721cddf5508ce
[result] combat style resolved and hashed -- a real schema, not a name mixed with adjectives
```

MASTER OF COMBAT Phase 2's own proof, `dominus-cli impact` — real force/
impact/damage math, every number traceable to an input:

```
./dominus-cli impact tests/fixtures/brooklyn_combat_physics.json
[impact] defender: mass=78kg armor=0.1 durability=55
[impact] attacker: mass=90kg velocity=8m/s
[impact] force=720 resistance=55 difference=665 severity=HIGH
[impact] post-armor force=648 (armor absorbed 72)
[impact]   if struck in the head: damage=1296
[impact]   if struck in the torso: damage=648
[impact]   if struck in the arm: damage=388.8
[impact]   if struck in the leg: damage=518.4
[result] every number above traces back to mass/velocity/armor/durability -- nothing invented
```

DOMINUS ERA I's own proof, `dominus-cli design-coherence` — the same
fighter data, two genres, genuinely different computed results:

```
./dominus-cli design-coherence tests/fixtures/design_soulslike.json tests/fixtures/psycho_drunken_martial_arts_style.json tests/fixtures/brooklyn_combat_physics.json
[design] genre='soulslike'
[design] 0 coherence note(s) for genre 'soulslike':
  (none -- this fighter's data reads as coherent with the declared genre by these checks)

./dominus-cli design-coherence tests/fixtures/design_arcade.json tests/fixtures/psycho_drunken_martial_arts_style.json tests/fixtures/brooklyn_combat_physics.json
[design] genre='arcade'
[design] 1 coherence note(s) for genre 'arcade':
  [physics.energy.fatigue_rate] arcade design typically wants low stamina friction -- current fatigue_rate (1.300000) is above 1.0, may feel punishing for the intended pace
```

The Dominus Pipeline's own proof, `dominus-cli build` — real
orchestration across every `.dominus` file in a project, found genuine
signal on the first run against this engine's own fixtures:

```
./dominus-cli build tests/fixtures
[build] scanning tests/fixtures for .dominus files...
[FAIL] tests/fixtures/broken_missing_skeleton.dominus
    asset_ownership: object 'broken_missing_skeleton' field 'skeleton' references a missing file: does_not_exist.skel.json
[PASS] tests/fixtures/brooklyn.dominus
[PASS] tests/fixtures/flare_stalker.dominus
[FAIL] tests/fixtures/missing_identity.dominus
    integrity: failed to load: Validation failed: Missing required field: identity;
[build] 7 file(s) checked, 2 failed
[result] BUILD FAILED
```

DORRE's own proof, `dominus-cli visual` — a real schema plus a real
memory connection, no fabricated "battles: 400+":

```
./dominus-cli visual tests/fixtures/brooklyn_visual.json
[visual] form: heavy_fighter, street_brawler, aggressive
[visual] presence: aura=chaotic threat_signature=0.8 style_id=STYLE-URBAN-COMBAT
[visual] genome_hash=6ee8680d16ce02b374dbcf210ea0608a2526240375edf15f677698cd93893cfa
[visual] memory: 3 real recorded event(s) (first=0s, last=60s)
[result] visual genome resolved and hashed; memory derived from real WorldHistory, not invented
```

Reality Description Foundation's own proof — real derived wear, and a
real cross-check between two independently-loaded files:

```
./dominus-cli material tests/fixtures/brooklyn_jacket_material.json
[material] MAT-JACKET-001 (urban_leather)
[material] derived wear_state from 2 real damage_event(s): 0.1
[result] material genome resolved and hashed; wear derived from real WorldHistory, not invented

./dominus-cli visual-style tests/fixtures/brooklyn_visual_style.json
[visual-style] Urban Combat (STYLE-URBAN-COMBAT)
[visual-style] influences: boxing, drunken_kung_fu
[result] visual style resolved and hashed; influences are data, not an auto-combine result
```

(`PHYSICS` and `REGISTRY` are header-only; `WORLD/Core` is not, as of
the persistence layer — it needs `WORLD/Core/WorldPersistence.cpp`
added to both build commands above; `VALIDATION` needs both
`VALIDATION/PackageValidator.cpp` and `VALIDATION/BuildPipeline.cpp`;
`CHARACTER` needs all eight genome loaders — `CombatIdentityLoader.cpp`,
`SocialGenomeLoader.cpp`, `CreatureGenomeLoader.cpp`,
`CombatStyleGenomeLoader.cpp`, `CombatPhysicsGenomeLoader.cpp`,
`GameDesignGenomeLoader.cpp`, `VisualGenomeLoader.cpp`,
`MaterialGenomeLoader.cpp`, and `VisualStyleGenomeLoader.cpp` — same as
any other binder. `CreatureGenomeSemanticValidator.h`,
`GameDesignCoherenceChecker.h`, `VisualMemorySummary.h`,
`MaterialWearDeriver.h`, and `COMBAT/PhysicsCombat/ImpactSolver.h` are
all header-only, no new `.cpp` to add. Every CLI command above uses the
same binary.)

## Relationship to existing HITM CITY skills

Dominus Engine does not replace `combat-genome-director`,
`hitm-character-forge`, `hitm-animation-director`, `image-to-rig`,
`spine-fighting-game`, `unreal-combat-architect`, or `hitm-engine`. Those
skills are the organs; Dominus Engine is the substrate they plug into. The
Brooklyn skeleton/clip/graph/combat/transformation fixtures here are
hand-authored stand-ins for what those skills would actually produce — the
loader contracts don't change when real content replaces them.

## Next ASCEND candidate

Thirteen real candidates, in rough priority order:

0. **Society Phase 2 — Simulation Clock**: now genuinely unblocked
   (Phase 1's genome data and `WorldHistory` exist to give a clock
   something to drive). "1 second real time = 10 minutes simulation
   time," entities perform actions (work/travel/train/age) — the
   directive's own recommended next step, and correctly not skipped to
   before Historical/Social Genome existed.
1. **Decide whether to wire `ImpactSolver` into the live combat
   pipeline** — not a build task, a design decision, same shape as the
   Registry generalization question below. The math is real and tested
   standalone; would it replace `ReactionSystem`'s current damage
   handling, augment it, or run alongside it for a different game mode?
   Answering that before wiring anything in avoids breaking tested,
   working combat code (Phase 3-3.9) for an integration nobody's
   actually specified yet.
2. **The `CreatureGenome` 7-genome decomposition** — split the current
   12-section monolith into `BehaviorGenome`/`CombatGenome`/
   `EcologyGenome`/`GrowthGenome`/`MutationGenome`/`ReproductionGenome`
   (plus reusing `SocialGenome`), with a real Cross-Genome Validation
   stage checking consistency BETWEEN them. Deliberately deferred out of
   the same pass as the semantic validator so it gets its own dedicated
   design pass — and arguably should be informed by which of the current
   12 sections' fields turned out to interact with each other during
   semantic-check design. Naming collision worth resolving during that
   pass: this decomposition's proposed `CombatGenome` and MASTER OF
   COMBAT's `CombatStyleGenome`/`CombatPhysicsGenome` are different
   concepts (per-creature fighting capability, a style's own identity,
   and physical combat traits) that would want a clearer naming
   distinction once all four exist side by side.
3. **Wire consumers for the Social, Creature, Combat Style, and Combat
   Physics Genome data** — `trust`/`aggression`/`loyalty`, relationship
   strength, `CreatureCombatProfile`'s weights, `CombatStyleGenome`'s
   weights, and `CombatPhysicsGenome`'s stamina/fatigue data currently
   do nothing beyond `ImpactSolver`'s standalone math. The same gap
   `CombatIdentity` had before `GenomeDecoder`/`CombatAI` existed to
   interpret it.
4. **Decide whether to generalize the Registry Prototype** — not a build
   task, a decision. The prototype proved the hash/immutable-artifact
   mechanism works on `CombatGenome`. Whether to extend it to Motion/
   Physics/Behavior/Audio/Visual/Camera genomes, add the Dependency Graph
   (the proposal's own recommended step 2), and eventually the Multi-
   Genome naming cleanup (step 3) is a real fork this session deliberately
   didn't resolve on its own — "the mechanism works" and "commit the
   whole engine to it" are different questions.
5. **Phase 4.2 — Terrain System**: 2D TileMap / 2.5D layered / 3D
   voxel-or-mesh, behind one `TerrainProvider` interface. The natural
   next World module now that Module 2 (Physics) is proven —
   `PHYSICS/Collider`/`RigidBody` are exactly what terrain colliders
   would attach, and `PhysicsSystem`'s gravity already has something to
   land on once terrain exists.
6. **Wire `TransformationSystem::Apply` to auto-fire the `transformation`
   cue state** before swapping components — still the last "loaded but
   not composed" seam in the transformation flow, unrelated to the
   World/Physics work and just as real a gap as it's been since Phase
   3.5.
7. **Box-box and circle-box collision** — `PHYSICS/Collider` supports
   `kBox` as data, but `CollisionSystem::Detect` only tests circle-circle
   pairs today. A box collider on any entity currently participates in
   zero collision detection — the most immediately actionable gap inside
   Phase 4.1 itself, ahead of moving on to Phase 4.2.
8. **Extend `PackageValidator`'s naming consistency to IK chains,
   retarget maps, and transformations** — currently only moves and
   animation clips get the ref-list-key-vs-internal-name check; the same
   latent-bug class (a rename that silently desyncs a `.dominus` ref from
   its target file's own identity) is possible in those three too.
9. **Add coherence criteria for more genres**, or decide not to — a real
   fork, not a build task by default. Every genre beyond `soulslike`/
   `arcade` currently produces zero `GameDesignCoherenceChecker` notes
   because the source document never specified concrete criteria for
   them. Adding more means either finding/agreeing on a real source for
   the criteria, or accepting the checker stays narrow by design.
10. **Wire `CORE::JobSystem` into the live tick pipeline** — a real,
    tested thread pool has existed since Phase 1 (`Application::
    Initialize` already constructs one), but `Application::Tick()` is
    still an empty stub and nothing has ever called `Submit()` for real
    work. Dispatching `PHYSICS`/`COMBAT`/`AI` updates through it would
    be this engine's first genuine parallel workload — real, scoped,
    and the infrastructure to do it already exists and is tested.
11. **Give `WORLD::HistoryEvent` structured, typed fields** (beyond the
    current free-form `event_type`/`description` strings) — the real
    prerequisite for `VisualMemoryDeriver`/`MaterialWearDeriver` to
    report anything richer than a count or a simple heuristic. A
    genuine schema decision (what fields? damage dealt? a kill flag?),
    not a small addition — worth its own scoping pass rather than
    bolting fields on speculatively.
12. **Bind `MaterialGenome`/`VisualStyleGenome` to entities via
    `RigBinder`** — same gap `VisualGenome` had until this phase closed
    it. Both currently load standalone via their own JSON file, same
    state `CombatStyleGenome`/`CombatPhysicsGenome`/`GameDesignGenome`
    are still in.

---

**TRACK H / HITM RIVALS INTEGRATION — Modules 0–4 + 5A complete:**
Opened by a full audit of DOMINUS against the real, existing HITM Rivals
codebase and its real authored fighter data — full findings in
`HITM_INTEGRATION_AUDIT.md`, full sequenced plan in `ROADMAP.md`'s
"TRACK H" section. Headline finding: every "Brooklyn" fixture this engine
has ever tested against is a strawman — real name, invented data. Module 0
fixed a real, previously-invisible bug in the shared JSON parser
(`\uXXXX` escapes were silently mis-decoded) that would have corrupted
real HITM text on ingest. Module 1 (`CHARACTER/HitmBridge/
HitmIdentityImporter`) is a real, tested path that reads the actual
`hitm-engine/data/identity/<fighter>/` files for all three real fighters
(Brooklyn, Rocket, Static) and losslessly validates them — live-run via
`dominus-cli import-hitm-identity <dir>`, not just unit-tested. Module 2
(`CHARACTER/HitmBridge/HitmCombatGenome`) is a new, explicit, typed
representation of a real fighter's full combat genome — every one of
Brooklyn's 13 authored components including his five-tier read-engine
mechanic, built on top of Module 1's output, provably lossless
(`ToJson()` returns the exact source tree; `dominus-cli
hitm-combat-genome <dir>` proves it live). `CombatIdentity` — the
original six-field strawman — is deliberately untouched, not replaced;
nothing in the engine reads `HitmCombatGenome` yet, so no gameplay
behavior has changed. Module 3 (`CHARACTER/HitmBridge/HitmPartsRig`) gives
HITM's real, generated `parts.json` (atlas name/size, per-part pivot +
normalized size + pixel frame, draw order, hand anchor, bone hierarchy) a
typed home — `ANIMATION/SkeletonSystem/Skeleton.h` had no part/pivot/
atlas-frame concept at all. Found and preserved a real, non-obvious fact
in the actual data: `handFar`/`handNear` each appear twice in every real
fighter's bone list, once rigid and once as a secondary-motion glove-bounce
overlay — a naive name-keyed map (the pattern `Skeleton::AddBone` itself
uses) would have silently discarded one. Deliberately does not bind into
a `Skeleton` (would require inventing bind-pose data the source doesn't
have) or touch rendering. Module 4 (`CHARACTER/HitmBridge/HitmGameRules`)
gives HITM's real, global `data/system/game.json` (gravity, walk/dash
speed, meter economy, damage scaling, hitstop frames, round rules, the
authoritative fighter roster) a typed home — nothing in `PHYSICS` or
`COMBAT` had fighting-game-specific constants anywhere; `GameDesignGenome`
is a meta-design descriptor, not this. Module 5A
(`CHARACTER/HitmBridge/HitmFighterRuntime`) is the pivot from "DOMINUS
can read HITM data" to "DOMINUS can simulate HITM gameplay": a real
Brooklyn, built entirely from Modules 1/2/4's real data, actually walks
(real `walkSpeed`), jumps (real `jumpVel`/`gravity`, integrated by the
real, unmodified `PHYSICS::PhysicsSystem`), executes his real "special"
move through its real startup/active/recovery frame counts, takes a hit
with real damage/hitstun/meter/hitstop numbers and a real reaction
decided by `COMBAT::ReactionSystem::Determine` from his real
`defense_profile.blockPreference`, and transitions his real five-tier
read-engine mechanic — deterministically, proven by two independent
runtimes producing byte-identical state from an identical input script.
Full accounting in `HITM_FIGHTER_RUNTIME_REPORT.md`, including a real
dangling-pointer bug this module found in itself (a `this`-captured
`WorldTick` closure that went stale on move) and a real finding that
HITM's move schemas vary by fighter/move-type (Rocket's and Static's real
specials are missing fields Brooklyn's has). The lifetime bug is now
fixed properly — every mutable field lives in one heap-allocated,
never-relocated block, so the registered closure stays valid across any
number of moves, `Result<T>` returns, or container storage — restoring
genuine `WorldTick` integration rather than avoiding it, verified safe
under construction/move/destruction and clean across a full-suite run
under AddressSanitizer + UndefinedBehaviorSanitizer. The same audit found
the identical `this`-capturing pattern dormant (unexploited, not yet
fixed) in `PHYSICS::PhysicsSystem::AsWorldSystem()`. A third continuation
added 5 more tests targeting the exact construct→move→move-again→
execute→destroy sequence and sibling-runtime survival across a
neighbor's destruction, then re-verified the full suite under two
independent clean rebuilds — a normal Release build and a separate
AddressSanitizer+UndefinedBehaviorSanitizer build — both green, zero
sanitizer findings across 4 ASan runs, and the live CLI demo's output
byte-for-byte identical between builds. A fourth continuation then closed
the one dormant hazard that was left: `PhysicsSystem::AsWorldSystem()`'s
identically-shaped `this`-capture, fixed (not just documented) by
capturing its one piece of state by value instead, with 3 new regression
tests and its own clean AddressSanitizer+UndefinedBehaviorSanitizer run
— **Module 5A is now formally closed with no known callback-lifetime
hazard left open** in anything it touched. Nothing is rendered, no sound
plays, and no second real player exists yet. See each module's own
"explicitly not done" note in `ROADMAP.md` for the honest boundary.
**765/765 tests passing (was 656 before this track).**
