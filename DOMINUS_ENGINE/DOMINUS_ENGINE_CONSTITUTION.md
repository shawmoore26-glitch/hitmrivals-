# DOMINUS ENGINE — Constitution
Version 0.1 · HITM CITY / Meta-Bin Systems

## Mission

Create the first Meta-Bin reality creation engine — a single substrate where
characters, worlds, combat, and intelligence are not separate content types
bolted together, but one living data organism that can be authored, versioned,
and evolved as a whole.

## Core Principle

**Every asset is a living data organism.**

A character is not a model. A character is:

- geometry
- skeleton
- animation
- combat
- intelligence
- evolution history

None of these live in isolation. A `.dominus` object is the atomic unit that
carries all of them together, versioned as one thing, never as six
disconnected files that happen to reference each other by convention.

## Laws

1. **No disconnected systems.** Every feature must integrate into the
   Meta-Bin architecture. If a system can't state which Meta-Bin object types
   it reads and writes, it isn't ready to be built.
2. **Data-driven over hardcoded.** Frame data, combat values, AI parameters,
   and material properties are data, not literals buried in code. Systems
   read data; they do not encode it.
3. **Modularity over monolith.** Every module in the tree
   (`CORE/GRAPHICS/ANIMATION/CHARACTER/COMBAT/WORLD/AI/TOOLS`) must be
   independently buildable and testable against a stub of its dependencies.
4. **Scalability before spectacle.** A feature that works for one character
   and breaks at roster scale is not done.
5. **Document or it doesn't exist.** Every system ships with purpose,
   architecture, dependencies, and future expansion notes. Undocumented code
   is considered unmerged.
6. **Phases are sequential, not parallel.** Do not start WORLD before COMBAT
   is proven. Do not start AI CREATOR before CHARACTER SYSTEM is proven. See
   Roadmap.

## Forbidden Shortcuts

- No open-world work before Core + Character + Combat are proven (Phase 1–3
  gate, see `ROADMAP.md`).
- No hardcoded frame data, hitboxes, or combat constants — everything is a
  serialized `.dominus` payload or a data table the runtime loads.
- No asset that skips the Meta-Bin object system "just this once" (concept
  art placeholders and prototyping spikes are exempt but must be labeled
  `EXPERIMENTAL/` and never merged into `CORE`).
- No animation, rig, or combat system built disconnected from the object
  it serves — see `unreal-combat-architect`, `hitm-animation-director`,
  `image-to-rig` for the domain skills that already own those layers; Dominus
  Engine's job is the substrate they plug into, not a reinvention of them.
- No AI/behavior system that can't be inspected and replayed deterministically
  in training mode.

## Architecture (top level)

```
DOMINUS_ENGINE/
├── CORE/            MetaBin, Runtime, Memory, Serialization
├── GRAPHICS/         Renderer, Materials, Lighting
├── ANIMATION/        SkeletonSystem, IK, MotionCapture, ProceduralMotion
├── CHARACTER/        Genome, Rig, AI
├── COMBAT/           HitSystem, ComboSystem, PhysicsCombat, ReactionSystem
├── WORLD/            Terrain, NPC, Simulation
├── AI/               Agents, Learning, Director
├── TOOLS/            Editor, AssetCompiler, Importers
├── schemas/          .dominus object schema, data table schemas
├── tests/            unit + integration tests per module
└── docs/             per-system architecture notes
```

Full technical breakdown: `docs/ARCHITECTURE_v0.1.md`.
Build order and milestones: `ROADMAP.md`.
Object format spec: `schemas/dominus_object.schema.json`.

## The Dominus Object Format

Everything the engine manages becomes a `.dominus` file — one serialized
object graph, not a folder of loosely associated assets.

Example: `Brooklyn.dominus` contains, as one versioned object:

```
Identity
Mesh
Textures
Skeleton
Animations
Combat DNA
AI Behavior
Physics Rules
Audio
Lore
Evolution Data
```

Full schema: `schemas/dominus_object.schema.json`.

## Recommended Technology Foundation

**Core Engine**
- C++20
- Vulkan (primary) / DirectX 12 (secondary target, post-v0.1)
- ECS architecture (data-oriented, not inheritance-based)
- Multithreaded job system for simulation/render/animation parallelism

**Tools layer**
- Python for asset pipeline automation and the `.dominus` compiler front-end
- C++ for runtime-critical systems
- Lua (primary) or C# (if editor tooling needs it) for gameplay scripting

**Animation**
- Skeleton hierarchy + animation graphs
- IK solver (two-bone analytic, FABRIK for chains)
- Motion matching (post-v0.2, once baseline clip playback is proven)

**AI**
- Behavior trees for v0.1–v0.3 combat/NPC AI
- Neural/learned systems deferred to Phase 5 (`AI Creator`) — not attempted
  before the deterministic systems are solid, per Law 6.

## Reference Bar

The engine's design targets — not to be cloned, but to define what "AAA
combat feel" and "living world" mean when Dominus Engine is evaluated:
Unreal (tooling maturity), Unity (iteration speed), Godot (open modularity),
Blender/Maya/MotionBuilder (animation authoring), Spine (2D skeletal
production, already integrated via `spine-fighting-game`), fighting-game
combat feel (AKI-style weight, DMC/Sparking Zero movement expressiveness),
God of War physics-driven hit reaction, Elden Ring-scale world simulation.
These are north stars for feel, not dependencies to wrap.

## Governing Skill

Claude, acting on this engine, operates as **DOMINUS ENGINE ARCHITECT**:

- Never create disconnected systems.
- Every feature must integrate into Meta-Bin architecture.
- Priorities, in order: **Scalability → Modularity → Performance →
  Data-driven design.**
- Every deliverable documents: purpose, architecture, dependencies, future
  expansion.
- Existing domain skills (`combat-genome-director`, `hitm-character-forge`,
  `hitm-animation-director`, `image-to-rig`, `spine-fighting-game`,
  `unreal-combat-architect`, `hitm-engine`) are not replaced by Dominus
  Engine — they are the organs; Dominus Engine's `CHARACTER/COMBAT/ANIMATION`
  modules are the substrate contract those skills' outputs must satisfy.
