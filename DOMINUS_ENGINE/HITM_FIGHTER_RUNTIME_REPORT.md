# Track H Module 5A — CPU-Observable HITM Runtime Vertical Slice

Date: 2026-08-19 (continued session; lifetime-safety fix added in a
further continuation the same day)
Scope: does real, authored HITM Rivals fighter data actually DRIVE
DOMINUS simulation behavior — not just load and validate — and, where it
doesn't yet, exactly what stops it?

Method, same as every prior Track H module: every claim below was
checked against source or an actual test/CLI run in this session, not
inferred. Real Brooklyn data throughout unless stated otherwise.

## Executive summary

**Brooklyn now exists as a DOMINUS-controlled fighter.** A real
`CHARACTER/HitmBridge/HitmFighterRuntime`, built from his real imported
identity (Module 1), real combat genome including the read-engine
(Module 2), and the real global game rules (Module 4), initializes,
walks, jumps under real gravity, executes his real "special" move
through real startup/active/recovery frame counts, takes a hit with real
damage/hitstun/meter/hitstop numbers, and transitions his real five-tier
read-engine mechanic — all deterministically, all CPU-only, all verified
by 35 new tests (757/757 total) and a live `dominus-cli
hitm-fighter-runtime` run reproduced below. The `WorldTick`-registration
lifetime bug the first pass introduced has been fixed properly (stable
heap-allocated state, genuine `WorldTick` integration restored, not
routed around) and specifically verified safe under construction, every
kind of move/relocation, and destruction — including a clean run under
AddressSanitizer + UndefinedBehaviorSanitizer, not just repeated runs
that happened not to crash. See "A real bug this module found, then
fixed PROPERLY" below.

**He cannot be seen, heard, or actually fought against another player.**
No pixel has been drawn, no sound has played, no second fighter's
inputs have been read. Those are different, harder problems this module
does not claim to have solved — see the classification below.

## PROVEN (CPU/runtime behavior, verified in this environment)

Every line here was independently confirmed this session — a passing
test, a full clean rebuild, or a live CLI run with printed output, not
a design intention.

- **Fighter initialization** from real Brooklyn data. `HitmFighterRuntime::
  Create` fails closed on identity/genome fighter_id mismatch and on a
  missing read_engine (isolated via a deliberate real-data mutation,
  since no real fighter happens to combine "extractable special move"
  with "no read_engine" — see below).
- **Idle, movement**: `AdvanceFrame(kRight)` sets velocity to the real
  `game.json` `walkSpeed` (4.4) and moves the real `PHYSICS::RigidBody`/
  `WORLD::SpatialComponent` by exactly that amount in one frame.
- **Jump/gravity**: `AdvanceFrame(kJump)` applies the real `jumpVel`
  (-13.6); each subsequent frame adds the real `gravity` (0.6) via
  `PHYSICS::PhysicsSystem::Integrate` — unmodified, existing, tested
  code, run with `dt=1.0` so its generic force/mass/dt integration
  reproduces HITM's discrete per-frame convention exactly (see
  `HitmFighterRuntime.cpp`'s `AdvanceFrame` comment for why that
  specific choice is correct, not incidental). The fighter lands back on
  the real `ground` value and returns to idle.
- **Wall clamp** at the real `wallL`/`wallR`.
- **Input command ingestion**: a real, evidenced vocabulary
  (left/right/jump/special/block) — deliberately no invented "light
  attack" (see Module 5A's `HitmMoveInstance` finding below).
- **Attack/state transition** through Brooklyn's real "special" move's
  authored `startup`(14)/`active`(4)/`recovery`(18) frame counts,
  verified checkpoint-by-checkpoint.
- **Hit/damage resolution where the real data supports it**: `TakeHit`
  applies the real `damage`(62)/`hitstun`(34)/`blockstun`(13)/
  `meterGain`(14) fields and calls **`COMBAT::ReactionSystem::Determine`
  — real, pre-existing, unmodified engine code** — with Brooklyn's real
  `defense_profile.blockPreference` (0.18) as `defense_bias`. Result,
  computed live: `hit_power=62` against his real, shifted threshold
  produces `ReactionType::kKnockdown` — a genuine computed outcome, not
  asserted.
- **Meter/resource changes**: real `game.json` `meter.onHitTake`(5)/
  `onBlockTake`(2)/`onHitGive`(8) deltas, clamped at the real
  `meter.max`(100).
