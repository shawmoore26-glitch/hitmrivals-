# Brooklyn vs Rocket Playability Audit — Dependency Map (audit only, zero implementation)

Requested scope: a pure dependency map — what exists, what connects, what's
missing, which missing pieces are code-shaped vs data-shaped, which modules
must not be reopened, and the smallest real sequence to close the gap. No
code was written or changed to produce this document. Every claim below is
either a direct file/line citation from this repo, or from the real
hitm-engine reference source (`scratchpad/inspect/hitm-engine`, the same
archive every prior Track H module has cited).

**The milestone, stated precisely** (so the sequence at the bottom has a
concrete target): DOMINUS loads Brooklyn and Rocket as two real,
independently-positioned `HitmFighterRuntime`s, steps them together frame by
frame, real attacks thrown by one land on the other using real range/height
data, real damage reduces a real HP pool, a real KO/round/match-win sequence
resolves — all still CPU-only, still driven by real authored numbers, no
new claim about pixels, sound, or input devices. That scope question (does
"playable" require the GRAPHICS pipeline to actually draw something, or is a
provable CPU match sufficient, matching every prior Track H milestone's own
definition of "proven") is flagged explicitly near the end — it changes the
sequence's last step and should be confirmed before implementation starts.

---

## 1. What DOMINUS already has

**Engine substrate actually used by Track H today** (all real, all tested,
none of it invented for HITM):
- `WORLD/Core/World.h`, `EntityRegistry`, `SpatialComponent`, `WorldSystemFn`
  registration (WORLD LAW 001/002/003) — each `HitmFighterRuntime` owns a
  **private** `world::World` (`HitmFighterRuntime.h:264`, `FrameState::world`)
  and registers its own per-frame logic as a `WorldSystemFn`. Two fighters
  today are two *independent* Worlds with no shared tick and no cross-fighter
  awareness — a real, current fact, not a design flaw to fix incidentally.
- `PHYSICS/PhysicsSystem.h` — real gravity/velocity/position integration,
  constructed with HITM's real `gravity` value; the `AsWorldSystem()`
  lifetime hazard was audited and fixed (Module 5A closure, `6f46c3e`).
- `COMBAT/ReactionSystem/ReactionSystem.h` — real, already-tested,
  unmodified; `HitmFighterRuntime::TakeHit` already calls
  `ReactionSystem::Determine` for reaction TYPE (stagger/knockback/launch/
  knockdown/block), fed by the defender's own real
  `defense_profile.blockPreference`.
- `CORE::Result<T>`, `MiniJson` — used throughout, unchanged.

**A second, separate, fully-built combat pipeline exists in DOMINUS that
Track H has never used and — as shown in §4 — largely cannot use as-is**:
`COMBAT/CombatController.h`, `COMBAT/HitSystem/{CollisionEvaluator,
CollisionResolver, MoveDef, Hurtbox, MoveLoader, HurtboxLoader}.h`,
`COMBAT/PhysicsCombat/ImpactSolver.h`, `ANIMATION/AnimationGraph/
MotionGraphEvaluator.h`. This is real, tested code (`tests/combat/
test_collision.cpp`, `test_collision_impact_loop.cpp`) — but every test
exercises it against DOMINUS-authored fixtures (`brooklyn_*.clip.json`,
`brooklyn_motion_graph.json`), never real HITM data. `HitmFighterRuntime.h`'s
own top comment already documents why (quoted in full in §4): it requires a
`Skeleton`/`Pose`/`AnimationClip`/`MotionGraph` stack no real HITM source
authors. This is DOMINUS's own generic engine, not something built for or by
Track H — cited here because "what DOMINUS already has" would be incomplete
without it, and because it explains *why* Track H built its own parallel,
smaller state machine instead of reusing this one.

