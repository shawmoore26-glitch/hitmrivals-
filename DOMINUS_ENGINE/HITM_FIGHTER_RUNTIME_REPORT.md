# Track H Module 5A — CPU-Observable HITM Runtime Vertical Slice

Date: 2026-08-19 (continued session; lifetime-safety fix added in a
further continuation, then the dormant `PhysicsSystem::AsWorldSystem()`
hazard audited and fixed in a fourth continuation, all the same day)
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
by 43 new tests (765/765 total) and a live `dominus-cli
hitm-fighter-runtime` run reproduced below. The `WorldTick`-registration
lifetime bug the first pass introduced has been fixed properly (stable
heap-allocated state, genuine `WorldTick` integration restored, not
routed around) and specifically verified safe under construction, every
kind of move/relocation, and destruction — including 4 clean runs under
AddressSanitizer + UndefinedBehaviorSanitizer with leak and stack-use-
after-return detection enabled, not just repeated runs that happened not
to crash. See "Exhaustive lifetime-safety verification" below for the
full, separately-reported results. A fourth continuation then closed out
the one architectural follow-up that verification surfaced: the dormant,
identically-shaped `this`-capture hazard in
`PHYSICS::PhysicsSystem::AsWorldSystem()` has been audited and fixed —
not merely documented — with its own regression tests and its own ASan
run. See "`PhysicsSystem::AsWorldSystem()` lifetime audit and fix" below.

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
- **The full existing test suite remains green**: 656 → 765 across this
  entire Track H session, zero regressions, confirmed via full clean
  rebuilds (`rm -rf build`) and repeat runs across every continuation,
  plus (per Module 3's and Module 4's established discipline) a
  from-scratch `git clone` build+test cycle before pushing.

## PROVEN WITH ASAN (lifetime/memory safety, verified in this environment)

Distinguished from the CPU-behavior claims above because they are a
different kind of claim: not "does the simulation compute the right
number" but "does this object survive being moved, stored, and destroyed
without touching freed or relocated memory." Full detail, the exact
sequences tested, and the exact commands run in "Exhaustive
lifetime-safety verification" below — summarized here:

- 11 `HitmFighterRuntime_LifetimeSafety_*` tests cover: construction
  actually registering a live `WorldTick` system (not a no-op);
  move-construct, move-assign, a 4-hop move chain, and the exact
  construct→move→execute→destroy and construct→move→move-again→execute→
  destroy sequences requested; return-from-a-function with the original
  wrapper destroyed; storage in a reallocating `std::vector`; a second
  runtime destroyed mid-scope while a sibling's `World`/`WorldTick`
  remains alive and keeps ticking correctly afterward; and 25
  repeated construct→advance→destroy cycles stressing allocator address
  reuse (the actual mechanism by which a stale pointer corrupts
  unrelated future data).
- All 11 pass in a normal build **and** in a from-scratch AddressSanitizer
  + UndefinedBehaviorSanitizer build, 4 separate ASan runs, with leak
  detection and stack-use-after-return detection both enabled — zero
  findings.
- The live CLI demo's real output is **byte-for-byte identical** whether
  run from the normal build or the ASan-instrumented one — the fix
  changed how safely the object survives relocation, not what real HITM
  data drives.

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

### An additional architectural issue this fix uncovered (later audited and fixed — see below)

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

**Not fixed at the time this was written** — it was a different file, in
a different, already-shipped phase, with no failing test and no current
caller that triggered it, and fixing it was outside that continuation's
explicit scope (the `HitmFighterRuntime` lifetime bug). Recorded as a
real, specific, actionable follow-up. **It has since been audited and
fixed** in a fourth continuation, explicitly requested rather than left
for whenever a caller happened to trip it — see the next section.

## `PhysicsSystem::AsWorldSystem()` lifetime audit and fix (fourth continuation)

Requested explicitly, before closing Module 5A: determine whether the
dormant hazard above can actually outlive or outmove its owning
`PhysicsSystem`, and either prove it's inherently safe or fix it with the
same ownership discipline used for `HitmFighterRuntime` — not leave a
known callback lifetime hazard sitting in the engine going into the next
integration layer.

**Verdict: not provably safe as it stood.** "No current caller exploits
it" was re-confirmed (still true — `dominus_cli.cpp` and both physics
test files keep every `PhysicsSystem` as a stable, unmoved local for the
registration's whole lifetime), but that is a fact about today's callers,
not a property of the type. `PhysicsSystem` is a general-purpose,
publicly reusable class — restricting how *future* callers are allowed
to use it (the fix that was explicitly rejected for `HitmFighterRuntime`)
is equally unacceptable here. The hazard is real: construct a
`PhysicsSystem`, register `AsWorldSystem()`, then move/reassign/destroy
that `PhysicsSystem` before or between `World::Tick()` calls, and the
closure reads through a dangling `PhysicsSystem*`.

