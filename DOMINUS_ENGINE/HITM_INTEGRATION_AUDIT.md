# DOMINUS ↔ HITM RIVALS — Integration Audit

Date: 2026-08-19
Scope: does DOMINUS_ENGINE, as it stands today, have what it needs to take real
HITM Rivals characters, art, animation, combat data, audio, environments, and
game rules and compile/assemble them into a playable HITM Rivals game?

Method: every claim below was checked against source, not against README
prose. Where a build/test command is quoted, it was actually run in this
session. Nothing here is inferred from documentation alone.

## 0. Verified baseline

```
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..   (DOMINUS_ENABLE_VULKAN=OFF, default)
$ ninja                                           # clean build, 0 errors, 0 warnings emitted
$ ./tests/dominus_core_tests
656/656 tests passed
```

The 656/656 figure in `ROADMAP.md` is real, current, and independently
reproduced in this session. This audit's job is to determine what those 656
tests actually prove — not to re-litigate that they pass.

**What 656/656 proves:** every `.dominus` load/compile/validate/simulate path
DOMINUS has built, on synthetic and Brooklyn-*named* fixtures under
`tests/fixtures/`, behaves correctly and fails loud on injected breakage.

**What 656/656 does NOT prove:** that any of those fixtures are the real,
authored HITM Rivals content. They are not. See §2.

## 1. The two codebases

| | DOMINUS_ENGINE | hitm-engine (existing HITM Rivals) |
|---|---|---|
| Language | C++20 | JavaScript (runtime) + Python (compiler pipeline) |
| Status | 656/656 tests, headless substrate, no window/audio/input | Working, playable, browser-buildable (`dist/index.html`) |
| Roster | test fixtures named after Brooklyn only | Brooklyn, Rocket, Static — full data + sprite atlases + transformation states |
| Art | none resolved anywhere (by design, see §3.3) | real sprite atlases (`assets/parts/*_atlas.png`), stage art (`assets/sprites/city.jpg`) |
| Rules | genome-driven per-move data, no global ruleset | `data/system/game.json` — gravity, walk/dash speed, meter, hitstop, round rules |
| Gates | `ctest` / `dominus_core_tests` | `tests/smoke.mjs`, `tests/function_test.mjs`, `genome_governor.py --strict`, `content_validate.py`, `kinetic_check.py`, `rig_validate.py` |

hitm-engine is the **product**. DOMINUS is the **substrate meant to eventually
build it**. They are not currently connected in any way — no code in either
tree references the other.

## 2. The genome-fidelity gap (most important finding)

DOMINUS's `CHARACTER/Genome/CombatIdentity.h` models a fighter's combat
identity as six flat enums:

```json
// DOMINUS_ENGINE/tests/fixtures/brooklyn_combat.json
{ "style": "psycho_drunken_martial_arts", "range": "close",
  "pressure": "relentless", "counter": "expert",
  "mobility": "unpredictable", "risk": "medium" }
```

The real, authored HITM Rivals Brooklyn (`hitm-engine/data/identity/brooklyn/combat_genome.json`,
extracted from the uploaded archive this session) is a 13-component document:
`rhythm_profile`, `weight_profile`, `risk_profile`, `defense_profile` (with a
`blockPreference` numeric cap and a cited `VOLUME 18` law), `pressure_profile`,
`range_profile`, `recovery_profile`, `impact_profile`, `ai_intent`,
`combat_verbs`, and a five-tier `read_engine` (Brooklyn's signature mechanic —
reads gained on counter-hit/whiff-punish/perfect-guard/throw-confirm, lost on
being read back, decaying after 420 frames, gating a `damage_mult` up to
1.42×).

**None of that is representable in DOMINUS's current `CombatIdentity` schema.**
The fixture reuses Brooklyn's name and archetype string but is a strawman,
not a lossy-but-faithful import. Every one of the 656 tests that exercises
"Brooklyn" is exercising this strawman, not the real character. This is not
a defect in the 656 — those tests are honestly scoped to what they claim.
It is the actual gap between "tests pass" and "HITM Rivals runs on this."

Same pattern holds for rig/art: HITM's real `data/characters/brooklyn/parts.json`
is a 2D sprite-atlas cutout format (`atlas` name, `sourceSize`, per-part
`pivot`/`normW`/`normH`/pixel `frame` rect, 29 bones per `rig.json`, generated
by `rig_compiler.py` from `character_dna` + `design` + `combat_genome`).
DOMINUS's `ANIMATION/SkeletonSystem/Skeleton.h` is a bone name/parent/
`Transform2D` hierarchy with **no concept of a sprite part, pivot, atlas
frame, or draw layer at all** — confirmed by grep, `atlas`/`sprite`/`texture`
appear only in `VISUALFORGE` (asset *specification*, still unresolved) and
`GRAPHICS/Vulkan` (gated off, see §3.3). There is currently no data
structure in DOMINUS that HITM's `parts.json` could even losslessly load
into.

## 3. Capability matrix

Legend: **HAVE** (real, tested, consumed) · **PARTIAL** (schema/struct exists,
not consumed, or consumed on strawman data) · **MISSING** (no code).

