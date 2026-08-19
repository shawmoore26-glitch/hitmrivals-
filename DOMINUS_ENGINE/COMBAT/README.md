# COMBAT

**Status: Phase 3 + Phase 3.5 + Phase 3.75 + Phase 3.9 -- built.** As of
Phase 4.0, COMBAT is architecturally a domain extension that runs ON TOP
of `WORLD` (an externally-registered `WorldSystemFn`, not a built-in
world feature) -- see `../WORLD/README.md` and
`../tests/world/test_hitm_rivals_as_world_entity.cpp` for the proof.
Nothing in this directory changed to make that true; the correction was
architectural (which module depends on which), not a rewrite. See
`../ROADMAP.md` Phase 3.9 for COMBAT's own full status including what's
genuinely unresolved.

- `HitSystem/` -- `MoveDef` (frame data + intent + hitboxes), `Hurtbox`,
  loaders for both, `CombatComponents` (what CombatBinder attaches),
  `CombatBinder` (resolves `combat_dna`/`moves`/`physics_rules.hurtbox_ref`
  refs through Meta-Bin), `CollisionEvaluator` (bone-relative circle
  overlap against a live Pose), `AssetValidation` (checks every move's
  `motion_trigger` resolves to a real motion-graph transition before
  runtime -- `dominus-cli validate-motion`)
- `ComboSystem/` -- `ComboEngine` (cancel-window + followup legality),
  `StyleRankSystem` (D-SSS live style grade), and `StyleCollector`
  (auto-populates `StyleMetrics` from real combat events instead of
  hand-typed literals -- see `CombatController::ApplyHit`/
  `RecordMoveLanded`)
- `PhysicsCombat/ClashSystem.h` -- attack-vs-attack force/speed/skill
  resolution; `SkillFromWeights` reads genome via `DecisionWeights`
  (LAW C003)
- `PhysicsCombat/ImpactSolver.h` -- attack-vs-**body** (the gap
  `ClashSystem` doesn't cover): real `F=m*v` force, threshold-based
  impact severity, proportional armor reduction, per-body-part damage
  multipliers. Standalone -- not wired into `ReactionSystem`/
  `CombatController`'s live hit resolution yet. See `../ROADMAP.md`
  MASTER OF COMBAT Phase 2.
- `ReactionSystem/ReactionSystem.h` -- hit power (+ genome `defense_bias`)
  -> reaction type -> motion-graph trigger name
- `CombatController.h` -- the combat state machine. Drives the *same*
  `MotionGraphEvaluator` from `ANIMATION`/`CHARACTER` via `Trigger()` --
  LAW C012, never a separate animation system. Optional
  `CinematicDirector` and `StyleCollector` hooks.
- `MoveSelector.h` -- genome-weighted move scoring (LAW C003's "Move
  Selection" stage), now actually consumed by `AI/Agents/CombatAI`
- `CinematicDirector.h` -- Combat Event System: Gameplay -> Special Event
  -> Camera -> Physics -> Return Gameplay
- `TransformationSystem.h/.cpp` -- LAW C010's full genome swap: Combat
  Identity + Motion Graph + Abilities + all five `Profiles.h` layers
  (AI/Physics/Audio/Visual/Camera), each optional, replaced atomically
  from a `.dominus`-loaded `TransformationDef`
- `Profiles.h` -- the five transformation-swappable gameplay layers. Only
  `AIProfile` has a real consumer (`CombatAI::ApplyProfile`); the rest are
  genuinely loaded/attached but inert (no physics/audio/render system
  exists in this engine)
- `Environment.h` -- wall/floor bounds + destruction zones;
  `ApplyEnvironment` upgrades a reaction to wall/ground impact when the
  predicted trajectory crosses a boundary
- `WorldEventSystem.h` -- the Wall Impact -> World Event bridge into a
  future World Engine: converts an `Environment`-upgraded reaction or a
  destruction-zone hit into a `WorldEvent` (destruction/particle/audio/
  camera tags). No consumer yet -- this is the seam, not the
  implementation on the other side of it.
- `AnimeSpeedSystem.h` -- velocity integrator + bounded afterimage trail

Proven end to end via `dominus-cli fight` (LAW C001 pipeline),
`dominus-cli transform` (LAW C010 genome swap, now with all five
profiles), `dominus-cli ai` (genome-driven decision pipeline, now fully
closed), and `dominus-cli validate-motion` (asset coverage check).

Brooklyn (`tests/fixtures/brooklyn.dominus`) is the first Combat Genome
test case (LAW C015) -- style `psycho_drunken_martial_arts`, six real
moves (`jab`, `dodge`, `counter`, `combo_starter`, `launcher`,
`air_combo`), one real transformation (`beast_mode`). As of Phase 3.9,
every one of those moves resolves to a real motion-graph state --
verified by `AssetValidation`, not asserted by hand. The combo chain
`jab → combo_starter → launcher → air_combo` and `knockdown →
knockdown_recovery → idle` both run genuinely end to end.
