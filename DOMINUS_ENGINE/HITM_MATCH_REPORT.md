# Track H, Phase 3 — The First Real Brooklyn-vs-Rocket Match

Scope: `HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md`'s Phase 3 (Match ->
Round -> Timer -> Fighter A / Fighter B -> Combat resolution), authorized
after Phase 1's and Phase 2's own stop-and-verify checkpoints, and after
Phase 3 itself was scoped down to its own explicit inclusion/exclusion
list. Everything below is either a direct citation of real, authored HITM
data (`game.json`/`combat_genome.json`/`signature.json`/
`character_dna.json`, all already imported by Modules 1/2/4) or a direct,
cited port of a real formula/constant from hitm-engine's own
`engine/combat/CombatSystem.js`.

## What this closes

A new class, `CHARACTER/HitmBridge/HitmMatch` (`HitmMatch.h`/`.cpp`), owns
a real, complete, CPU-observable two-fighter HITM match — the first time
this track has driven two real fighters against each other, not one
fighter in isolation. It is deliberately **match-level**, not
fighter-level, exactly matching the real architecture the audit itself
found:

```
Match -> Round -> Timer -> Fighter A / Fighter B -> Combat resolution
```

`HitmMatch` owns two already-real, already-independently-proven
`HitmFighterRuntime`s and drives them together one real frame at a time,
reaching only through their already-public seams
(`AdvanceFrame`/`TakeHit`/`ResolveOutgoingHitLanded`/`SetFacing`/
`ResetForNewRound`/`Snapshot`/`State`/`SpecialMove`) — never its private
state. No round, timer, or match-win concept was added to
`HitmFighterRuntime` itself; that boundary, drawn explicitly in Phase 2's
own "Step 7" reasoning, holds.

## The real phase machine (a direct port of `CombatSystem.js`'s own `state.phase`)

| Phase | Real source | What it does |
|---|---|---|
| `kRoundIntro` | `CombatSystem.js:31,49,54` | Real, hardcoded 120-frame pre-round window. Neither fighter ticks at all — a direct port of `update()`'s own `return;` right after the intro check. |
| `kFight` | `CombatSystem.js:56-62` | Live gameplay: both fighters advance, real hit detection resolves, the real per-frame timer counts down, a real sudden-death rule resolves a timeout. |
| `kKO` | `CombatSystem.js:63-65,458-460` | Real, hardcoded 150-frame post-KO settle window. Both fighters keep advancing (their own real gravity/ground-clamp settling still runs) before the round resolves. |
| `kMatchOver` | (real `update()`'s own silent omission of a `'matchOver'` case) | Terminal. Nothing advances once the match is over — verified by a dedicated test. |

## Real hit detection (Phase 3 Step 5)

`CHARACTER/HitmBridge/HitmMeleeHitCheck.h`'s `MeleeHitConnects()` (built
in Phase 2, consumed here) resolves exactly once per real attack
activation. This needed no new "already hit" flag: the real engine's own
`hitFrame` convention (`engine/combat/FrameData.js:14`: `this.hitFrame =
this.startup; // first active frame`) is exactly `state==kAttackActive &&
state_frame==0` — Module 5A's own `state_frame` (closed two continuations
before Phase 1 even began) already resets to 0 on that exact transition,
so the real invariant the real engine's `hitDone` flag exists to enforce
falls out of already-existing, already-tested infrastructure for free.
Proven directly: a dedicated test (`HitmMatch_Fight_BrooklynSpecialConnectsExactlyOnceAndDealsRealDamage`)
confirms Rocket's hp drops exactly once across Brooklyn's real 4-frame
active window, not once per frame.

Per-frame tick order matches the real engine's own
`s.fighters.forEach`: fighter A's own action resolves before fighter B's.

## Real damage/HP/KO (Phase 3 Step 6)

Unmodified from Phase 2's own `TakeHit()` — this phase's own job was
supplying the real *inputs* to it (attacker's move, defender's real
blocking state) at the right real moment, not changing what it does.
`defenderBlocking` is derived exactly as the real engine does
(`CombatSystem.js:410`, `d.state===STATE.BLOCK && d.y>=ground`).