- **Hitstop representation**: a real hit-freeze — while
  `hitstop_frames_remaining>0` (set from the real per-move `hitstop`
  category through the real `game.json` `hitstopLight`/`hitstopHeavy`/
  `hitstopCounter` values), *nothing else* in the simulation advances,
  proven by a test that checks the hitstun counter is unchanged across
  every frozen frame and only starts counting down the frame after.
- **The five-tier read-engine mechanic transitions**: `GainRead`/
  `LoseRead` move through the real tiers (`"This Is Fun"` 1.0× →
  `"Perfect Hunter"` 1.42×), capped/floored at the real bounds, and
  `ResolveOutgoingDamage` implements the real authored law ("the read
  engine multiplies OUTPUT, never the table") literally:
  `62 × 1.21 = 75.02` at 3 reads, live-verified.
- **Deterministic frame advancement**: two independently constructed
  `HitmFighterRuntime` instances fed the identical 20-command script
  produce byte-identical `HitmFighterSnapshot`s after every single
  frame, not just at the end — and a negative-control test confirms
  *different* input genuinely produces *different* state, so the
  equality check isn't vacuous.
- **Invalid inputs/data fail deterministically, never silently**:
  re-triggering `kSpecial` mid-attack is dropped (proven: the state
  timer continues its original countdown, not reset); a second `kJump`
  input mid-air doesn't re-apply `jumpVel` (proven: velocity only gains
  one more frame of gravity, not a fresh jump); a fighter/genome
  mismatch, a missing read_engine, and a real fighter's real move with a
  genuinely different schema (below) all return `Result::Fail`, never a
  thrown exception, never a partial fighter.
- **No fake Brooklyn values were introduced.** Every number quoted above
  traces to a real, cited authored field. Where a real number was
  genuinely missing (hit_advantage/block_advantage, move `speed`,
  bone-relative hitboxes), the field was left unpopulated and documented
  — never guessed to fill a struct.
- **The full existing test suite remains green**: 656 → 751 across this
  entire Track H session, zero regressions, confirmed via a full clean
  rebuild (`rm -rf build`) and 10 repeat runs this module, plus (per
  Module 3's and Module 4's established discipline) a from-scratch `git
  clone` build+test cycle before pushing.

### A real bug this module found, then fixed PROPERLY (not routed around)

The first implementation registered `HitmFighterRuntime`'s per-frame
logic as a `this`-capturing `world::WorldSystemFn` closure on
`world_.Systems()` — the literal Phase 4.0 plugin pattern. Every factory
function returns a `HitmFighterRuntime` by value through
`Result<T>::Ok(std::move(...))`, and the compiler-generated move
constructor moves `world_` (including that closure) member-wise without
rewriting the captured raw `this` pointer. Result: a dangling-pointer
segfault the moment any moved-to runtime's `AdvanceFrame` ran the stale
closure — caught by actually running the test suite, not by review.

**First pass (superseded)**: removed the `WorldTick` registration
entirely and called the frame logic directly from `AdvanceFrame()`. That
made the segfault stop, but it also gave up genuine
`WorldTick`/`WorldSystemFn` integration to do it — treating "the
registration is unsafe" as a reason to stop registering, rather than
fixing why it was unsafe.

**The actual fix**: every mutable field `HitmFighterRuntime` owns
(`World`, `PhysicsSystem`, gameplay state, the read-engine, ...) now
lives in a private `FrameState`, allocated exactly once on the heap via
`std::unique_ptr<FrameState>` and never relocated for that FrameState's
life. `HitmFighterRuntime` itself holds nothing but that one
`unique_ptr` — so its compiler-generated move constructor only ever
transfers a pointer *value*; the `FrameState` object it points at never
moves, copies, or changes address. The registered `WorldSystemFn`
closure captures a raw `FrameState*` (obtained once, immediately after
allocation, before the state is ever handed to anything that could move
it) — never `this`. This is the standard "stable pImpl block" pattern
for a self-referential object, and it restores real `WorldTick`
registration (`world_.Systems().RegisterSystem(...)`, `world_.Tick(1.0f)`
from `AdvanceFrame()`) with **zero** trade-off against move-safety —
neither requirement was weakened to satisfy the other.

