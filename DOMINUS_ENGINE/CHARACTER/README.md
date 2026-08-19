# CHARACTER

**Status: Phase 2 -- `Rig/` built. Phase 3 -- `Genome/CombatIdentity`
built. Society Phase 1 -- `Genome/SocialGenome` built. MONSTERFORGE
Phase 1 -- `Genome/CreatureGenome` built. MASTER OF COMBAT Phase 1 --
`Genome/CombatStyleGenome` built. MASTER OF COMBAT Phase 2 --
`Genome/CombatPhysicsGenome` built. DOMINUS ERA I --
`Genome/GameDesignGenome` built. DORRE -- `Genome/VisualGenome` +
`Genome/VisualMemorySummary` built. Reality Description Foundation --
`Genome/MaterialGenome` + `Genome/VisualStyleGenome` built,
`VisualGenome` bound to entities via `RigBinder`.**

- `Rig/RigBinder.h/.cpp` -- the composition layer: resolves CORE's generic
  `SkeletonRefComponent` / `AnimationRefListComponent` path refs into real
  `SkeletonComponent` / `AnimationSetComponent` data, sourced from
  `ANIMATION`'s loaders. This is what proves a `.dominus` object is an
  actually-loadable, actually-playable fighter. As of Society Phase 1,
  `RigBinder` also resolves `CORE::SocialGenomeRefComponent` into
  `SocialGenomeComponent` (see below).
- `Genome/CombatIdentity.h` + `CombatIdentityLoader` -- style/range/
  pressure/counter/mobility/risk (LAW C002/C003), loaded via a `.dominus`
  object's `combat_dna` ref. Bound by `COMBAT/HitSystem/CombatBinder`, not
  `RigBinder` -- CHARACTER never depends on COMBAT headers, only the
  reverse.
- `Genome/DecisionWeights.h` + `GenomeDecoder` -- deterministic decode of
  `CombatIdentity` strings into numeric weights (aggression,
  risk_tolerance, unpredictability, counter_bias, defense_bias), read by
  `COMBAT/PhysicsCombat/ClashSystem` and `COMBAT/MoveSelector`. This is
  what actually closes LAW C003 ("genome controls personality") -- see
  `../ROADMAP.md` Phase 3.5.
- `Genome/SocialGenome.h` + `SocialGenomeLoader` -- personality traits
  (trust/aggression/loyalty) and relationships (entity_id, free-form
  relation tag, signed strength), loaded via a `.dominus` object's
  `social_genome` ref. Bound by `RigBinder` (unlike `CombatIdentity`,
  `SocialGenome` has no dependency that would force it into `COMBAT`'s
  binder). Pure data, same as `CombatIdentity` before `GenomeDecoder`
  existed -- nothing reads `trust`/`aggression`/`loyalty` or relationship
  strength yet. See `../ROADMAP.md` Society Phase 1.
- `Genome/CreatureGenome.h` + `CreatureGenomeLoader` -- a 12-section
  schema (Identity/Taxonomy/Anatomy/Physiology/Senses/Locomotion/
  Cognition/Behavior/Ecology/CombatProfile/Growth/Evolution) for
  non-fighter entities, loaded via a `.dominus` object's
  `creature_genome` ref and bound by `RigBinder`, same path as
  `SocialGenome`. Genuinely strict validation (required `species_name`,
  every `[0,1]`-range field checked, every violation collected).
  `REGISTRY::CreatureGenomeCompiler` extends the hash pipeline to this
  genome type -- see `../ROADMAP.md` MONSTERFORGE Phase 1 for what that
  proved about the Registry Prototype generalizing, and what still
  doesn't (no decoder exists yet, so no `ImmutableArtifact`).
- `Genome/CreatureGenomeSemanticValidator.h` -- a genuinely separate
  second compiler pass beyond structural validation: five real
  cross-field biological consistency checks (flight vs. wing area,
  locomotion vs. limb count, diet vs. preferred prey, growth reaching
  maturity, aquatic locomotion vs. respiration -- the last one a
  warning, not an error, since air-breathing aquatic life is real).
  See `../ROADMAP.md` MONSTERFORGE Phase 1.5.
