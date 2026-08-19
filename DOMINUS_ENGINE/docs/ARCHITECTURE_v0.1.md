# DOMINUS ENGINE — Technical Architecture v0.1

Status: **Phase 1 (DOMINUS CORE) — scaffold stage.**
Do not write gameplay code against this doc until Phase 1's exit criteria
(bottom of this file) are met. See `ROADMAP.md` for the phase gate.

---

## 1. Concept Model

Dominus Engine is a Meta-Bin object substrate: a runtime and toolchain built
around one atomic unit, the `.dominus` object, which carries geometry,
skeleton, animation, combat data, AI behavior, physics rules, audio, and
lore as a single versioned entity instead of a folder of loosely-coupled
files. Systems (rendering, animation, combat, AI, world simulation) are
consumers of Meta-Bin objects, not owners of their own private data format.
The near-term proof case is HITM RIVALS: an existing 2D fighting game whose
character, combat, and animation data will be the first content migrated
into `.dominus` objects and run through the Core → Character → Combat phase
gates.

## 2. System Architecture

```
                         ┌───────────────────┐
                         │   TOOLS/Editor     │  authoring surface
                         │  AssetCompiler     │  raw asset → .dominus
                         │  Importers         │  fbx/psd/spine → intermediate
                         └─────────┬──────────┘
                                   │ writes
                                   ▼
                         ┌───────────────────┐
                         │  CORE/MetaBin      │  object graph, component
                         │  CORE/Serialization│  storage, .dominus (de)serialize
                         │  CORE/Memory       │  arena/pool allocators
                         │  CORE/Runtime      │  app loop, job system, ECS world
                         └─────────┬──────────┘
                     ┌─────────────┼─────────────────┬───────────────┐
                     ▼             ▼                 ▼               ▼
              ┌────────────┐ ┌───────────┐   ┌──────────────┐ ┌────────────┐
              │ ANIMATION  │ │ GRAPHICS  │   │   COMBAT     │ │    AI      │
              │ Skeleton   │ │ Renderer  │   │  HitSystem   │ │  Agents    │
              │ IK         │ │ Materials │   │  ComboSystem │ │  Director  │
              │ MoCap      │ │ Lighting  │   │  PhysicsCbt  │ │  Learning  │
              │ Procedural │ │           │   │  Reaction    │ │ (Phase 5)  │
              └─────┬──────┘ └─────┬─────┘   └──────┬───────┘ └─────┬──────┘
                    │              │                │               │
                    └──────────────┴────────┬───────┴───────────────┘
                                             ▼
                                     ┌───────────────┐
                                     │ CHARACTER      │  Genome, Rig, AI
                                     │ (composition   │  binds the above into
                                     │  layer)        │  one playable fighter
                                     └───────┬────────┘
                                             ▼
                                     ┌───────────────┐
                                     │    WORLD       │  Terrain, NPC, Sim
                                     │  (Phase 4)     │  consumes CHARACTER
                                     └────────────────┘
```

**Data flow, one sentence per stage:** raw art/mocap/design data enters
through `TOOLS/Importers`, gets compiled into a `.dominus` object by
`TOOLS/AssetCompiler`, is loaded at runtime by `CORE/Serialization` into the
`CORE/MetaBin` object graph, and is then read (never mutated in place, only
through the ECS world) by `ANIMATION`, `GRAPHICS`, `COMBAT`, and `AI` systems
each frame via `CORE/Runtime`'s job system. `CHARACTER` is the composition
layer that proves a given `.dominus` object is a complete, playable fighter —
this is the same contract `hitm-character-forge` already validates at the
design-doc level; Dominus Engine's `CHARACTER` module is where that contract
becomes runtime-loadable rather than documentation.

## 3. Module Boundaries

