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
`state_frame` closure above (see that section, and
`HITM_SPRITE_ASSET_REPORT.md`, for everything added by Module 5B and the
Track A gap closures in between). 43 new tests at this module's own
closure: 5 in `test_hitm_move_instance.cpp`, 7 in
`test_hitm_read_engine_state.cpp`, 28 in `test_hitm_fighter_runtime.cpp`
(17 covering the vertical slice's gameplay behavior, 11
`LifetimeSafety_*` tests added across two continuations covering every
relocation path requested), and 3 in `test_physics_system.cpp`
(`PhysicsSystem_AsWorldSystem_*`, added in the fourth continuation to
close the dormant lifetime hazard); plus 6 more in
`test_hitm_fighter_runtime.cpp` in the fifth continuation
(`state_frame`, see above) — 34 in that file total. Full clean rebuilds
+ repeat runs across all five continuations (Release: 15+ repeats;
ASan+UBSan: 4 repeats of the full suite plus the live CLI demo for the
first four continuations, run once more for the fifth), all green — no
flakes observed anywhere. (The one segfault encountered in the first
lifetime continuation was deterministic — it reproduced on every run
before the fix, and has not recurred once, under any build
configuration, since — so it is reported as a found-and-fixed bug, not
logged as flakiness. The `PhysicsSystem` hazard never actually crashed
in this codebase, since no call site exploited it — it was found by
audit and closed pre-emptively, not by chasing an observed failure.)

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