**Verification, specifically targeting this fix, all in this
continuation**:
- 6 new tests exercising construction → move-construct → move-assign →
  a 4-hop move chain → move-out-of-a-returning-function-with-the-
  original-destroyed → storage in a `std::vector` that reallocates
  (pushing 8 elements with no reserved capacity) → a long real run after
  heavy relocation. All pass, and each asserts real Brooklyn numbers
  (real `walkSpeed`, real `jumpVel`+`gravity`) still drive the
  post-move instance correctly, not just "didn't crash."
- Full suite (757/757) clean under **AddressSanitizer + UndefinedBehaviorSanitizer**
  (`-fsanitize=address,undefined`), 4 separate runs, zero findings —
  this is the actual, credible answer to "verify the WorldTick callback
  cannot reference a destroyed or relocated HitmFighterRuntime": ASan
  detects a stale-pointer dereference deterministically and loudly the
  first time it happens, regardless of whether the corrupted memory
  happens to still look plausible (which is exactly what made the
  original bug pass several clean-looking runs before the crash that
  caught it). Exact commands:
  ```
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" ..
  ninja tests/dominus_core_tests dominus-cli
  ./tests/dominus_core_tests   # 757/757, no ASan/UBSan output
  ./dominus-cli hitm-fighter-runtime <brooklyn identity dir> <game.json>  # identical real output, clean
  ```
- The live CLI demo (`dominus-cli hitm-fighter-runtime`) produces
  byte-identical real output before and after the fix (same frame
  numbers, same real `walkSpeed`/`jumpVel`/`gravity`/damage/meter/
  hitstop/reaction values) — the fix changed nothing about what real
  data drives, only how safely it survives relocation.
- Full clean rebuild (`rm -rf build`) + 15 repeat runs across the
  Release build, zero failures, zero flakes.

See `HitmFighterRuntime.h`'s top comment for the complete account.

### An additional architectural issue this fix uncovered, not yet exploited

Auditing every `RegisterSystem`/`AsWorldSystem` call in the engine (not
just this module's own code) while diagnosing the bug above found the
**same latent hazard already present in `PHYSICS/PhysicsSystem.h`**
(Phase 4.1, long-since-verified, untouched by Track H):

```cpp
world::WorldSystemFn AsWorldSystem() const {
    return [this](world::EntityRegistry& registry, float dt) { Integrate(registry, dt); };
}
```

This captures `this` exactly the way `HitmFighterRuntime`'s first,
buggy version did. It is **not currently triggered anywhere** —
grepping every real call site (`tests/physics/test_physics_system.cpp`,
`tests/physics/test_universal_physics_proof.cpp`,
`tests/world/test_hitm_rivals_as_world_entity.cpp`,
`TOOLS/Editor/dominus_cli.cpp`) shows every one of them keeps the
`PhysicsSystem` instance as a stable local variable for the registration's
entire lifetime, never moving or relocating it afterward — so the dormant
bug has simply never been exercised. `COMBAT/PhysicsCombat`'s sibling,
`PHYSICS/CollisionSystem::AsWorldSystem()`, is unaffected: it is `static`
and its lambda captures nothing.

**Not fixed in this session** — it is a different file, in a different,
already-shipped phase, with no failing test and no current caller that
triggers it, and fixing it is outside this continuation's explicit scope
(the `HitmFighterRuntime` lifetime bug). Recorded here as a real,
specific, actionable follow-up: any future code that constructs a
`PhysicsSystem`, calls `AsWorldSystem()` on it, and then moves or
relocates that `PhysicsSystem` (returns it by value, stores it in a
`std::vector` that reallocates, etc.) will hit the identical dangling-
`this` bug this module already found and fixed once. The same stable
pImpl-block fix would apply if/when a real caller needs it.

### A real, evidenced move-schema finding

Building `HitmMoveInstance::Extract` against all three real fighters'
`signature.json` (not just Brooklyn's) surfaced that **real move schemas
are not uniform**: Brooklyn's "special" has every field this module
needs (including `blockstun`, `range`, `height`, `hitstop`). Rocket's
real "special" ("Ghost Dash") is a *rush*-type move with none of
`blockstun`/`range`/`height` — it has a `rush` sub-object
(`velocityX`/`friction`/`hitRangeX`/`hitRangeY`) instead. Static's real
"special" ("Live Wire") has `range`/`height` but no `blockstun` and no
`hitstop` at all. `HitmMoveInstance::Extract` correctly **refuses**
Rocket's and Static's real specials rather than silently defaulting the
missing fields — confirmed live: `dominus-cli hitm-fighter-runtime
tests/fixtures/hitm_identity/rocket ...` fails cleanly with `missing
required field 'blockstun'`. A general move importer would need
per-move-*type* schemas (kick/rush/projectile/...), which is real,
separately-scoped future work.

## IMPLEMENTED BUT UNVERIFIABLE (GPU/display/audio/device-dependent)

Nothing new was implemented in this category this module — 5A was
scoped specifically to avoid it. Named here for completeness, carried
forward from the Track H audit: GPU/texture rendering, audio playback,
and real input-device polling all remain **not attempted**, not
"implemented but unverified" — there is no code for them to be unverified
against. The distinction matters: this module did not write renderer or
audio code and then hand-wave its correctness; it did not write that
code at all, because this sandbox has no GPU, no audio device, and no
display, and this project's own methodology (established in the GRAPHICS
phases) requires real-device verification for any such claim.

## NOT IMPLEMENTED (remaining systems, real and specific)

- **A second fighter / opponent.** `TakeHit` resolves an incoming move's
  numbers on one fighter; nothing generates or represents an opponent's
  own state, AI, or inputs. Two-fighter matches, hit priority, trades,
  and the read-engine's real gain/lose *trigger conditions* (counter
  hit, whiff punish, perfect guard, throw confirm / being counter hit,
  dropping a chain, whiffing a throw) all require an opponent model this
  module does not build — `GainRead`/`LoseRead` are exposed as a real,
  explicit, publicly-callable seam instead of guessed at.