| Module | Owns | Reads from | Never does |
|---|---|---|---|
| `CORE/MetaBin` | Object graph, component storage | `Serialization` | Know about rendering, combat, or any leaf system |
| `CORE/Serialization` | `.dominus` read/write, versioning/migration | `MetaBin`, `schemas/` | Contain gameplay logic |
| `CORE/Memory` | Arena/pool allocators, frame allocators | nothing (leaf) | Allocate via raw `new`/`malloc` anywhere else in the engine |
| `CORE/Runtime` | App loop, job scheduler, ECS world, system registration | `MetaBin`, `Memory` | Contain any domain-specific system logic itself |
| `GRAPHICS/*` | Rendering, materials, lighting | `MetaBin` (mesh/material components), `Runtime` (job system) | Own gameplay or combat state |
| `ANIMATION/*` | Skeleton, IK, mocap import, procedural motion | `MetaBin` (skeleton/clip components) | Decide combat outcomes — it plays clips the combat/character layer requests |
| `CHARACTER/*` | Genome, rig binding, per-character AI config | `ANIMATION`, `COMBAT` schemas, `MetaBin` | Reimplement what `combat-genome-director` / `hitm-character-forge` already produce at design time — it binds their output, it doesn't redesign it |
| `COMBAT/*` | Hit detection, combos, physics reactions | `CHARACTER` (frame data), `MetaBin` | Hardcode any character-specific values |
| `WORLD/*` | Terrain, NPC population, simulation | `CHARACTER`, `AI` | Start before Phase 4 gate is open |
| `AI/*` | Combat AI, director-level pacing, (later) learned agents | `COMBAT` (state), `CHARACTER` (behavior config) | Ship neural/learned systems before Phase 5 |
| `TOOLS/*` | Editor, compiler, importers | filesystem, DCC exports | Ship as part of the runtime binary |

## 4. API Surface (v0.1 — Core only)

Phase 1 exposes exactly four subsystems. Everything else is a stub until its
phase gate opens.

```cpp
// CORE/Runtime/Application.h
class Application {
public:
    bool Initialize(const AppConfig& config);
    void Run();              // blocks, drives the frame loop
    void Shutdown();

    JobSystem&  GetJobSystem();
    EcsWorld&   GetWorld();
};

// CORE/MetaBin/MetaBinObject.h
class MetaBinObject {
public:
    ObjectId       Id() const;
    ObjectVersion  Version() const;

    template <typename Component>
    Component*     GetComponent();
    template <typename Component>
    Component&     AddComponent(Component data);
    bool           HasComponent(ComponentTypeId type) const;
};

// CORE/Serialization/DominusSerializer.h
class DominusSerializer {
public:
    static Result<MetaBinObject> Load(const std::filesystem::path& dominusFile);
    static Result<void>          Save(const MetaBinObject& obj,
                                       const std::filesystem::path& outFile);
    static bool                  Validate(const MetaBinObject& obj,
                                           std::vector<std::string>* errorsOut);
};

// CORE/Memory/Allocator.h
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t byteCapacity);
    void*  Allocate(size_t bytes, size_t alignment);
    void   Reset();      // frame-scoped reuse, no per-allocation free
};
```

Everything downstream (`GRAPHICS`, `ANIMATION`, `COMBAT`, `CHARACTER`,
`WORLD`, `AI`) will register against `EcsWorld` and consume `MetaBinObject`
components — no direct module-to-module calls outside that contract.

## 5. Data Schema — the `.dominus` Object

Canonical schema: `schemas/dominus_object.schema.json`.

Top-level shape (JSON on disk for v0.1 for inspectability; binary-packed
form is a Phase 2+ optimization, not a v0.1 concern per Law 4 — scalability
before spectacle, but readability first while the format is still moving):

```json
{
  "dominus_version": "0.1.0",
  "object_id": "brooklyn",
  "identity": { "display_name": "Brooklyn", "faction": "Renegades" },
  "mesh": { "ref": "mesh/brooklyn.mesh" },
  "textures": [ { "slot": "albedo", "ref": "tex/brooklyn_albedo.png" } ],
  "skeleton": { "ref": "skeleton/brooklyn.skel" },
  "animations": [ { "name": "idle", "ref": "anim/brooklyn_idle.clip" }, { "name": "attack_jab", "ref": "anim/brooklyn_attack.clip" } ],
  "combat_dna": { "ref": "combat/brooklyn_genome.json" },
  "ai_behavior": { "ref": "ai/brooklyn_behavior.bt" },
  "physics_rules": { "mass_kg": 68, "hurtbox_ref": "physics/brooklyn_hurtboxes.json" },
  "audio": [ { "event": "hit_light", "ref": "audio/brooklyn_hit_light.wav" } ],
  "lore": { "ref": "lore/brooklyn.md" },
  "evolution_history": [
    { "version": "0.1.0", "date": "2026-07-31", "note": "initial import from hitm-engine roster" }
  ]
}
```