**Track H's own module chain (Modules 0–5B, all formally closed except the
one explicitly-scoped `state_frame` reopening)**:
`HitmIdentityImporter` → `HitmCombatGenome` → `HitmGameRules` →
`HitmMoveInstance` → `HitmFighterRuntime` (state machine, physics, read
engine, `state_frame`) → `HitmAssetImporter`/`HitmAnimationSet`/
`HitmRigPlacement` → `HitmSpriteDrawData` (clip selection, per-part draw
data, secondary motion). Proven, for Brooklyn, as a live simulation
(`dominus-cli hitm-fighter-runtime`) and as live draw data
(`dominus-cli hitm-sprite-draw-data`); proven for Rocket and Static at the
*asset* layer only (`HitmAssetImporter` imports their real atlases/rigs/anim
cleanly, and `BuildSpriteDrawData` was proven against hand-constructed
snapshots for both — `HITM_SPRITE_ASSET_REPORT.md`'s "Track A gap #2
closed"). Neither Rocket nor Static has ever had a working
`HitmFighterRuntime::Create()` call — see §4/§5.

`HitmGameRules` (Module 4) has **already imported, and never yet consumed**:
`combat.scaleMin`/`scaleStep` (combo damage scaling), `meter.onBlockGive`/
`onBlockTake`/`onHitGive`/`onHitTake`, and — not previously called out in any
report — `rounds.toWin` / `rounds.timerSeconds` (`HitmGameRules.h:116`,
`HitmRoundRules`). This last one matters directly for this milestone: the
real round/match-win threshold is already sitting in imported, tested,
real data with nothing reading it yet.

---

## 2. What real HITM combat systems already exist (that DOMINUS has never ported)

This is the one genuinely new finding this audit surfaced: **`engine/combat/
CombatSystem.js`** (570 lines, real hitm-engine source, never cited in any
prior Track H report — confirmed by grep across every `.md` in this repo).
It is the real match/round driver Fighter.js and the identity/genome data
feed into, and it answers almost every open question the "second
fighter/opponent model" NOT-IMPLEMENTED item has carried since Module 5A's
own closure. Concretely, real and load-bearing for this milestone:

- **Real per-fighter HP**: `Fighter.js:23-24` —
  `const hp = Math.round(1000 * def.stats.healthMult); this.maxhp = hp; this.hp = hp;`.
  `1000` is a hardcoded engine constant (uniform across every fighter, not
  authored per-character data); `healthMult` is real, authored, per-fighter
  data already sitting in `character_dna.json` (Brooklyn: `0.94`, confirmed
  read from the real fixture). **DOMINUS currently tracks no HP field
  anywhere.**
- **Real fighter hitbox dimensions**: `Fighter.js:28` —
  `this.w = 52; this.hgt = 150;`. Also hardcoded, uniform, not per-fighter.
- **Real position-based melee hit-check** (`CombatSystem.js:370-374`, the
  `_melee` function) — this is the real answer to "how does a real HITM
  attack actually connect," and it is **not** a bone-relative hitbox test:
  ```
  _melee(f, foe, def) {
    const hx = f.x + f.facing*(f.w/2 + def.range/2);
    if (Math.abs(hx - foe.x) < (def.range/2 + foe.w/2) &&
        Math.abs((f.y-80)-(foe.y-80)) < (def.height || 105))
      this.applyHit(foe, f, def);
  }
  ```
  Flat x/y proximity against the attacker's real `range`/`height` (already
  imported by `HitmMoveInstance` for Brooklyn and Static) and the two
  fighters' real `x`/`y` positions (already tracked by `HitmFighterSnapshot`
  today). No skeleton, no pose, no bone lookup — confirming, independently
  of Track H's own prior reasoning, that `COMBAT::CollisionEvaluator` was
  never the right tool for this (see §4).
