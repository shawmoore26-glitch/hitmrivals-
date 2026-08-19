# REGISTRY

**Status: scoped prototype -- `CombatGenome` fully wired
(`GenomeCompiler`/`ImmutableArtifact`/`GenomeRegistry`/`RuntimeSnapshot`);
`CreatureGenome` hash pipeline only (`CanonicalSerializer`/
`CreatureGenomeCompiler`, no artifact/registry/snapshot layer yet).**

Not a phase in the Phase 4.x World Engine sequence, and not (yet) a
commitment for the rest of the engine. A bounded proof-of-concept for one
piece of a much larger proposed architecture (deterministic compilation,
content-addressed immutable artifacts, a registry, version lineage,
runtime snapshots isolated from authored data). Proved on one genome
type before any decision to generalize -- see `../ROADMAP.md`'s
"Registry Prototype" section for the full writeup and the explicit
decision NOT to generalize this without further justification.

Depends on `CHARACTER/Genome` only -- never `COMBAT`/`ANIMATION`/
`WORLD`/`PHYSICS`, verified by grep before any test was written, same
discipline as every other module boundary in this engine.

- `Hash/Sha256.h` -- hand-rolled SHA-256, verified against digests
  independently computed via Python's `hashlib` (both standard published
  test vectors plus a multi-64-byte-block input to exercise the
  chunking/padding path). A hash function that's silently wrong is worse
  than none -- correctness was checked, not assumed.
- `CanonicalSerializer.h` -- deterministic, source-formatting-independent
  bytes from a `CombatIdentity`. Also `SerializeCreatureGenome` --
  extends the same discipline to a genome with ~4x the field count, real
  evidence the pattern generalizes.
- `GenomeCompiler.h` -- Validator -> Canonical Serializer -> Hash ->
  `ImmutableArtifact`, in that order.
- `CreatureGenomeCompiler.h` -- Validator -> Canonical Serializer -> Hash
  only, for `CreatureGenome`. Deliberately does NOT produce an
  `ImmutableArtifact` -- that type bakes in `CombatGenome`-specific
  `DecisionWeights`, and no `CreatureGenome` decoder exists yet to
  produce an equivalent. Returning hash + canonical bytes rather than an
  artifact with fake baked data was the honest choice -- see
  `../ROADMAP.md` MONSTERFORGE Phase 1.
- `ImmutableArtifact.h` -- every member private, const getters only, zero
  mutation API on the type. "Compiled assets never change" as a
  structural fact, not a comment.
- `GenomeRegistry.h` -- content-addressed (same content always hashes to
  the same key, so overwriting is structurally impossible), with
  per-entity ordered version lineage.
- `RuntimeSnapshot.h` -- `SnapshotBuilder::Build()` has exactly one
  overload, accepting only an `ImmutableArtifact`. There is no code path
  that builds a snapshot without going through a compiled, hashed
  artifact first.

All five claims from the source directive (deterministic compilation,
stable/content-addressed hashing, registry lookup, version lineage,
runtime never touching authored data) are independently proven against
real Brooklyn combat genome data in `tests/registry/`. Live via
`dominus-cli genome-compile`. `CreatureGenome`'s narrower claim
(deterministic compilation + stable hashing only) is proven in
`tests/genome/test_creature_genome.cpp` and live via
`dominus-cli creature`.

**What this does NOT mean:** the rest of the engine is completely
unchanged. Every other genome-like data (Motion/Physics/Behavior/Audio/
Visual/Camera profiles) still loads directly via `DominusSerializer` with
no hashing, no immutability, no registry. `CreatureGenome` proved the
CanonicalSerializer/hash layer generalizes; it did NOT extend
`ImmutableArtifact`/`GenomeRegistry`/`RuntimeSnapshot` to a second type
-- that structural generalization remains a real, separate, still-open
decision.