**Fix, and why it's a different shape than `HitmFighterRuntime`'s:**
`HitmFighterRuntime::FrameState` is a large, multi-field, self-referential
block, so the correct fix was a heap-allocated, never-relocated pImpl
block. `PhysicsSystem`'s *entire* runtime state is one `float`
(`gravityY_`) — there is nothing to justify heap allocation. Instead,
`AsWorldSystem()` now captures `gravityY_` **by value** and calls a new
`static IntegrateWithGravity(registry, dt, gravityY)` helper (the same
logic `Integrate()` itself now delegates to) instead of capturing `this`
and calling back through the source object:

```cpp
world::WorldSystemFn AsWorldSystem() const {
    float gravity = gravityY_;
    return [gravity](world::EntityRegistry& registry, float dt) {
        IntegrateWithGravity(registry, dt, gravity);
    };
}
```

The returned closure owns a private copy of the only state it needs. It
has **zero** pointer/reference dependency on the `PhysicsSystem` instance
that produced it — there is nothing left for a move, reassignment, or
destruction of that instance to invalidate. This is a *stronger*
guarantee than "provably safe under the current ownership model": it is
safe under every ownership model, because the hazard's precondition (a
callback reading through the original object) no longer exists. No
indirection, no `unique_ptr`, no move-constructor customization was
needed or added — `PhysicsSystem` keeps its plain, trivially-movable
value semantics throughout.

**3 new regression tests** in `tests/physics/test_physics_system.cpp`,
targeting the exact sequences that would have failed under the old
`[this]`-capturing version:

| Test | What it proves |
|---|---|
| `PhysicsSystem_AsWorldSystem_ClosureOutlivesDestroyedSourceObject` | The source `PhysicsSystem` is destroyed *before the world is ever ticked* — real gravity still integrates afterward. |
| `PhysicsSystem_AsWorldSystem_ClosureUnaffectedByMoveThenReuseOfSourceSlot` | The source is moved away, then its old variable slot is overwritten in place with a different, easily-distinguished gravity value — the registered closure still uses the value captured at `AsWorldSystem()` call time, not whatever now sits at that address. |
| `PhysicsSystem_AsWorldSystem_EachClosureKeepsItsOwnGravityIndependently` | Two independent `PhysicsSystem`s (different gravity), both destroyed before either `World` ticks — each world's fall matches its own captured gravity, proving no aliasing between the two closures. |

**Verification, run and reported separately, same protocol as the
`HitmFighterRuntime` lifetime fix:**

1. **Clean normal build** (`rm -rf build`, Release): zero errors, zero
   warnings.
2. **Full normal test suite**: **765/765 passed**, exit code 0 (762 +
   3 new `PhysicsSystem_AsWorldSystem_*` tests). All 3 confirmed passing.
3. **Clean ASan+UBSan build** (separate `build-asan/` directory, Debug,
   `-fsanitize=address,undefined -fno-omit-frame-pointer -g`): zero
   errors, zero warnings.
4. **Lifetime tests under ASan**: all 3 `PhysicsSystem_AsWorldSystem_*`
   tests pass, plus all 11 `HitmFighterRuntime_LifetimeSafety_*` tests
   re-confirmed passing in the same binary.
5. **Full ASan suite**: **765/765 passed**, exit code 0, across 4
   separate runs with
   `ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:check_initialization_order=1:detect_stack_use_after_return=1`.
   Grepped specifically for sanitizer diagnostic markers (`ERROR:
   AddressSanitizer`, `ERROR: UndefinedBehaviorSanitizer`, `SUMMARY:`,
   `runtime error:`, `heap-buffer-overflow`, `use-after-free`,
   `use-after-move`, `stack-use-after-return`/`-scope`) rather than a
   naive substring match (test names like `..._IsError` would otherwise
   false-positive on a bare "error" grep) — **zero matches** across all
   4 runs.

**Live behavior re-verified unchanged**: `dominus-cli physics
tests/fixtures/brooklyn_canonical.dominus` and `dominus-cli
hitm-fighter-runtime` were both run against the normal build and the
ASan build and `diff`'d — byte-for-byte identical output in both cases.
`PhysicsSystem::Integrate()`'s math is untouched (the body only moved
into a `static` helper); this was a lifetime fix, not a behavior change,
and the live demos confirm it.

`CollisionSystem::AsWorldSystem()` was re-checked in the same pass and
remains what it always was — `static`, capturing nothing — so it carries
no equivalent hazard and needed no change.