Every `ref` field is a relative path resolved against the object's own
directory at compile time by `TOOLS/AssetCompiler`, and packed into the
final `.dominus` binary/archive — nothing is a loose external dependency at
runtime.

### Extended fields (Phase 2.5 — DOMINUS MOTION INTELLIGENCE SYSTEM)

Three additional top-level ref fields, all following the same "generic
path ref in CORE, resolved by a leaf module" pattern as `skeleton`/
`animations`:

```json
{
  "motion_graph": { "ref": "brooklyn_motion_graph.json" },
  "ik_chains": [ { "name": "arm_reach", "ref": "brooklyn_arm_ik.json" } ],
  "retarget_map": { "ref": "generic_to_brooklyn_retarget.json" }
}
```

- `motion_graph` — one ref, parsed into `CORE::MotionGraphRefComponent`,
  resolved by `RigBinder` into a `CHARACTER::MotionGraphComponent`
  (`ANIMATION::MotionGraph` data) via `ANIMATION::MotionGraphLoader`.
- `ik_chains` — a named ref list (same shape as `animations`), parsed into
  `CORE::IKChainRefListComponent`, resolved into
  `CHARACTER::IKChainSetComponent` via `ANIMATION::IKChainLoader`.
- `retarget_map` — one ref, parsed into `CORE::RetargetMapRefComponent`,
  resolved into `CHARACTER::RetargetMapComponent` via
  `ANIMATION::RetargetMapLoader`. Only present on objects that need to play
  someone else's motion — the source skeleton (e.g. Brooklyn) doesn't need
  one.

### Phase 3 fields (DOMINUS COMBAT SYSTEM)

Three more fields, same pattern, resolved by `COMBAT::CombatBinder` rather
than `CHARACTER::RigBinder` (COMBAT reads CHARACTER, never the reverse):

```json
{
  "combat_dna": { "ref": "brooklyn_combat.json" },
  "moves": [ { "name": "jab", "ref": "brooklyn_move_jab.json" } ],
  "physics_rules": { "hurtbox_ref": "brooklyn_hurtboxes.json" }
}
```

- `combat_dna` — one ref, parsed into `CORE::CombatDnaRefComponent`,
  resolved into `COMBAT::CombatIdentityComponent` (wrapping
  `CHARACTER::CombatIdentity`) via `CHARACTER::CombatIdentityLoader`.
- `moves` — a named ref list (same shape as `animations`/`ik_chains`),
  parsed into `CORE::MoveRefListComponent`, resolved into
  `COMBAT::MoveSetComponent` via `COMBAT::MoveLoader`.
- `physics_rules.hurtbox_ref` — parsed into `CORE::HurtboxRefComponent`,
  resolved into `COMBAT::HurtboxSetComponent` via `COMBAT::HurtboxLoader`.

### Phase 3.5 field (COMBAT SOUL + CINEMATIC BATTLE LAYER)

```json
{
  "transformations": [ { "name": "beast_mode", "ref": "brooklyn_beast_mode.transform.json" } ]
}
```

- `transformations` — a named ref list, parsed into
  `CORE::TransformationRefListComponent`. Unlike other refs, the file it
  points at is not resolved by a `*Binder` at load time — a transformation
  file bundles its OWN `combat_dna_ref`/`motion_graph_ref`/`moves` (see
  `COMBAT::TransformationDef`), each resolved on demand by
  `COMBAT::TransformationSystem::Apply` when the transformation actually
  triggers, replacing `CombatIdentityComponent`/`MotionGraphComponent`/
  `MoveSetComponent` in place.

### Phase 3.75 (DOMINUS COMBAT INTEGRATION LAYER)

No new `.dominus` schema fields -- this phase is entirely about wiring
already-loaded systems together rather than adding new refs. Two schema-
adjacent additions worth noting: `TransformationDef`'s inline JSON (not a
new top-level `.dominus` field, but part of the transformation file
format already documented above) now accepts five more optional blocks --
`ai_profile`, `physics_profile`, `audio_profile`, `visual_profile`,
`camera_profile` -- each parsed into `COMBAT::Profiles.h`'s corresponding
struct and attached as its own component by `TransformationSystem::Apply`
when present (an omitted block leaves the object's existing value
untouched). See `ROADMAP.md` Phase 3.75 for the full module list
(`AI/Agents/CombatAI` genome wiring, `COMBAT::ComboSystem::StyleCollector`,
`COMBAT::WorldEventSystem`).

