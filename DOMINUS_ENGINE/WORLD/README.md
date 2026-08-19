# WORLD

**Status: Phase 4.0 -- `Core/` built (Module 1), now including Society
Phase 0's persistence layer.**

- `Core/` -- the Universal World Kernel: `EntityRegistry` (WORLD LAW
  002 -- the multi-entity container; `CORE::MetaBinObject` already
  satisfies "Entity + Components" per-object, this holds many together),
  `WorldTick` + `World` (WORLD LAW 003 -- headless simulation, systems
  run with zero rendering calls), `SpatialComponent` (WORLD LAW 001 --
  one component type serves 2D/2.5D/3D alike).
- `Core/SourceRefComponent.h`, `WorldHistory.h`, `WorldPersistence.h/.cpp`
  -- Society Phase 0: real `Save()`/`Load()` against disk. "Entity
  exists -> History recorded -> World saved -> Creator leaves -> Creator
  returns -> World continues." Saves what WORLD actually owns (entity
  ids, source refs, positions, elapsed time, history) -- never bound
  runtime state from `CHARACTER`/`COMBAT`/`ANIMATION`, which WORLD
  doesn't know exists. Reconstructing a fully bound entity on load is
  external orchestration code's job (a CLI command, an integration
  test), re-running the existing `DominusSerializer`/`RigBinder`/
  `CombatBinder` pipeline -- never something inside `WORLD/Core` itself.
  As of Society Phase 1, `WorldHistory` events also carry `consequences`
  (free-form strings) and support `EventsForEntity()` filtering.
  `CHARACTER::VisualMemoryDeriver` (DORRE) is the first real external
  consumer of `EventsForEntity()` -- proof the query shape is actually
  useful, not just plausible.

`WORLD/Core` depends on `CORE` only -- zero includes of
`ANIMATION`/`CHARACTER`/`COMBAT`, verified by grep before any test was
written, including after the persistence layer was added. `COMBAT` runs
ON TOP of `WORLD` as an externally-registered `WorldSystemFn`, not the
reverse -- proven by `tests/world/test_hitm_rivals_as_world_entity.cpp`,
and the persistence layer's own full-cycle proof
(`tests/integration/test_world_persistence_full_cycle.cpp`) shows the
same discipline holding even across a save/destroy/reload boundary: a
real Brooklyn is destroyed entirely and rebuilt from disk into a fully
combat-capable entity again, with `WORLD/Core` itself never touching a
single `COMBAT`/`CHARACTER` type.

`Terrain/`, `NPC/`, and `Simulation/` remain exactly the placeholders
they were after Phase 1 -- Modules 3-6 (Terrain, Streaming, NPC
Simulation expansion, Procedural World Generation) are real, scoped
future work, not started. Module 2 (Universal Physics) is built -- see
`../PHYSICS/README.md` -- as a sibling module, not a subfolder of
`WORLD/`: `PHYSICS` depends on `WORLD/Core` (for `SpatialComponent`),
never the reverse. See `../ROADMAP.md` Phase 4.1 and Society Phase 0 for
the full milestone writeups and what's explicitly deferred (no
`economy/` output -- Economy doesn't exist yet; no bound-runtime-state
persistence; no incremental saves).