| HITM Rivals need | Status | Evidence |
|---|---|---|
| Character identity/DNA ingestion | PARTIAL | `CombatIdentity.h` real and tested, but schema is a strawman — see §2 |
| Rig / skeleton | PARTIAL | `Skeleton.h` bone hierarchy real and tested; no sprite-part/pivot/atlas concept exists |
| 2D cutout sprite rendering | MISSING | no `atlas`/`sprite`/`texture` type outside unconsumed `VISUALFORGE` specs |
| Animation clips / motion graph | HAVE | `ANIMATION/AnimationGraph` — state machine, blend transitions, layer stack; real, tested, drives Brooklyn's (strawman) combat states |
| IK | HAVE (bounded) | `ANIMATION/IK/TwoBoneIK` — two-bone analytic only, documented as such |
| Frame data / hitboxes / hurtboxes | HAVE | `COMBAT/HitSystem` — real collision evaluation against live pose data |
| Combos / cancels | HAVE | `COMBAT/ComboSystem/ComboEngine` — real move-data-driven legality |
| Hit reactions | HAVE | `COMBAT/ReactionSystem` — power → reaction → motion-graph trigger |
| Transformations | HAVE | `COMBAT/TransformationSystem` — atomic genome/graph/moves swap, Brooklyn `beast_mode` proven |
| Combat AI | HAVE | `AI/Agents/CombatAI` + `BehaviorTree`, genome-weighted move selection |
| Rigid body physics | HAVE (generic) | `PHYSICS/PhysicsSystem` — general rigid-body/collision, not fighting-game-specific (no gravity/walk/dash/jump tuning table) |
| Global game rules (gravity, walk speed, meter, hitstop, round timer) | MISSING | HITM's `data/system/game.json` has no DOMINUS equivalent; `GameDesignGenome` is a meta-design descriptor (arcade vs. soulslike), not a physics/meter constants table |
| Real-time window + present loop | MISSING | `CORE/Runtime/Application.h` is an empty headless tick loop by its own header comment ("Phase 1: empty tick. Systems attach here starting Phase 2") |
| Input | MISSING | no `INPUT/` directory, no input type anywhere |
| Audio | MISSING (schema-only) | `.dominus` schema has an `audio: [{event, ref}]` array; zero engine code reads it. `COMBAT/Profiles.h` has an `AudioProfile` struct explicitly documented as inert — "no physics/audio/render system exists in this engine" (`README.md`) |
| GPU / texture rendering | MISSING here | Vulkan path exists (`GRAPHICS/Vulkan/`) but is gated `OFF` by CMake default and **requires a real GPU device to verify** (project's own methodology: "real-device-verified", deliberate-break-tested against actual VUIDs). This sandbox has no Vulkan loader/ICD — confirmed (`pkg-config vulkan` not found, no `/usr/include/vulkan`). Any GPU work done in a session like this one cannot be honestly claimed PROVEN. |
| CPU raster fallback | PARTIAL (honest stub) | `GRAPHICS/Raster/RasterDevice.h` draws one flat-colored rectangle per `DrawCommand`, explicitly documented as *not* mesh/sprite rendering |
| Stage / environment | MISSING | `WORLD/Core` is a generic entity/spatial/tick kernel; no stage-asset loader, no equivalent of HITM's `data/stages/hitm_city` or `assets/sprites/city.jpg` |
| Camera | PARTIAL | `GRAPHICS/Renderer/Camera.h` exists (render-camera struct); no fighting-game camera behavior (HITM's `data/system/cameras.json`, `COMBAT/CinematicDirector`'s camera *cues* exist but nothing consumes `cameras.json`) |
| VFX | MISSING | `WorldEventSystem` emits VFX *tags* as part of `WorldEvent` descriptors; explicitly documented as "the seam into a future ... system, not an implementation of one" |
| Save / load | PARTIAL | `WORLD/Core/WorldPersistence` — real world-state save/load; not the same thing as HITM's player-facing `SaveSystem.js` (settings/progress), no overlap yet |
| Replay | MISSING | no equivalent of `engine/replay/ReplaySystem.js` |
| JSON ingestion of real authored text | **BUG** | `CORE/Serialization/MiniJson.h::ParseString` mis-decodes `\uXXXX` escapes — turns `—` into the literal text `u2014` instead of an em-dash, and mishandles `\b`/`\f`/`\r`, all silently (falls through to a `default: out += next` branch, no error). Real HITM identity JSON uses `—` extensively (`combat_genome.json`, `_law`/`_note` fields). This is not hypothetical: any current attempt to ingest real HITM text through the shared parser used by all 17+ existing `.dominus` loaders would silently corrupt it. |

## 4. Priority conclusion

The blocking dependency is not rendering, audio, or input — a fighting game's
*substance* (who Brooklyn is, what his combat genome says, what his rig looks
like) has to be representable and ingestible *before* anything downstream
(compiling, rendering, playing) can be honest rather than decorative. Per the
Constitution's own Law 6 ("Do not start WORLD before COMBAT is proven") and
the existing Sequencing Rule, the correct next step is not a new visible
system — it's fixing the two things that make every downstream step
currently dishonest if built on top of them:

1. **The shared JSON parser bug** (§3, last row) — every loader in the engine
   depends on it; ingesting real authored HITM text through it today would
   silently corrupt data. Fix first; it's infrastructure, not a feature.
2. **Real identity ingestion** (§2) — a path that takes the actual
   `hitm-engine/data/identity/<fighter>/*.json` files (not synthetic
   `tests/fixtures/*_combat.json` strawmen) and produces validated DOMINUS
   objects, with the full authored richness preserved losslessly, not
   flattened into the current six-enum `CombatIdentity`.

Everything else in §3's MISSING column (rendering, audio, input, stage,
global game rules) is real, sequenced work, but building any of it against
the current strawman Brooklyn would mean re-doing it once real data is
ingestable. See `ROADMAP.md`'s new **HITM INTEGRATION** phase section for the
full sequenced plan.