### Phase 3.9 (MOTION LIBRARY COMPLETION)

No new `.dominus` schema fields either -- this phase is entirely fixture
content (the motion graph JSON, ten new clip files, two new move files)
plus one new pure-logic validator, `COMBAT::AssetValidation`, which reads
an already-bound `MoveSetComponent` and `MotionGraph` and reports any
move whose `motion_trigger` has no matching transition. See `ROADMAP.md`
Phase 3.9 for the full state/transition inventory (13 states, 47
transitions) and what's still genuinely unresolved.

### Phase 4.0 (UNIVERSAL WORLD KERNEL) — architectural note, not a schema change

Adds no `.dominus` schema fields -- `WORLD/Core` operates on
already-loaded `CORE::MetaBinObject` instances, not on the file format.
The relevant architectural fact belongs here rather than in the schema
list above: **`WORLD` depends on `CORE` only.**
`WORLD/Core/EntityRegistry.h`, `WorldTick.h`, `World.h`, and
`SpatialComponent.h` contain zero includes of `ANIMATION`, `CHARACTER`,
or `COMBAT` -- verified by grep before any Phase 4.0 test was written,
not asserted after the fact. This inverts the informal dependency
direction Phases 3-3.9 had drifted toward (where "the engine" implicitly
meant "the combat engine"): `COMBAT` is now correctly positioned as a
domain extension that *reads* `WORLD`'s entities and registers itself as
a `WorldSystemFn`, never the reverse.

`EntityRegistry` does not introduce a new entity concept --
`MetaBinObject` (section 4) already satisfies WORLD LAW 002 ("Entity +
Components") at the single-object level; `EntityRegistry` is the
multi-entity container that was actually missing, plus a
`WithComponent<T>()` type-filter query. See `ROADMAP.md` Phase 4.0 for
the full module breakdown, the milestone proof
(`tests/world/test_hitm_rivals_as_world_entity.cpp`), and what's
explicitly deferred (Modules 2-6: Physics, Terrain, Streaming, NPC
expansion, Procedural Generation).

### Phase 4.1 (UNIVERSAL PHYSICS LAYER) — architectural note, not a schema change

Also adds no `.dominus` schema fields. The rule extends one level down:
**`PHYSICS` depends on `WORLD` (for `SpatialComponent`, where position
lives) and `CORE` only** -- never `CHARACTER`/`COMBAT`/`ANIMATION`.
Verified the same way as Phase 4.0: grepping actual `#include` lines in
`PHYSICS/*.h` (a first pass grepping for the bare strings "COMBAT"/
"CHARACTER" caught false positives from prose in comments explaining the
rule -- worth noting because it's exactly the kind of self-check mistake
worth catching before it becomes a false sense of verification).

`PHYSICS/RigidBody.h` and `Collider.h` are pure data -- no dependency on
anything. `PhysicsSystem.h` and `CollisionSystem.h` both expose
`AsWorldSystem()`, returning a `world::WorldSystemFn` -- the same
registration seam `COMBAT` uses in the Phase 4.0 proof, meaning `PHYSICS`
is provably a plugin `WORLD` ticks, not a feature `WORLD` implements.
See `ROADMAP.md` Phase 4.1 for the full milestone writeup (three
identity-unrelated entities colliding through generic components alone)
and what's explicitly deferred (box collision, a real impulse solver,
spatial indexing, additional constraint types).

### Phase 7 (PACKAGE VALIDATOR) — architectural note, not a schema change

Adds no new `.dominus` schema fields -- `VALIDATION/PackageValidator`
inspects an already-loaded `MetaBinObject` and its bound components. It
is the terminal node of the dependency graph: depends on `CORE`,
`ANIMATION`, `CHARACTER`, and `COMBAT` (needs `RigBinder`/`CombatBinder`
to produce the bound state it inspects); nothing depends on it. Numbered
"Phase 7" to match its source document rather than "Phase 4.1.5" or
similar, because it is explicitly NOT part of the Phase 4.x World Engine
sequence -- it's cross-cutting quality-gate infrastructure applicable to
every phase's output. See `ROADMAP.md` Phase 7 for the full check
inventory and the real bug it found in Brooklyn's own fixture on first
use.

