# HITM Asset Coverage / Gap Report — Brooklyn, Rocket, Static

Date: 2026-08-20
Scope: what real HITM visual/animation content actually exists, in what
form, at what layer, for each of the three real fighters — and what
doesn't exist yet. Written after an audit of the Rocket and Static
character reference sheets against the real, committed atlas textures,
`parts.json`, `rig.json`, `anim.json`, `design.json`, and identity data
(see chat history for the crop-by-crop visual comparison this report is
built from).

**Governing rule for everything below and everything after it**:
DOMINUS's job is to faithfully consume and execute authored HITM
content, not to create missing HITM content. Nothing in this report, and
nothing produced by Track H Module 5B, invents, reconstructs, redraws,
or infers a texture, pose, or clip that isn't already present in the
real, committed HITM data. Where art doesn't exist, this report says so
and stops there.

## ⚠️ The Rocket and Static reference sheets are DESIGN INTENT — NOT RUNTIME AUTHORED

The two character-reference sheets reviewed (turnarounds, expression
grids, hand-pose grids, prop details) are **not** present anywhere in
the real, committed game data pipeline — not in `design.json` (the
AUTHORED SOURCE), not in `parts.json`, not in the atlas texture, not in
`anim.json`. They are treated here purely as evidence of design intent.
**No code, asset, or data file in this repository was created, modified,
or informed by copying content from those sheets.** Everything in the
"PROVEN" columns below traces only to files already committed under
`tests/fixtures/hitm_sprite_assets/` and `tests/fixtures/hitm_identity/`.

## Coverage matrix

| | **Brooklyn** | **Rocket** | **Static** |
|---|---|---|---|
| **Source-art asset** | Real, committed `brooklyn_atlas.png` (1024×269px, real PNG header verified) | Real, committed `rocket_atlas.png` (1024×326px) | Real, committed `static_atlas.png` (1024×313px) |
| **Authored metadata** | Real `design.json` — 22 parts across `core_parts`+`features` | Real `design.json` — 19 parts. Its own `_note` records a real, prior gap: *"PHASE 7 RECONSTRUCTION: rig.json shipped 6 parts while parts.json and the atlas carried 12. All six leg/foot entries restored from existing assets."* (already fixed once, before this session) | Real `design.json` — 19 parts. Antenna pivot has a documented, tool-corrected value: *"authored pivot landed on transparent pixels; snapped to nearest opaque. Measured, not estimated."* |
| **Runtime representation** | `HitmPartsRig`/`HitmRigPlacement` import real 22-part `parts.json`+`rig.json` — **PROVEN** (`HitmPartsRig_ImportsRealBrooklyn`, `HitmRigPlacement_*`) | Real 19-part `parts.json`+`rig.json` import — **PROVEN** (`HitmPartsRig_ImportsRealRocket_*`, `HitmRigPlacement_ImportsRealRocketAndStatic`) | Real 19-part `parts.json`+`rig.json` import — **PROVEN** |
| **Animation availability** | Real `anim.json`, 26 clips (idle/walk/dash/jump/block/hurt/down/ko/light1-3/heavy/smash/air/special/specialF/blockbuster/intro/victory/maniac/throw/throwF) — **PROVEN** import (`HitmAnimationSet_ImportsRealBrooklyn`) | Real `anim.json`, 23 clips, same coverage minus `maniac` — **PROVEN** import (`HitmAnimationSet_RocketHasRealSpecialClipDespiteIncompleteCombatData`) | Real `anim.json`, 23 clips, same shape as Rocket — **PROVEN** import (`HitmAnimationSet_ImportsRealStatic`) |
| **Expression variants** | **NONE.** One fixed `head` texture (manic grin baked in). Partial exception: a separately-cutout `jaw` bone (`design.json`: *"the grin is the character; it must be able to open"*) can rotate open/closed via the real secondary-motion spring system — a real, working **articulation**, not a texture swap; the grin drawn on the jaw piece is itself fixed. | **NONE.** One fixed `head` texture (neutral). No articulated jaw/mouth part at all — strictly more limited than Brooklyn's. | **NONE.** One fixed `head` texture (grin baked in). No articulated jaw/mouth part. |
| **Hand-pose variants** | **NONE.** One fixed `handFar` + one fixed `handNear` texture each (near hand shows a fixed gripped-object pose). | **NONE.** One fixed `handFar` (fused with the held lantern) + one fixed `handNear` (gripping fist). | **NONE.** One fixed `handFar` (fist) + one fixed `handNear` (fist with an "X" mark on the glove). |
| **Props** | `hat`, `tie`, `chain`, `coatFar`/`coatNear`, `dreadFar`/`dreadNear` — all real, named parts with real secondary-motion spring params, all imported and cross-validated. | `collarChain` (spiked collar), `bandana` (w/ lightning-bolt motif, drawn with mouth open baked in), lantern — **fused into `armFarL`/`handFar`, not a separate independent part**. | `wireFar`/`wireNear`, `coatStripFar`/`coatStripNear`, `antenna`, walkie-talkie/pager gadget — **baked into `torso`, not a separate part**. |
| **Missing authoring** | Bone-relative bind-pose data (`bones[].at`/`.part`) needed for full forward-kinematics rendering — a real, upstream gap in hitm-engine's own checked-in data (see `HITM_SPRITE_ASSET_REPORT.md`), not fighter-specific. | Same upstream FK gap. Additionally: the real move-schema gap Module 5A already found — his real "special" ("Ghost Dash") has no `blockstun`/`range`/`height`, so `HitmMoveInstance::Extract` refuses it. Expression/hand-pose art per the sheets above does not exist in authored data. | Same upstream FK gap. Real move-schema gap: his real "special" ("Live Wire") has no `blockstun`/`hitstop`. Expression/hand-pose art per the sheets above does not exist in authored data. |
| **Runtime-proven?** | **Yes, fully.** Real `HitmFighterRuntime` (Module 5A) + real `HitmAssetImporter`/`BuildSpriteDrawData` (Module 5B) both succeed end to end — live-verified via `dominus-cli hitm-sprite-draw-data`, exact real elapsed-frame values reproduced at every attack sub-state boundary. | **Asset layer only.** `HitmAssetImporter::Import` succeeds (atlas/parts/anim/rig all real, valid, cross-checked) — but `HitmFighterRuntime::Create`/`HitmMoveInstance::Extract` fail on his real, incomplete move data (Module 5A's own pre-existing finding), confirmed live: `dominus-cli hitm-sprite-draw-data` on Rocket fails at move extraction, never at asset import. No full gameplay-driven draw-data sequence has been run for him. | **Asset layer only.** Same shape as Rocket — asset import proven, full runtime gameplay sequence not provable until his own move-schema gap is separately addressed (out of this report's scope). |

