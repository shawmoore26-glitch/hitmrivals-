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