### Registry prototype (hash + immutable artifacts, CombatGenome only)

A scoped Phase 1 prototype of a much larger proposed architecture
(deterministic compilation, content-addressed immutable artifacts, a
registry, version lineage, runtime snapshots isolated from authored
data) -- deliberately limited to ONE genome type (`CombatGenome`) to
prove the pipeline before committing the whole engine to it, per an
explicit "validate the architecture before generalizing it" directive.
`REGISTRY/` depends on `CHARACTER/Genome` only -- never `COMBAT`/
`ANIMATION`/`WORLD`/`PHYSICS`, verified by grep before any test was
written, same discipline as every other module boundary in this engine.

The pipeline is real, not sketched: `CanonicalSerializer` produces
deterministic bytes from a `CombatIdentity` (fixed field order, no
whitespace dependency on source formatting); `Sha256.h` is a hand-rolled
SHA-256 implementation verified against digests independently computed
via Python's `hashlib` (both standard published test vectors AND one
matching this module's own canonical format, including a multi-64-byte-
block input to exercise the chunking path -- a hash function that's
silently wrong is worse than none, so correctness was checked, not
assumed); `GenomeCompiler` runs Validator -> CanonicalSerializer -> Hash
-> `ImmutableArtifact` (every member private with only const getters --
no mutation API exists on the type at all, not just by convention);
`GenomeRegistry` is content-addressed (same content always hashes to the
same key, so "overwriting" is structurally impossible) with per-entity
ordered lineage tracking; `RuntimeSnapshot`/`SnapshotBuilder` prove
"runtime never touches authored data" structurally -- `Build()` accepts
only an `ImmutableArtifact`, never a `CombatIdentity`, so there is no
code path that builds a snapshot without going through the compiled,
hashed artifact first.

This is explicitly NOT a claim that the rest of the engine now works
this way -- every other genome type (Motion/Physics/Behavior/Audio/
etc., named in the source proposal) still loads directly via
`DominusSerializer` with no hashing, no immutability, no registry. See
`ROADMAP.md`'s Registry Prototype section for the full proof (5 claims,
each independently tested against real Brooklyn data) and the explicit
decision NOT to generalize this to the whole engine without further
justification.

### Society Phase 0 (WORLD PERSISTENCE LAYER)

The rule extends one more level: `WorldPersistence` (in `WORLD/Core`)
still depends on `CORE` only. It does not gain a COMBAT/CHARACTER
dependency just because it's saving entities that HAVE combat data --
it only ever reads/writes `SourceRefComponent` (a path string) and
`SpatialComponent` (a WORLD-owned type), never the bound
`CombatIdentityComponent`/`MotionGraphComponent`/etc. that live one
layer up. Reconstructing a fully bound entity on load is explicitly
external orchestration code's job (a CLI command, an integration test)
-- re-running the ALREADY-EXISTING `DominusSerializer::Load` +
`RigBinder::Bind` + `CombatBinder::Bind` pipeline against the saved
source ref, exactly the same division of labor as the Phase 4.0 and
4.1 milestone proofs (`WORLD`/`PHYSICS` stay pure, external code
composes them with `COMBAT`/`CHARACTER`). See `ROADMAP.md`'s Society
Phase 0 section for the full write-up, including the complete
save-destroy-reload-refight proof against a real Brooklyn.

### Society Phase 1 (UNIVERSAL ENTITY MODEL)

Adds three new optional `.dominus` fields (documented in
`schemas/dominus_object.schema.json`): `entity_type` (a free-form tag,
parsed straight into `CORE::EntityTypeComponent`), `provenance` (parsed
into `CORE::ProvenanceComponent`), and `social_genome` (a ref, parsed
into `CORE::SocialGenomeRefComponent` and resolved by
`CHARACTER::RigBinder` into `SocialGenomeComponent`, wrapping
`CHARACTER::SocialGenome` -- the same load/bind shape `CombatIdentity`
already established). All three are pure metadata or path-refs at the
`CORE` level, same discipline as every other ref component: `CORE`
carries a tag or a path, it does not know what "rival" or "organism"
means.