## What this report does NOT claim

- It does not claim Rocket's or Static's expression/hand-pose art
  exists anywhere in this repository. It doesn't.
- It does not claim `HitmFighterRuntime` works for Rocket or Static —
  Module 5A's own real finding (incomplete move schemas) still stands,
  unchanged, unaddressed here.
- It does not claim the full bone-hierarchy forward-kinematics gap is
  fighter-specific — it's a property of hitm-engine's own real,
  checked-in `parts.json` schema, identical for all three fighters (see
  `HITM_SPRITE_ASSET_REPORT.md`'s "A real architectural finding").

## Two separate future tracks, deliberately not mixed

**Track A — DOMINUS Asset Pipeline** (what Module 5B already builds, and
the only track this session continues): use what actually, really
exists — `atlas → parts → rig → animation → runtime → draw data` — with
zero invention. This is a consumption/execution pipeline. It stays
strictly downstream of whatever HITM's own authors and generators
produce.

**Track B — HITM Asset Authoring** (explicitly not undertaken in this
session, or by this report): if the reference-sheet expressions and hand
poses are wanted in-game later, that is new authoring work in
hitm-engine's own pipeline — `reference sheet → author new parts.json
core_parts/features entries → re-slice the atlas (tools/slice_rig.py) →
update design.json → rig/animation integration → validate` — a
content-creation effort with its own real, separate scope. DOMINUS does
not perform this; it would only ever consume the result once it exists.

Keeping these separate is the point: DOMINUS's job is to faithfully
consume and execute authored HITM content, never to become responsible
for producing the HITM content that doesn't exist yet. This report exists
so that boundary is explicit and evidenced, not assumed.
