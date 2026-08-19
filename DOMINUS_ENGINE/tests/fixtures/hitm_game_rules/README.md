# hitm_game_rules fixtures — provenance

`game.json` is a byte-identical copy of the real, authored
`hitm-engine/data/system/game.json` (same archive/commit as
`../hitm_identity/README.md` and `../hitm_parts_rig/README.md`). Verified
byte-identical via `diff` at copy time. `data/system/cameras.json` and
`data/system/vfx.json` are deliberately not copied — out of scope for this
module, see `CHARACTER/HitmBridge/HitmGameRules.h`'s top comment.

`broken_*/` are deliberate-break fixtures for `HitmGameRules::Import`'s
negative tests, each the real file with exactly one mutation (or, for
`broken_missing_file/`, no file at all):

- `broken_missing_file/` — no `game.json` present.
- `broken_malformed_json/` — the real file truncated mid-object (invalid JSON syntax).
- `broken_missing_combat_section/` — the required `"combat"` object removed entirely.
- `broken_roster_wrong_type/` — `"roster"` is a string, not an array.
- `broken_empty_roster/` — `"roster"` is an empty array (a game with no fighters is not a real roster).
- `broken_roster_non_string_element/` — `"roster"` contains a number alongside real fighter-id strings.
- `broken_missing_gravity/` — `physics.gravity` removed.
- `broken_meter_max_wrong_type/` — `meter.max` is a string, not a number.
