# hitm_parts_rig fixtures — provenance

`brooklyn/parts.json`, `rocket/parts.json`, `static/parts.json` are
byte-identical copies of the real, **generated** `hitm-engine/data/characters/
<fighter>/parts.json` files (compiled by that project's `tools/slice_rig.py`
from the real sprite sheet plus the authored `design.json` — see
`CHARACTER/HitmBridge/HitmPartsRig.h`'s provenance note for why this is
legitimate to read but is not itself the authored source). Same archive and
commit as `../hitm_identity/README.md`. Verified byte-identical via `diff`
at copy time.

`broken_*/` are deliberate-break fixtures for `HitmPartsRig::Import`'s
negative tests, each a copy of Brooklyn's real file with exactly one
mutation (or, for `broken_missing_file/`, no file at all):

- `broken_missing_file/` — empty directory, no `parts.json`.
- `broken_malformed_json/` — Brooklyn's file truncated mid-object (invalid JSON syntax).
- `broken_atlas_mismatch/` — Brooklyn's real file (`atlas: "brooklyn_atlas"`) placed under a directory named `broken_atlas_mismatch`, so the real, evidenced `"<dir>_atlas"` convention is violated.
- `broken_missing_handbone/` — the required `"handBone"` key removed entirely.
- `broken_dangling_handbone/` — `"handBone"` renamed to a name no bone in `"bones"` actually has.
- `broken_draworder_unknown_part/` — an extra `"drawOrder"` entry naming a part that doesn't exist in `"parts"`.
- `broken_bone_dangling_parent/` — an extra bone entry whose `"parent"` names a bone that doesn't exist.
- `broken_part_missing_frame/` — the `"torso"` part's required `"frame"` key removed.