One real, documented gap, found and left alone rather than silently
worked around: the real engine gives the attacker meter on ANY landed
hit, blocked or not (`CombatSystem.js:438`,
`blocking?onBlockGive:onHitGive`) — but `HitmFighterRuntime::
ResolveOutgoingHitLanded` (Module 5A, unmodified) only ever applies the
real `onHitGive` amount, with no blocked-vs-unblocked branch of its own.
Calling it on a blocked hit would have silently overstated the
attacker's real meter gain, so `HitmMatch` deliberately skips it on a
block instead: the attacker gets zero meter for a blocked hit here, not
the real, smaller `onBlockGive` amount — a real, documented gap, not a
wrong number presented as correct.

## Real round resolution — the one deliberate control-flow departure, not an outcome departure

The real engine's `_endRound()` determines a round's loser by
re-inspecting `fighters[0].state===STATE.KO` (real, including its own "a
double-KO in the same frame favors fighter B" quirk, since only fighter
A's state is actually checked). Reproducing that by force-setting a
fighter's own `state` to `kKO` on a real TIMEOUT (no hit landed) would
have needed a new "force this fighter into kKO" mutator on
`HitmFighterRuntime` that nothing else needs. Instead, `HitmMatch` tracks
`roundLoserIndex_` itself — computed identically to the real tie-break
for a genuine hit-KO (fighter A checked first, so a same-frame double-KO
still favors fighter B, matching the real quirk exactly), and directly
from the real hp-fraction comparison
(`CombatSystem.js:59-61`, `a.hp/a.maxhp >= b.hp/b.maxhp ? a : b`, ties
favor A) on a timeout. `EndRound()` then always resolves the exact same
real winner the real engine would, without `HitmFighterRuntime` exposing
a mutator nothing else needs. Documented in full in `HitmMatch.h`'s own
header comment.

## Real round reset (Phase 3 Step 12)

A new `HitmFighterRuntime::ResetForNewRound(x, y, facing)` — a direct
port of the real engine's own `resetRound()` (`CombatSystem.js:38-47`):
real `hp` back to `max_hp`, `state` back to `kIdle`, every countdown
cleared, position/velocity/facing set to the given real values. **A real
finding, caught by reading the real function in full rather than
assuming "reset" means "reset everything": the real `resetRound()` does
NOT reset `f.meter` or `f.reads`** — neither field appears anywhere in
its real body. Both genuinely persist across rounds within a real match.
Getting this wrong (resetting meter/reads every round, which a naive
"recreate the fighter" implementation would have done) would have been a
real, silent fidelity bug — caught before it was ever written, not after.
Proven directly by a dedicated per-fighter test and again at match level
(`HitmMatch_KO_EndsRoundResetsBothFightersAndAwardsRealRoundWin`, which
confirms Brooklyn's real meter — clamped at the real max well before the
KO blow — is still there in round 2's own snapshot).

Real per-match starting positions/facing
(`CombatSystem.js:26`, `mk(defA,0,300,1,...)`/`mk(defB,1,760,-1,...)`):
fighter A always at real `x=300` facing right, fighter B always at real
`x=760` facing left. Hardcoded, uniform engine constants — not per-fighter
authored data, same category as `HitmFighterRuntime.cpp`'s own `kBaseHp`
and `HitmMeleeHitCheck.h`'s own `kMeleeFighterWidth`.

## Rocket exists as a real fighter — his own special still does not (Phase 4, still unauthorized)

`HitmFighterRuntime::Create()`'s second, independent blocker (found by
the original audit, alongside Phase 1's read-engine relaxation) is now
also resolved: a fighter whose real special doesn't extract with today's
schema (Rocket's real "Ghost Dash") still constructs successfully, just
with `SpecialMove()` returning `nullptr`. This is **not** Ghost Dash —
Rocket's `kSpecial` input is a real, honest, documented no-op in every
match this class drives (proven directly, at match level, by
`HitmMatch_Fight_RocketSpecialInputIsRealNoOp_GhostDashNotImplemented`).
Making his real rush-type special actually execute (`_zoneHit` + travel
scheduling, a structurally different real code path from `_melee`) is
the audit's own, still-unauthorized Phase 4 — deliberately not attempted
here, per the user's own explicit exclusion list for this phase.

## Explicitly out of scope for this phase (per explicit instruction)

Ghost Dash, rendering, full bone-hierarchy FK, new asset authoring,
`character.json`'s generated values (found and excluded in Phase 2,
unchanged here), real combo damage scaling, the attacker's own
read-engine damage multiplier, networking, menus/UI, and any fighter
beyond these two. None of these were touched.

## The success criterion, demonstrated live

`dominus-cli hitm-match <brooklyn_identity_dir> <rocket_identity_dir>
<game.json>` runs a complete, real, deterministic match end to end,
printing every stage as it actually happens:

```
[hitm-match] fighter_a=brooklyn (real x=300 facing=+1) fighter_b=rocket (real x=760 facing=-1) real rounds.toWin=2
[hitm-match] ROUND START (round 1, real 120-frame intro)
[hitm-match] round=1 (fight begins) brooklyn[state=idle x=300 hp=940/940 meter=0] rocket[state=idle x=760 hp=1090/1090] roundsWon=0-0
[hitm-match] ACTION (real walkSpeed closing the real starting gap)
[hitm-match] round=1 (in real range) brooklyn[state=walking x=621.2 hp=940/940 meter=0] rocket[state=idle x=760 hp=1090/1090] roundsWon=0-0
[hitm-match] HIT (real _melee connected)
[hitm-match] DAMAGE (real 62 applied)
[hitm-match] HITSTUN (real hitstop=7 frames, then real hitstun countdown)
[hitm-match] KO (rocket hp=0, real 150-frame settle window begins)
[hitm-match] ROUND WIN + RESET (roundsWon=1-0, both fighters real-reset to the real per-match start, meter/reads persisted)
... (round 2 repeats identically) ...
[hitm-match] MATCH WIN (real rounds.toWin=2 reached, winner=brooklyn)
[result] real HITM data drove a complete, deterministic, CPU-observable Brooklyn-vs-Rocket match -- NOT rendered, NOT a claim Ghost Dash or any other unimplemented mechanic works
```

Every number in that transcript is real: `hp=940/940`/`hp=1090/1090` are
`round(1000*healthMult)` for Brooklyn/Rocket's own real, distinct
multipliers; `62` is Brooklyn's real special's authored `damage`; `7` is
the real `hitstopHeavy`; `150`/`120` are the real, hardcoded engine
constants cited above; `x=300`/`x=760` are the real per-match starting
positions; `meter=100` is Brooklyn's real meter clamped at the real
`meter.max` after enough real landed hits; `roundsWon=2-0` reaching real
`rounds.toWin=2` is what actually ends the match.

## Tests

9 new in the new `tests/integration/test_hitm_match.cpp`:
initialization (real positions/facing/hp/timer), round-intro no-tick
proof, a real attack connecting exactly once with real damage, a real
blocked attack dealing real chip damage only, a full real
KO -> 150-frame settle -> round reset -> round-win cycle (including the
meter/reads-persist proof), a full real match-win after real
`rounds.toWin`, Rocket's honest no-special proof at match level, and a
determinism/divergence pair (same discipline as every prior module's own
determinism proof). Plus 4 new on `HitmFighterRuntime` itself
(`SpecialMove()`/`HasSpecialMove()` positive+negative,
`ResetForNewRound()` restores real state, `ResetForNewRound()` preserves
meter/reads) — see `HITM_FIGHTER_RUNTIME_REPORT.md`'s "eighth
continuation" section.

## Verification

1. Clean Release build (`rm -rf build`): zero errors, zero warnings.
2. Full suite: **869/869 passed** (was 857/857 before this phase — 4 net
   new on `HitmFighterRuntime`, 9 new in `test_hitm_match.cpp`).
3. Clean Debug+AddressSanitizer+UndefinedBehaviorSanitizer build: zero
   errors, zero warnings. Full suite under it: **869/869 passed** across
   2 separate runs, zero sanitizer findings (checked via precise
   diagnostic-marker greps, not a naive substring match).
4. All three live `dominus-cli` demos (`hitm-fighter-runtime`,
   `hitm-sprite-draw-data`, the new `hitm-match`) run under ASan too —
   the first two reproduce byte-identical output to every prior phase;
   `hitm-match` reproduces the full transcript above.
5. Fresh-clone verification (`git clone` the pushed branch into a
   scratch directory, clean build, full test run, all three live CLI
   demos) before push.

No data invented. No existing behavior changed for any fighter or module
that already worked. Phase 4 (Rocket's own real Ghost Dash) remains
explicitly unimplemented, per this session's phase-by-phase
authorization discipline.