`WORLD::WorldHistory` also gained `consequences` (free-form strings
attached to an event) and `EventsForEntity()` (a filter query) --
neither required a schema change, since history lives in
`WORLD_STATE/history/timeline.json`, not in a `.dominus` file.

See `ROADMAP.md`'s Society Phase 1 section for the full write-up,
including the deliberate cross-system connection (Brooklyn's
`provenance.creation_hash` matches the real SHA-256 the Registry
Prototype's `GenomeCompiler` produces for his combat genome) and what
was explicitly scoped out of the source 8-point proposal (Economy-as-
entity, the presentation-layer/graphics abstraction, Q-WEAVE-as-
duplicated-engine-code).

### MONSTERFORGE Phase 1 (CREATUREGENOME SCHEMA)

Adds one new optional `.dominus` field, `creature_genome` (documented in
`schemas/dominus_object.schema.json`), parsed into
`CORE::CreatureGenomeRefComponent` and resolved by `CHARACTER::RigBinder`
into `CreatureGenomeComponent` -- the exact load/bind shape
`SocialGenome` already established, including inheriting its flagged
skeleton-requirement limitation. `CHARACTER::CreatureGenome` itself
depends on nothing beyond the standard library.

The architecturally interesting part sits in `REGISTRY`:
`CanonicalSerializer::SerializeCreatureGenome` and
`CreatureGenomeCompiler` extend the Registry Prototype's determinism/
hashing discipline to a second genome type with roughly 4x
`CombatIdentity`'s field count -- real evidence (not assertion) that
the canonical-serialization/hashing layer generalizes. It deliberately
stops there: `ImmutableArtifact` still bakes in `CombatGenome`-specific
`DecisionWeights`, and no `CreatureGenome` decoder exists to produce an
equivalent, so `CreatureGenomeCompiler` returns hash + canonical bytes
only rather than misusing `ImmutableArtifact` to hold data it doesn't
have. Whether to structurally generalize `ImmutableArtifact`/
`GenomeRegistry`/`RuntimeSnapshot` themselves (e.g. via a template
parameter) remains the same open decision flagged since the original
Registry Prototype phase -- this phase added evidence toward that
decision without making it.

See `ROADMAP.md`'s MONSTERFORGE Phase 1 section for the full write-up,
including what was explicitly NOT built from the source proposal
(evolution simulation, ecosystem generation, a "Unknown Entity
Generator" producing invented threat-level percentages) and why.

## 6. Testing Strategy

- **Unit tests** (`tests/core/`): `MetaBinObject` component add/get/remove,
  `ArenaAllocator` allocation/reset correctness, `DominusSerializer`
  round-trip (save → load → equality) and schema validation rejection cases.
- **Integration tests** (`tests/integration/`, added Phase 2+): load a real
  `.dominus` object built from HITM RIVALS roster data, verify every
  referenced sub-asset resolves.
- **Golden-file tests**: `schemas/dominus_object.schema.json` validated
  against fixture files in `tests/fixtures/` on every CI run — a schema
  change that breaks an existing fixture fails the build, forcing an
  explicit version bump.
- **No gameplay/combat tests until Phase 3** — there is no combat system to
  test yet; writing them earlier would test against a moving target.

Test runner: CTest (bundled with CMake), see `CMakeLists.txt` /
`tests/CMakeLists.txt`.

## Phase 1 Exit Criteria (must pass before Phase 2 starts)

- [ ] `Application::Initialize/Run/Shutdown` compiles and runs an empty frame
      loop with the job system spinning up and down cleanly.
- [ ] `MetaBinObject` supports add/get/has for at least 3 distinct component
      types with no dynamic_cast — compile-time type IDs only.
- [ ] `DominusSerializer::Load/Save` round-trips a fixture `.dominus` file
      byte-for-byte equivalent (modulo key ordering) and `Validate` correctly
      rejects a fixture with a missing required field.
- [ ] `ArenaAllocator` passes an allocation/reset/reuse unit test with no
      leaks under ASan.
- [ ] All of the above have a green test in `tests/` — no exit criteria is
      considered met without a corresponding automated test.

Phase 2 (`DOMINUS CHARACTER SYSTEM`) does not start until every box above is
checked. See `ROADMAP.md`.