No test was weakened, no move semantics were disabled or restricted (the
fix doesn't even touch `PhysicsSystem`'s move/copy members — they stay
compiler-generated and trivial), and no ASan finding was suppressed.

## Exhaustive lifetime-safety verification (second continuation)

Requested explicitly: prove the exact critical sequence — construct →
register `WorldTick` callback → move → execute a frame → destroy → no
callback into a dead object — and the extended sequence (construct →
move → move again → execute → destroy), under ASan specifically, since
the original defect was precisely a would-be use-after-move. Five new
tests were added on top of the six from the first lifetime continuation,
for **11 total** `HitmFighterRuntime_LifetimeSafety_*` tests:

| Test | What it proves |
|---|---|
| `ConstructionActuallyRegistersAWorldTickSystem` | The registered closure genuinely runs (frame counter only advances inside it) — not just that `AdvanceFrame` doesn't crash. |
| `ConstructMoveExecuteDestroy` | The literal minimal critical sequence, real `walkSpeed` still applies post-move. |
| `ConstructMoveMoveAgainExecuteDestroy` | The extended sequence — two real moves before any execution. |
| `DestructionDoesNotAffectSiblingRuntimeOrItsWorld` | A second, independent runtime destroyed mid-scope has zero effect on a survivor's real behavior — "destruction while the world/tick system remains alive" and "subsequent world ticks after runtime destruction," both from the survivor's side. |
| `RepeatedConstructDestroyCyclesStressAllocatorReuse` | 25 create→advance→destroy cycles — the actual mechanism by which a stale-pointer bug corrupts *unrelated* future data (freed memory getting reused), not merely "doesn't crash once." |

(Plus the six from the first continuation: `MoveConstructThenAdvanceFrame`,
`MoveAssignThenAdvanceFrame`, `MultipleSequentialMoves`,
`OriginalWrapperDestroyedAfterMove`, `StoredInVectorAndReallocated`,
`ManyMovesThenManyFrames`.)

**Results, exactly as requested, reported separately:**

1. **Clean normal build** (`rm -rf build`, Release): zero errors, zero
   warnings.
2. **Full normal test suite**: **762/762 passed**, exit code 0. All 11
   lifetime tests listed above pass. 4 repeat runs, all clean.
3. **Clean ASan build** (separate `build-asan/` directory, Debug,
   `-fsanitize=address,undefined -fno-omit-frame-pointer -g`): zero
   errors, zero warnings.
4. **Lifetime/integration tests under ASan**: all 11
   `HitmFighterRuntime_LifetimeSafety_*` tests pass; grepped directly out
   of the sanitizer-instrumented binary's output, not inferred.
5. **Full ASan test suite** (the test framework runs all registered
   tests with no name-filtering flag, so "full" and "the lifetime tests"
   are the same binary run): **762/762 passed**, exit code 0, with
   `ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1` —
   the stronger-than-default leak and stack-use-after-return checks
   specifically relevant to a `unique_ptr`-owned heap block. Grepped for
   `ERROR: AddressSanitizer`, `ERROR: UndefinedBehaviorSanitizer`,
   `runtime error:`, `SUMMARY:`, `heap-buffer`, `use-after`, `leak` —
   **zero matches** across 4 separate runs.

**Gameplay behavior explicitly re-verified unchanged**: the live
`dominus-cli hitm-fighter-runtime` demo's output was captured before this
continuation's changes and again from the ASan-instrumented binary after
— `diff` reports the two **byte-for-byte identical** (same frame numbers,
same real `walkSpeed`/`jumpVel`/`gravity`/damage/meter/hitstop/reaction
values throughout the full 84-frame run). The lifetime-safety work added
tests and verification; it did not touch, and did not change, what real
HITM data drives.

No test was weakened, no move constructor/assignment was disabled or
deleted, and no ASan finding was suppressed to reach this result — there
were none to suppress.

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