- **Sprite/texture rendering** (Track H's own proposed Module 5C).
  `GRAPHICS` is untouched. `HitmPartsRig` (Module 3) exists but is not
  bound to anything drawn.
- **Audio pipeline** (Module 5D). No code.
- **Real input-device polling** (Module 5E). `HitmInputCommand` is a
  plain enum fed programmatically by tests/the CLI demo; no keyboard/
  controller/network input path exists.
- **Combo damage scaling** (`game.json`'s real `combat.scaleMin`/
  `scaleStep`). Imported (Module 4) but not applied here — doing so
  needs a combo-hit counter this module doesn't track.
- **Basic normals** (jab/light1-3). Not authored anywhere in HITM's real
  identity data (see `HitmMoveInstance.h`'s top comment) — generated
  output this track has never imported, a real, separately-scoped gap.
- **hit_advantage/block_advantage** on `MoveDef` — computable from real
  numbers by a real formula, but the exact convention isn't itself
  authored data; left unpopulated rather than guessed.
- **Bone-relative hitboxes** (`COMBAT::HitboxDef`) — real HITM reach is
  `range`/`height`, a fundamentally different, pose-free convention;
  no `Skeleton` exists to attach one to regardless (Module 3's own
  boundary).
- **CombatController/MotionGraphEvaluator integration.** Deliberately
  not attempted — see `HitmFighterRuntime.h`'s top comment for the full
  reasoning (would require inventing keyframe pose data no real HITM
  source has).

## Track split for what comes after this module

Classified by what this sandbox can actually verify, per the plan this
module was scoped against:

| Track | What it proves | Sandbox verification |
|---|---|---|
| **5A** (this module) | CPU game simulation | **Fully provable — done** |
| 5B | Sprite/texture asset ingestion (beyond Module 3's atlas-space data) | Mostly provable |
| 5C | GPU renderer integration | Code-level only — no real device here |
| 5D | Audio pipeline | Code-level only — no real device here |
| 5E | Input/device integration | Boundary only |
| 6 | Actual playable HITM vertical slice | Requires a real runtime/device outside this sandbox |

## Test count

757/757 (was 722 before this module). 35 new tests: 5 in
`test_hitm_move_instance.cpp`, 7 in `test_hitm_read_engine_state.cpp`, 23
in `test_hitm_fighter_runtime.cpp` (17 covering the vertical slice's
gameplay behavior, 6 added in the lifetime-safety continuation covering
move-construct, move-assign, a 4-hop move chain, move-out-of-a-function-
with-the-original-destroyed, `std::vector` reallocation, and a long real
run after heavy relocation). Full clean rebuild + 15 repeat runs across
both continuations, all green; a further 4 runs clean under
AddressSanitizer + UndefinedBehaviorSanitizer specifically for the
lifetime fix (see above) — no flakes observed anywhere. (The one segfault
encountered in the first continuation was deterministic — it reproduced
on every run before the fix, and has not recurred once since — so it is
reported above as a found-and-fixed bug, not logged as flakiness.)