- `Genome/CombatStyleGenome.h` + `CombatStyleGenomeLoader` -- a fighting
  STYLE as its own entity, decoupled from any one fighter, giving
  `CombatIdentity.style`'s previously-undefined string
  (`"psycho_drunken_martial_arts"`) a real, structured referent:
  `ancestry` (parent style names), real `[0,1]` weights, free-form
  philosophy/rhythm/weaknesses. `CombatIdentity` itself was NOT touched
  -- this is additive. Verified by test to actually agree with
  Brooklyn's real `combat_dna` file, not just named alike. See
  `../ROADMAP.md` MASTER OF COMBAT Phase 1.
- `Genome/CombatPhysicsGenome.h` + `CombatPhysicsGenomeLoader` -- real
  per-entity physical combat traits (body/energy/impact) filling a
  specific gap: `COMBAT::PhysicsCombat::ClashSystem` already resolves
  attack-vs-attack; this supplies the data for attack-vs-**body**
  (`COMBAT::PhysicsCombat::ImpactSolver` supplies the math). Loaded
  standalone via its own JSON file -- not yet bound to an entity through
  `RigBinder` the way `SocialGenome`/`CreatureGenome` are. See
  `../ROADMAP.md` MASTER OF COMBAT Phase 2.
- `Genome/GameDesignGenome.h` + `GameDesignGenomeLoader` +
  `GameDesignCoherenceChecker` -- a genre/difficulty/risk-reward schema
  sitting above the other genomes, scoped from a proposed 20-dimension
  list down to 4 fields with actual engine hooks. `Evaluate()` produces
  ADVISORY notes, never hard errors -- "is this soulslike enough" has
  no objective ground truth the way wing-area-vs-flight does. Real
  criteria only for `soulslike`/`arcade` (the two genres the source
  document actually specified); every other genre produces zero notes,
  not a guessed opinion. See `../ROADMAP.md` DOMINUS ERA I.
- `Genome/VisualGenome.h` + `VisualGenomeLoader` -- form/skin/clothing/
  presence, real data describing what SHOULD eventually be rendered
  (`GRAPHICS` is still an empty placeholder). `Genome/
  VisualMemorySummary.h` -- the one file in `CHARACTER/Genome` that
  deliberately depends on `WORLD` (a documented connector, not a core
  genome type): `VisualMemoryDeriver::Derive()` computes a real summary
  from `WORLD::WorldHistory`'s actual event log instead of inventing a
  second, parallel, fabricated history system. See `../ROADMAP.md`
  DORRE. `VisualGenome` is now bound to entities via `RigBinder`
  (`CORE::VisualGenomeRefComponent`) -- same path as `SocialGenome`/
  `CreatureGenome`; Brooklyn's real fixture carries the ref.
- `Genome/MaterialGenome.h` + `MaterialGenomeLoader` -- object/material
  state (age/wear_state/damage_history/weather_exposure), matching the
  Reality Description Foundation document's own worked jacket example.
  `Genome/MaterialWearDeriver.h` -- second file that deliberately
  depends on `WORLD`, deriving `wear_state` from real `WorldHistory`
  damage events rather than inventing one (an honest, labeled heuristic,
  not a claim of real damage-magnitude consultation).
- `Genome/VisualStyleGenome.h` + `VisualStyleGenomeLoader` -- an art
  style as comparable, referenceable data (`style_id`/`name`/
  `visual_rules`/`influences`), distinct from `CombatStyleGenome`
  (fighting style). `influences` is data-only -- deliberately no
  auto-combine algorithm, same epistemic reasoning as declining Style
  Fusion (see `../ROADMAP.md` MASTER OF COMBAT Phase 2 / DORRE).
  `VisualGenome.presence.style_id` was added specifically so the two
  could be cross-checked for real; Brooklyn's fixtures prove it. See
  `../ROADMAP.md` Reality Description Foundation.

`AI/` remains a placeholder here -- combat AI foundation lives in the
top-level `AI/` module instead (see `../AI/README.md`); nothing
character-specific has needed its own AI binding yet.