(The dormant `PhysicsSystem::AsWorldSystem()` lifetime hazard previously
listed here has been audited and fixed — see "`PhysicsSystem::
AsWorldSystem()` lifetime audit and fix" above. It is no longer an open
item.)

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

765/765 as of this module's own formal closure (was 722 before this
module, 656 before Track H); 837/837 as of the fifth continuation's
`state_frame` closure (see that section, and `HITM_SPRITE_ASSET_REPORT.md`,
for everything added by Module 5B and the Track A gap closures in
between); 842/842 as of the sixth continuation's Phase 1 runtime
foundation closure; 857/857 as of the seventh continuation's Phase 2
real-combat closure; **869/869** as of the eighth continuation's Phase 3
touch points above (see `HITM_MATCH_REPORT.md` for everything Phase 3
itself added on top of this class). 43 new tests at this module's own
closure: 5 in `test_hitm_move_instance.cpp`, 7 in
`test_hitm_read_engine_state.cpp`, 28 in `test_hitm_fighter_runtime.cpp`
(17 covering the vertical slice's gameplay behavior, 11
`LifetimeSafety_*` tests added across two continuations covering every
relocation path requested), and 3 in `test_physics_system.cpp`
(`PhysicsSystem_AsWorldSystem_*`, added in the fourth continuation to
close the dormant lifetime hazard); plus 6 more in
`test_hitm_fighter_runtime.cpp` in the fifth continuation (`state_frame`,
see above); plus 5 net new in the sixth continuation (Phase 1 — one
existing test rewritten in place, five added, see above); plus 6 more in
`test_hitm_fighter_runtime.cpp`, 8 in the new `test_hitm_melee_hit_check.cpp`,
and 1 in `test_hitm_sprite_draw_data.cpp` in the seventh continuation
(Phase 2, see above); plus 4 more in `test_hitm_fighter_runtime.cpp` in
the eighth continuation (Phase 3's two touch points, see above) — 49 in
`test_hitm_fighter_runtime.cpp` total. Full clean rebuilds + repeat runs
across all eight continuations (Release: 15+ repeats; ASan+UBSan: 4
repeats of the full suite plus the live CLI demo for the first four
continuations, run once more for the fifth, sixth, seventh, and eighth),
all green — no flakes observed anywhere. (The one segfault encountered
in the first lifetime
continuation was deterministic — it reproduced on every run before the
fix, and has not recurred once, under any build configuration, since —
so it is reported as a found-and-fixed bug, not logged as flakiness. The
`PhysicsSystem` hazard never actually crashed in this codebase, since no
call site exploited it — it was found by audit and closed pre-emptively,
not by chasing an observed failure.)

## Final Module 5A status

- **PROVEN** (CPU runtime behavior): fighter init, movement, jump/gravity,
  attack state transitions, hit/damage resolution, meter, hitstop,
  five-tier read-engine transitions, deterministic frame advancement,
  deterministic failure on invalid input/data — all from real Brooklyn
  data, all backed by a passing test or a live CLI run. See "PROVEN"
  above.
- **PROVEN WITH ASAN** (lifetime/memory safety): `HitmFighterRuntime`
  construction, `WorldTick` registration, move-construct, move-assign,
  multi-hop moves, the exact construct→move→execute→destroy and
  construct→move→move-again→execute→destroy sequences, container
  storage/reallocation, and destruction alongside a still-live sibling
  `World` — 11 tests. Separately, `PhysicsSystem::AsWorldSystem()`'s
  dormant identical-shaped hazard, audited and fixed (value-capture
  closure, no `this` dependency at all) rather than left dormant —
  destruction of the source object before any tick, move-then-slot-reuse,
  and two independent instances never aliasing each other's state — 3
  more tests. **14 lifetime tests total**, clean in both a normal build
  and 4 separate AddressSanitizer+UndefinedBehaviorSanitizer runs (run
  twice, once per fix) with leak and stack-use-after-return detection
  enabled. Zero findings, zero suppressions, no test weakened, no move
  semantics disabled anywhere. See "PROVEN WITH ASAN", "Exhaustive
  lifetime-safety verification", and "`PhysicsSystem::AsWorldSystem()`
  lifetime audit and fix" above.
- **IMPLEMENTED BUT UNVERIFIABLE** (GPU/audio/device behavior): nothing —
  deliberately. This module wrote zero rendering/audio/device code, so
  there is nothing in this category to report other than its continued
  absence.
- **NOT IMPLEMENTED** (remaining HITM systems): a second fighter/opponent
  and the read-engine's automatic gain/lose trigger detection; sprite/
  texture rendering; audio; real input-device polling; combo damage
  scaling; basic normals (not authored in real HITM data);
  `CombatController`/`MotionGraphEvaluator` integration. See "NOT
  IMPLEMENTED" above for the full list. (The dormant `PhysicsSystem`
  lifetime hazard formerly listed here has been closed — see above — and
  is not carried forward as an open item into whatever comes next.)

Module 5A is now formally closed: real HITM data drives real,
deterministic DOMINUS simulation; the mechanism that makes
`HitmFighterRuntime` move- and container-safe is verified, not assumed;
and the one dormant lifetime hazard this module's own audit surfaced
elsewhere in the engine (`PhysicsSystem::AsWorldSystem()`) has been fixed
and verified rather than carried forward into the next integration layer.
No known callback lifetime hazard remains open in the systems this module
touched.

## A deliberately scoped reopening (fifth continuation): `state_frame`

Module 5B's own "NOT IMPLEMENTED" accounting (see
`HITM_SPRITE_ASSET_REPORT.md`) later identified a real, specific gap
this module's own public surface left open: `HitmFighterSnapshot` only
exposed the match-wide monotonic `frame` counter, with no way for a
downstream consumer to know how many real frames had elapsed since
`state` itself last changed — the real engine's own `animT`/`stateT`
reset-on-transition convention, which Module 5B's animation-frame
selection needed and did not yet have. At that time this report
explicitly declined to add it, per the standing instruction not to
reopen this module without a genuine defect forcing it.

The user later gave explicit, specific authorization to close exactly
this gap: *"When you're ready to reopen 5A, the FrameState extension
should be a deliberately scoped change, not an excuse to reopen the
entire module ... Don't manufacture the missing 5%. Protect the 95%
you've now proven."* This section is that closure, held to that
standard.

**What changed** (see `HitmFighterRuntime.h`'s own "A DELIBERATELY
SCOPED EXTENSION" header comment for the complete account): one new
field, `state_frame` (public on `HitmFighterSnapshot`, private as
`FrameState::stateFrame`). It counts frames elapsed since `state` last
changed — 0 on the frame a transition happens (input-driven switch,
ground-clamp landing, or a countdown-driven sub-state advance, captured
via a single before/after comparison of `state` across the whole frame,
not a reset planted at each individual transition site), incrementing
every real frame after that, frozen — neither reset nor incremented —
during hitstop (the code that updates it sits after `RunOneFrame`'s
existing hitstop early-return, so it genuinely does not run that frame,
the same discipline `state_frames_remaining` already uses), and reset
unconditionally by `TakeHit()` even when the fighter is already in
`kHitstun` — a fresh hit is always a new hurt reaction, restarting from
its own frame 0, even mid-hitstun, exactly as a real fighting game's hit
reaction always does.

**What did NOT change**, by design: `Create()`, `AdvanceFrame()`,
`TakeHit()`, `GainRead()`/`LoseRead()`,
`ResolveOutgoingDamage()`/`ResolveOutgoingHitLanded()`, and every other
public signature — unchanged. No existing gameplay number, transition
rule, or timing value was touched. No landing-recovery timer, no
facing/opponent concept, and — critically — no bind-pose/forward-
kinematics data was manufactured to close the *other* gap this module's
own real-data audit found (`bones[].at`/`.part` missing from real
`parts.json`). That gap remains exactly as blocked as it has always
been: real upstream authoring data does not exist for it, so it stays
undone, not approximated.

**Verification**: 6 new, direct tests added to
`test_hitm_fighter_runtime.cpp` (this module's own test file, not just
the indirect coverage `HitmSpriteDrawData`'s downstream tests already
provided) — starts-at-zero, resets-on-transition-then-increments,
resets-again-on-a-second-transition, frozen-during-hitstop, resets-on-
`TakeHit()`-even-mid-hitstun, and an independently-re-derived reset/
increment invariant checked at every frame across a real attack's full
startup/active/recovery sub-state sequence. Full suite green with zero
regressions before any downstream code was even touched (837/837 minus
these 6 = 831/831 unchanged), confirming the addition is purely
additive. Three existing `HitmSpriteDrawData` tests needed updating for
the new, correct values this produces (documented in
`HITM_SPRITE_ASSET_REPORT.md`'s own "Track A gap #3 closed" section) —
fixed, not weakened. Clean under a second Debug+AddressSanitizer+
UndefinedBehaviorSanitizer build (2 full-suite runs, zero sanitizer
findings, checked via precise diagnostic-marker greps), and both live
`dominus-cli` demos (`hitm-fighter-runtime`, `hitm-sprite-draw-data`)
reproduce identical real gameplay numbers with the corrected, per-state
`raw_frame` values downstream. Fresh-clone verification performed before
push, per this session's standing discipline.

This reopening does not reopen Module 5A's formal closure above — it is
the one, explicitly-authorized, additive exception to it, and the module
remains closed to everything else.

## A second scoped reopening (sixth continuation): Phase 1 runtime foundation

`HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md` (a pure dependency-map
audit, zero implementation, committed separately) proposed a four-phase,
dependency-ordered sequence toward a real Brooklyn-vs-Rocket CPU match.
The user explicitly authorized only its Phase 1 — "the narrowly scoped
runtime extension for HP + facing + the read-engine requirement — not the
full match driver yet" — and asked for a stop-and-verify checkpoint
before any further phase. This section is that checkpoint.

Three real, additive changes, each held to the same "deliberately scoped
extension" discipline as the `state_frame` reopening above:

1. **The read engine is now optional.** `Create()` previously hard-failed
   any fighter with no `read_engine` in their real combat genome. Rocket
   and Static genuinely, permanently have none — real, verified data
   about them (confirmed again by direct read of both real
   `combat_genome.json` fixtures), not a gap — so failing on it was a
   DOMINUS implementation choice, not a reflection of missing authoring.
   `FrameState::readEngine` is now `std::optional<HitmReadEngineState>`;
   `GainRead()`/`LoseRead()` degrade to real, documented no-ops for a
   fighter with none; `ResolveOutgoingDamage()` degrades to a real 1.0x
   (no table to multiply by); `ReadEngineState()` now returns
   `const HitmReadEngineState*` (nullptr instead of a reference) and a new
   `HasReadEngine()` lets a caller ask explicitly. Brooklyn's own behavior
   is byte-identical — verified by the live CLI demo reproducing the exact
   same numbers as before this change.
   **This does NOT unblock Rocket/Static's own `Create()` call.** They
   still fail — now provably for only one reason: their real move schemas
   don't fit `HitmMoveInstance::Extract`'s current required-field set
   (Rocket's real "Ghost Dash" has no `blockstun`). A new test
   (`HitmFighterRuntime_Break_Rocket_StillBlockedByMoveSchemaNotReadEngine`)
   asserts the failure message names the move schema and does NOT mention
   `read_engine` — proving the scope of this change precisely, not just
   asserting "it still fails."
2. **Real per-fighter HP.** `max_hp = round(1000 * healthMult)` — `1000`
   is a real, hardcoded, uniform engine constant (hitm-engine's own
   `Fighter.js:23`, not authored per-fighter data); `healthMult` is real,
   per-fighter data already imported losslessly by Module 1
   (`HitmIdentityRecord::character_dna`'s real `frames.healthMult`) but
   never before extracted into a typed field — extracted locally in
   `HitmFighterRuntime.cpp` rather than reopening Module 2's
   `HitmCombatGenome` (per the audit's own "which modules should not be
   reopened" finding). Brooklyn: `round(1000*0.94) = 940` — confirmed by
   both a dedicated test and the live CLI demo. **`hp` starts at `max_hp`
   and Phase 1 does not reduce it** — `TakeHit` is completely unchanged;
   wiring real damage into `hp` and detecting KO is the audit's own
   separately-scoped Phase 2, deliberately not touched here.
3. **Real facing**, exposed via a new explicit seam, `SetFacing(int)` —
   the same "real, explicit, publicly-callable seam" discipline
   `GainRead()`/`LoseRead()` already use for real triggers this
   single-fighter runtime cannot detect on its own (no opponent exists to
   derive a real value from — the same reasoning `TakeHit`'s own
   `impactDirX=1.0f` default already documents). Defaults to `+1` at
   `Create()`. The one piece of real logic this runtime DOES enforce
   itself, because it needs no opponent to know: the real engine's own
   rule that facing never changes while a fighter is committed to any
   attack sub-state (`CombatSystem.js:488,509`) — `SetFacing()` is a real
   no-op, not a caller error, during
   kAttackStartup/kAttackActive/kAttackRecovery, proven by a test that
   calls it at each of the three sub-states and confirms it only takes
   effect once the fighter returns to `kIdle`.

**Tests**: 6 new/rewritten in `test_hitm_fighter_runtime.cpp` — the old
`Break_FighterWithoutReadEngine_Fails` (whose entire premise this change
deliberately reverses) rewritten to `NoReadEngine_CreateSucceedsWithRealNoOpDefaults`
asserting the new behavior; a new Rocket-specific test proving the
read-engine relaxation's exact scope (above); a deliberate-break test for
a `character_dna.json` missing `healthMult`; an HP-initializes-from-940
test; and two facing tests (default + explicit set, and the attack-lock
rule across all three real sub-states). `TOOLS/Editor/dominus_cli.cpp`
updated for the new pointer-returning `ReadEngineState()` and to print
`hp`/`facing`, so both live demos now double as real verification of every
new field.

**Verification**: full suite green with zero regressions confirmed at
every step (842/842, up from 836 baseline going into this work — 5 net
new tests, since one existing test was rewritten in place rather than
added alongside); clean Release rebuild (`rm -rf build`); clean
Debug+AddressSanitizer+UndefinedBehaviorSanitizer build, 2 full-suite
runs, zero sanitizer findings; both live `dominus-cli` demos
(`hitm-fighter-runtime`, `hitm-sprite-draw-data`) run under ASan too,
reproducing identical real gameplay numbers (`hp=940/940 facing=1`
appended, nothing else changed) — the sprite-draw-data demo's entire
output is byte-identical to before this change, confirming zero
cross-module impact. Fresh-clone verification performed before push.

This is a second, separate, additive exception to Module 5A's formal
closure — same discipline as `state_frame`, not a reopening of the
module's own scope. The audit's Phase 2 (real `_melee` hit-check, HP/
damage/hitstun/hitstop wiring, KO/round/timer via `HitmGameRules`) and
Phase 3 (the actual two-fighter match driver) remain explicitly
unimplemented, per the user's own "stop and verify" instruction — see
`HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md` for the full proposed
sequence this checkpoint is one step into.

## A third scoped reopening (seventh continuation): Phase 2 real combat

Phase 1's checkpoint verified, the user authorized Phase 2 ("real
combat") in full: steps 5-7 of `HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md`'s
proposed sequence — a real position-based hit-check, real HP/damage/
hitstun/hitstop wiring, and KO/rounds/timer via the already-imported
`HitmGameRules`. Two real findings surfaced along the way that changed
how "rounds/timer" and part of the damage formula were actually
implemented — both documented here rather than worked around silently.

**Step 5 — real hit detection.** `CHARACTER/HitmBridge/HitmMeleeHitCheck.{h,cpp}`
is a new, standalone, pure function (`MeleeHitConnects`), NOT a method on
`HitmFighterRuntime` — mirrors `HitmSpriteDrawData`'s own "lives outside
the runtime as a pure consumer" shape. A direct, line-by-line port of the
real engine's own `CombatSystem.js:370-374` (`_melee`): flat x/y
proximity using the attacker's real `range`/`height` and two real,
hardcoded, uniform engine constants (`Fighter.js:28`: `w=52`, and the
real `def.height || 105` fallback, including its JS falsy-zero quirk,
faithfully preserved the same way this track's secondary-motion work
already preserved an identical quirk). No `Skeleton`, no `Pose`, no bind
pose — confirms, from the real engine's own source rather than inference,
that `COMBAT::CollisionEvaluator` was never the right tool (see the
audit's own "what's missing and why" finding). 8 new tests: boundary-
exact arithmetic on every edge (just-inside/just-outside the real reach
window, both facings, the vertical gate, the real zero-height fallback),
plus two realistic-position scenarios using the real stage bounds
(`wallL`/`wallR`).

**Step 6 — real HP/damage/hitstun/hitstop.** `TakeHit()` now reduces real
`hp`: the incoming move's own real `power`, times the real block-chip
multiplier (`HitmGameRules::Combat().chip_mult`) when blocking — a direct
port of `CombatSystem.js:424,436`. `hp<=0` transitions the fighter to a
new `kKO` state (a direct port of the real engine's own `_ko()`,
`CombatSystem.js:458`): terminal, input-locked (added to the same
"locked" `RunOneFrame` case as the attack/stun states), and `TakeHit()`
now real-no-ops on an already-KO'd fighter (`CombatSystem.js:385`'s own
guard). A real, faithfully-preserved quirk, not softened: the real
engine checks `hp<=0` unconditionally right after the reduction, with no
exemption for a blocked hit — so a `blocking=true` `TakeHit()` call CAN
still KO via chip damage, proven by its own dedicated test.

**Three real pieces of the full real damage formula were found and
deliberately excluded, not silently defaulted to 1.0**, each documented
in `HitmFighterRuntime.h`'s own "PHASE 2" comment:
- The attacker's own read-engine multiplier — `ResolveOutgoingDamage()`
  already owns that law, and is an ATTACKER-side method this
  DEFENDER-focused `TakeHit()` has no access to; combining them needs a
  caller (a future match driver) that knows both fighters.
- Real combo damage scaling (`game.json`'s already-imported
  `combat.scaleMin`/`scaleStep`) — applying it needs a combo-hit counter
  this class does not track, the same gap Module 5B's own report already
  named.
- **A genuinely new finding this phase surfaced**: the real engine's
  `atk.power` multiplier (`CombatSystem.js:424`) comes from
  `character.json`'s `stats.power` — and `character.json` is itself
  self-labeled `"_generated": "genome_compiler.py"` in the real source
  tree, the SAME "generated, not authored" category this track has
  refused to import since its very first module (the basic-normals gap).
  `character_dna.json`'s own already-imported `frames.damageMult` is a
  different, genuinely authored per-fighter multiplier in the same
  spirit, but not the same value the real engine actually multiplies by
  (Brooklyn: `damageMult=1.03` vs. `character.json`'s compiled
  `power=1.01`) — substituting one for the other would be real data used
  dishonestly, not the real formula, so it was left out entirely.

A small, necessary side effect: adding `kKO` to `HitmFighterState` (a
type shared with Module 5B) required two of `HitmSpriteDrawData.cpp`'s
own exhaustive `switch` statements to handle it or fail to compile.
Resolved honestly, not by adding a placeholder case: a real `'ko'` clip
was confirmed present in all three real fighters' own `anim.json`
(matching the real engine's own `AnimationSystem.js` `clipFor()`
exactly), so `SelectClipName(kKO)` now returns it for real, with its own
test. This is the one place this phase touched Module 5B, and only to
keep it correct against the new enum value — not a reopening of its own
scope.

**Step 7 — rounds/timer: a real architectural finding, not implemented
on this class.** `HitmGameRules::Rounds()` already has the real
`to_win`/`timer_seconds` data imported and ready — but in the real
engine, rounds-won and match-timer live on the SHARED match state
(`CombatSystem.js`'s own `state.round`/`state.timer`/`state.phase`),
never on an individual `Fighter`. A single fighter genuinely has no
"rounds it has won" without a second fighter's outcome to compare
against — adding a per-fighter rounds counter here would manufacture an
incoherent concept the real architecture itself does not have, the same
category of mistake as fabricating missing authored data. This is real,
match-level state that belongs in the audit's own Phase 3 (the
two-fighter match driver, still unauthorized), not `HitmFighterRuntime`.
Nothing was added to this class for it; `HitmFighterRuntime.h`'s own
"PHASE 2" comment documents this explicitly so it reads as a deliberate
scoping decision, not a missed item.

**Tests**: 15 new — 8 in the new `test_hitm_melee_hit_check.cpp`, 6 in
`test_hitm_fighter_runtime.cpp` (real damage, real chip damage, repeated-
hits-to-KO, chip-damage-can-KO, no-op-after-KO, input-locked-during-KO),
1 in `test_hitm_sprite_draw_data.cpp` (the real `'ko'` clip selection).

**Verification**: full suite **857/857** (was 842/842); clean Release
rebuild (`rm -rf build`), zero errors, zero warnings (including the two
`HitmSpriteDrawData.cpp` switch statements the new `kKO` enumerator
required — both now exhaustive, no `-Wswitch` diagnostics); clean
Debug+AddressSanitizer+UndefinedBehaviorSanitizer build, 2 full-suite
runs, zero sanitizer findings; both live `dominus-cli` demos run under
ASan too — `hitm-fighter-runtime` now shows real `hp=878/940` after
Brooklyn's real special (940-62) with everything else byte-identical;
`hitm-sprite-draw-data`'s entire output is unchanged. Fresh-clone
verification performed before push.

This is a third, separate, additive exception to Module 5A's formal
closure. Phase 3 (the actual two-fighter match driver) remains
explicitly unimplemented, per this session's phase-by-phase
authorization discipline — see
`HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md` for the full proposed
sequence.

## A fourth scoped reopening (eighth continuation): Phase 3's two touch points on this class

Phase 3 (the actual two-fighter match driver, `CHARACTER/HitmBridge/HitmMatch.h` —
full account in `HITM_MATCH_REPORT.md`) needed exactly two small,
additive changes on this class, both already documented in its own
header comment ("PHASE 3") and summarized here for this report's own
continuity:

1. **`Create()` no longer hard-fails when a fighter's real special
   doesn't extract.** Was blocking Rocket from existing as a runtime
   fighter at all, for a reason unrelated to his real "Ghost Dash" itself
   (see the "PHASE 3" comment for the full reasoning). `FrameState::
   specialMove` is now `std::optional<HitmMoveInstance>`; new
   `SpecialMove()`/`HasSpecialMove()` let a caller query it.
2. **A new `ResetForNewRound(x, y, facing)`** — a direct port of the real
   engine's own `resetRound()`, deliberately preserving the real,
   evidenced fact that `meter` and read-engine reads persist across
   rounds (neither field appears in the real function's own body).

**Tests**: 4 new — a positive/negative pair for
`SpecialMove()`/`HasSpecialMove()` (Rocket now constructs with none;
Brooklyn's is unaffected), and a pair for `ResetForNewRound()` (restores
real hp/state/position/every countdown; explicitly proves meter/reads
survive the call unchanged).

**Verification**: full suite **860/860** at this class's own closure
(before Phase 3's own new `HitmMatch`/`HitmMeleeHitCheck` test files
added their own coverage on top — **869/869** overall, see
`HITM_MATCH_REPORT.md` for the complete Phase 3 verification account);
clean Release, AddressSanitizer+UndefinedBehaviorSanitizer (2 runs), all
three live `dominus-cli` demos (including the new `hitm-match`) under
ASan too. Fresh-clone verified before push.

This is a fourth, separate, additive exception to Module 5A's formal
closure — same discipline as every prior one.
