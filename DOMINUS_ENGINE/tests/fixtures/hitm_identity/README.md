# hitm_identity fixtures — provenance

`brooklyn/`, `rocket/`, `static/` are byte-identical copies of the real,
authored `hitm-engine/data/identity/<fighter>/{character_dna,combat_genome,
design,identity,signature}.json` files from the HITM Rivals engine archive
(`HITM_RIVALS_GITHUB_REPO.tar.gz`, extracted from the user-provided
`files (11).zip` upload, commit `babe381`). Verified byte-identical via
`diff` at copy time — nothing here was retyped, reformatted, or
paraphrased. `motion_bible.json` and `hit_feel_profile.json` are
deliberately not copied — `HitmIdentityImporter` has no consumer for them
yet (see its header comment).

`broken_*/` directories are synthetic, deliberate-break fixtures built by
taking a real fighter's files and damaging exactly one thing each, for
`HitmIdentityImporter`'s negative tests:

- `broken_missing_file/` — Brooklyn's files minus `combat_genome.json` entirely.
- `broken_malformed_json/` — Brooklyn's files with `identity.json` truncated
  mid-object (invalid JSON syntax).
- `broken_missing_key/` — Brooklyn's `identity.json` with the required
  `"sprite"` key removed (rest of the file otherwise real).
- `broken_id_mismatch/` — Brooklyn's files with Rocket's real `identity.json`
  substituted, so `id: "rocket"` sits in a directory the importer will see
  as `broken_id_mismatch` — cross-check must reject this, not import Rocket
  under the wrong fighter id.

## Module 2 (`HitmCombatGenome`) deliberate-break fixtures

`broken_genome_*/` are all Brooklyn's real files (`identity.json`'s `id`
field retargeted to match each directory's own name, so Module 1's import
succeeds and the malformed shape below it is what `HitmCombatGenome::
FromRecord` must actually catch) with exactly one structural mutation each
in `combat_genome.json`:

- `broken_genome_archetype_wrong_type/` — `"archetype"` is a number, not a string.
- `broken_genome_ai_intent_not_array/` — `"ai_intent"` is a string, not an array.
- `broken_genome_block_preference_wrong_type/` — `defense_profile.blockPreference` is a string, not a number.
- `broken_genome_read_engine_missing_tiers/` — `read_engine.tiers` removed entirely.
- `broken_genome_read_engine_tier_missing_field/` — one `read_engine.tiers[]` entry is missing `damage_mult`.
- `broken_genome_read_engine_bad_decay/` — `read_engine.decay.frames` removed.