- **Real damage/scale/chip/counter-hit/read-engine pipeline**
  (`CombatSystem.js:383-456`, `applyHit`) — combo scaling
  (`Math.max(scaleMin, 1 - scaleStep*combo)`), chip-on-block
  (`chipMult`), counter-hit detection and bonus, perfect-guard detection,
  the read-engine damage multiplier applied at the point of damage (exactly
  matching `HitmFighterRuntime::ResolveOutgoingDamage`'s own documented "the
  read engine multiplies OUTPUT, never the table" law), real knockback
  `vx`/`vy` derived from move flags (`launcher`/`knockdown`/`spike`), and —
  the actual HP application — `d.hp = Math.max(0, d.hp - _out)`.
- **Real KO and round/match resolution** (`CombatSystem.js:458-473`,
  `_ko`/`_endRound`) — `if (d.hp<=0) this._ko(d, atk)`, then
  `winner.rounds++`, then `if (winner.rounds >= cfg.rounds.toWin)
  phase='matchOver'`, else `resetRound()`.
- **Real two-fighter starting layout** (`CombatSystem.js:41`) —
  `f.x = i===0?300:760; ... f.facing = i===0?1:-1`. `HitmFighterRuntime::
  Create` today always centers a single fighter at `(wallL+wallR)/2` — a
  real, different, single-fighter-only convention that a two-fighter match
  cannot reuse unmodified.
- **Real continuous facing** (`CombatSystem.js:488,509`) —
  `if (f.state!==ATTACK) f.facing = foe.x>=f.x?1:-1` — auto-turns to face
  the opponent every frame except mid-attack (facing locks for the
  attack's duration). This is the exact, previously-unresolved input
  `HitmSpriteDrawData.h`'s own `walk`-vs-`walkBack` gap has been waiting on
  since Module 5B's own closure.
- **Rocket's real special is a structurally different move TYPE, not just a
  wider schema.** "Ghost Dash" (`rocket/signature.json`'s real `special`)
  has no `blockstun`, no flat `range`/`height` — instead a `rush` sub-object
  (`velocityX`, `friction`, `hitRangeX`, `hitRangeY`, `leaveGhost`). In the
  real engine this is resolved through a *different* code path
  (`_zoneHit`, `CombatSystem.js:376-379`, fed by a scheduled dash-travel
  step, not `_melee`). Making Rocket's own attack genuinely work is real,
  separately-scoped work beyond widening `HitmMoveInstance`'s required-field
  set — see §9's Step 7.

None of `CombatSystem.js` has been read by any prior Track H module or
report. This audit is the first time it's been cited.

---

## 3. What connects DOMINUS and HITM today

Exactly the Module 0→5B chain in §1, plus `ReactionSystem::Determine` and
`PhysicsSystem`. That's the complete list — there is no existing two-fighter
connection, no HP, no hit-detection-from-position, and no round/match state
anywhere in the repo today. Every one of those is new ground, not a rewire
of something partially working.

---

## 4. What's missing, and why it's missing (the load-bearing distinction)

Quoting `HitmFighterRuntime.h`'s own top comment, because it is the single
most important piece of prior reasoning this audit rests on:

> A REAL ARCHITECTURAL GAP, found and NOT papered over: COMBAT's existing
> CombatController/MotionGraphEvaluator ... require a real Skeleton, real
> AnimationClips, and a real MotionGraph — all three exist today only as
> DOMINUS-authored fixtures ... never derived from real HITM data ...
> Reusing CombatController/MotionGraphEvaluator here would mean driving
> Brooklyn through DOMINUS's own invented animation fixtures — exactly the
> strawman pattern this whole track exists to move away from.
>
> A SECOND REAL GAP: real HITM move reach is a flat `range`/`height` pair
> ..., not COMBAT::HitboxDef's bone-relative circles (which require a live
> Pose ...). TakeHit() below therefore does not use
> COMBAT::CollisionEvaluator or Hurtbox at all.

§2's discovery of `_melee`'s real formula *independently confirms* this
reasoning was correct, from the real engine's own source rather than
DOMINUS's own inference: the real engine itself never uses bone-relative
hitboxes for 2D fighters either. So the missing hit-detection piece is not
"finish wiring `CollisionEvaluator`" — it is "port `_melee`'s real,
much smaller, position-only formula," which needs no Skeleton/Pose/
MotionGraph at all, and therefore does not touch the permanently-blocked
bind-pose gap.

The complete missing-piece list, each tagged by category (§5/§6/§7 below):

| Missing piece | Category |
|---|---|
| Two-fighter match/round driver (nothing like `_tick`/`resetRound` exists) | new implementation |
| Position-based hit-check (`_melee` port) | new implementation |
| Real per-fighter HP / KO | new implementation (real formula known) |
| Real facing field/derivation | new implementation (real rule known) |
| Real two-fighter starting positions | new implementation (real values known) |
| Rocket/Static existing as a runtime fighter at all | new implementation, blocked on two independent `Create()` hard-requirements — see §5 |
| Rocket's own real special (`rush`/`_zoneHit` path) | new implementation, larger — see §9 Step 7 |
| Combo damage scaling application | new implementation (data already imported) |
| Counter-hit / perfect-guard detection | new implementation (needs opponent-state awareness `TakeHit` doesn't have today) |
| Full bone-hierarchy FK / rendered pixels | **blocked — real data does not exist**, see §7 |
| Basic normals (jab/light1-3) | **blocked — real data does not exist**, see §7 |

---

## 5. Missing pieces already representable in code (no new data needed)

Everything in this section can be built entirely from data DOMINUS has
already imported and validated — no HITM authoring gap blocks any of it:

- **HP/KO.** `1000 * healthMult` is 100% real: the constant is hardcoded
  and uniform in the real engine, `healthMult` is real per-fighter data
  already sitting, losslessly, inside `HitmIdentityRecord::character_dna`
  (Module 1's own raw `core::json::Value` import) — confirmed present in
  all three real fighters' own fixtures in this audit: Brooklyn `0.94`,
  Rocket `1.09`, Static `0.96` (three genuinely different real values, not
  one reused across the roster, same pattern as the secondary-motion
  spring constants Track A gap #1 found). It has not yet been extracted
  into a typed field by any downstream module (`HitmCombatGenome` does not
  expose it today) — Step 3 below needs one small, new, typed-extraction
  addition of the same shape every other real field this track has
  extracted, not a search for missing data.
- **Facing.** The real rule (`foe.x>=f.x?1:-1`, locked during attack) needs
  only each fighter's own `x` (already tracked) plus the opponent's `x`
  (needs the match driver to supply it — a wiring problem, not a data gap).
- **Position-based hit-check.** `_melee`'s formula needs `range`/`height`
  (already imported for Brooklyn and Static via `HitmMoveInstance`), the two
  hardcoded constants (`w=52`, `hgt=150`), and both fighters' `x`/`y`
  (already tracked). Zero new authored data required.
- **Real two-fighter starting positions/round/match state.** `x=300/760`,
  `rounds.toWin`/`timerSeconds` — literal, already-imported or
  directly-portable real constants.
- **Combo scaling.** `scaleMin`/`scaleStep` already imported by
  `HitmGameRules` (Module 4), simply never multiplied into a damage
  calculation yet.
- **Rocket and Static existing as runtime fighters for movement/blocking/
  taking-hits (their OWN special aside).** Both fighters' real
  `combat_genome.json` genuinely has no `read_engine` key — confirmed by
  direct read of both real fixture files in this audit
  (`rocket/combat_genome.json`, `static/combat_genome.json`). This is real,
  verified authored information ("this fighter has no read engine"), not a
  gap — `HitmFighterRuntime::Create()`'s current hard requirement for a
  non-null read engine (`HitmFighterRuntime.cpp:23-30`) is a DOMINUS
  *implementation choice* (documented at the time as "this vertical slice
  starts with Brooklyn specifically because he is the one real fighter who
  has one"), not a reflection of missing data. Relaxing it to tolerate
  absence is new implementation over already-real, already-imported data.

---

## 6. Missing pieces that require new implementation, using real data that exists but needs a new code path

- **The two-fighter match driver itself.** Nothing like it exists at any
  layer of DOMINUS today (confirmed — no `Match`, `Round`, or two-entity
  combat orchestrator anywhere under `WORLD/` or `COMBAT/`). This is
  necessarily new: stepping two `HitmFighterRuntime`s in lockstep, computing
  facing, running the hit-check, applying `TakeHit`/
  `ResolveOutgoingHitLanded` across the fighter boundary, tracking HP/KO,
  and running the round/match state machine.
- **Rocket's own real "Ghost Dash."** Real data for it fully exists
  (`rush.velocityX/friction/hitRangeX/hitRangeY`), but resolving it needs
  porting a second, different real algorithm (`_zoneHit` + the real dash-
  travel scheduling around it, `CombatSystem.js:340-368`, only partially
  read in this pass) — genuinely more work than the position-based `_melee`
  port, and separable from the rest of this milestone (see §9 Step 7).
- **Counter-hit / perfect-guard detection.** The real formulas exist
  (`CombatSystem.js:410-422`) but require the DEFENDER's own live state at
  the moment of impact (is the defender mid-attack-startup? mid-perfect-
  guard-window?) — today `TakeHit(incoming, blocking)` takes `blocking` as
  an opaque caller-supplied bool with no defender-state introspection. The
  match driver (§ above) would need to read the defender's own
  `HitmFighterSnapshot` to derive these, which it can (state/state_frame
  are already public), but nothing does today.

---

## 7. Missing pieces genuinely blocked on absent HITM authored data (do not fabricate)

- **Full bone-hierarchy forward kinematics / rendered pixels.** The
  permanently-documented gap (`bones[].at`/`.part` missing from real
  `parts.json`, Module 3's original finding, re-confirmed at every
  subsequent Track H closure). Nothing in this audit changes that status.
  Not required for the CPU-simulated definition of "playable" this track
  has used for every prior milestone — see the scope flag at the end of
  this document.
- **Basic normals (jab/light1-3).** Confirmed, again, not authored anywhere
  in real non-generated HITM sources (`HitmMoveInstance.h`'s own finding,
  unchanged). If "a complete playable fight" is read to require normals in
  addition to each fighter's special, this stays blocked; if the milestone
  is scoped to specials + movement + block (as `HitmInputCommand` already
  is, today, for both fighters), it is not needed.
- **hit_advantage/block_advantage.** Computable from real numbers by a real
  formula, but the convention itself isn't authored data — stays deferred,
  not required for hit resolution to work (only for frame-data display).

---

## 8. Which modules must not be reopened

- **Modules 0–4** (JSON parser, Identity Import, Combat genome mapping,
  2D sprite-cutout rig, global game rules) — complete, closed, nothing this
  milestone needs is missing from what they already import (see §1's
  `HitmGameRules.Rounds()` finding). No reason to touch any of them.
- **Module 5B Phase 1** (asset importer, animation set, rig placement,
  sprite draw data, secondary motion, Track A gaps #1/#2/#3) — complete,
  closed. This milestone's "real authored assets" half is already provably
  satisfied by this module's existing output for all three fighters; a
  two-fighter match driver would *consume* `BuildSpriteDrawData` for both
  fighters' snapshots exactly as gap #2 already proved it can, not modify
  it.
- **Module 5A's `FrameState`** — already reopened once, deliberately and
  narrowly, for `state_frame`. Any further additions this milestone needs
  (HP, facing) should be held to the exact same discipline that reopening
  used — small, additive, zero change to existing gameplay numbers — not
  treated as a new blank check to redesign the class. This is a
  recommendation for *how* to scope Step 3/4 below, not a claim those
  fields can be added without touching the file at all.
- **The bind-pose/FK gap** (§7) — stays permanently blocked. No step in
  §9 touches it, and none should, regardless of schedule pressure.

---

## 9. Smallest real sequence to get Brooklyn vs Rocket playable

Dependency-ordered; each step names exactly what data it needs and confirms
that data already exists (per §5/§6 above) unless flagged otherwise. Nothing
in this section has been implemented — it's the ordering this audit was
requested to produce, for the user to scope/approve before any code is
written.

1. **Relax `HitmFighterRuntime::Create()`'s read-engine requirement.** Make
   a fighter's read engine optional (`std::optional`/nullable), since
   "no read engine" is itself real, verified data for Rocket and Static, not
   a gap. Unblocks `Create()` past its first Rocket/Static-specific failure
   point.
2. **Decide, and then implement, how `Create()` handles a fighter whose
   "special" doesn't extract with today's schema (Rocket, Static).** Two
   real options, genuinely different in cost — this is the one decision in
   this sequence that most changes total scope, and should be made
   explicitly rather than defaulted:
   - **(a) Minimal:** let `Create()` succeed with the special move slot
     empty/disabled for a fighter whose extraction fails, documented, not
     silently defaulted — `kSpecial` input is then a real, explicit no-op
     for that fighter until Step 7 closes it. Gets Rocket into a real match
     (walking, blocking, taking real hits, real HP, real KO) fastest.
   - **(b) Full:** extend `HitmMoveInstance::Extract` with a real
     rush-type schema for Rocket's data shape before touching `Create()` at
     all — defers "Rocket exists" until Rocket's own attack also works.
3. **Add real per-fighter HP** (`round(1000*healthMult)`) and a KO
   flag/state to `HitmFighterSnapshot`/`FrameState`, applied inside the
   existing `TakeHit` damage path — additive, same discipline as
   `state_frame`.
4. **Add real facing** to `HitmFighterSnapshot`/`FrameState` — needs an
   opponent `x` to compare against, which only the match driver (Step 6)
   can supply, so this field's *value* is only meaningful once Step 6
   exists, but the field and its "auto-face-unless-attacking" rule can be
   written now.
5. **Write the position-based hit-check** (`_melee` port) as a small, pure,
   standalone function — mirroring how `HitmSpriteDrawData` already lives
   outside `HitmFighterRuntime` as a pure consumer, not a method on the
   runtime itself. Takes both fighters' snapshots plus the attacker's
   current `HitmMoveInstance`; returns whether it connects.
6. **Write the minimal two-fighter match driver.** Owns two
   `HitmFighterRuntime`s, sets real starting positions/facing per
   `CombatSystem.js`'s real convention, steps both once per real frame,
   calls Step 5's hit-check when the attacker is in `kAttackActive`, calls
   `defender.TakeHit(...)` + `attacker.ResolveOutgoingHitLanded(...)` across
   the boundary, tracks HP/KO (Step 3) and the real `rounds.toWin`
   threshold (already-imported `HitmGameRules::Rounds()`) for match-win.
   This is the actual "Brooklyn vs Rocket playable" deliverable — a live
   CLI demo of this driver is the natural verification artifact, matching
   every prior module's `dominus-cli` proof pattern.
7. **(Fast-follow, not required for the milestone as scoped above.)** Port
   Rocket's real rush/dash-attack resolution (`_zoneHit` + travel
   scheduling) so Rocket's own "Ghost Dash" genuinely connects too, closing
   whichever of Step 2's options was taken at (a).

**The one open scope question this audit surfaces rather than resolves**:
every prior Track H milestone has defined "proven" as CPU-observable
(a passing test, a live CLI run) without requiring GRAPHICS to actually
render a pixel — Module 5B's own closure was explicit that "real authored
assets" meant deterministic draw data, not drawn pixels. If "playable" for
this milestone means the same thing (a live `dominus-cli` match demo,
provably driven by real data, real assets available as draw data but not
rendered), Step 6 above is the finish line. If it means something is
actually drawn on screen, that pulls in GRAPHICS/Vulkan integration — a
substantially larger, still-real, still-not-fabricated scope this document
deliberately does not size, since it wasn't asked to.
