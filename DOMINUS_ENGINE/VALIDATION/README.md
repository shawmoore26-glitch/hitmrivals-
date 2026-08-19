# VALIDATION

**Status: Phase 7 -- `PackageValidator` built. The Dominus Pipeline --
`BuildPipeline` built.**

`PackageValidator` -- the terminal consumer of the engine's dependency
graph: depends on `CORE`/`ANIMATION`/`CHARACTER`/`COMBAT`, nothing
depends on it. Four checks, run against a `.dominus` file + its base
directory:

- `CheckIdentityCompleteness` -- an `IdentityComponent` exists with a
  non-empty `display_name`; combat identity (if present) has a non-empty
  `style`; moves without a combat genome behind them are an error
  (LAW C002).
- `CheckAssetOwnership` -- every ref path across every CORE ref
  component actually exists on disk, checked before anything tries to
  load it.
- `CheckNamingConsistency` -- `object_id` is snake_case (the schema's own
  documented convention, previously unenforced); move/animation-clip
  ref-list keys match their file's own internal `name` field (a
  genuinely new catch -- nothing else in the engine verified this).
- `CheckDeterministicRebuild` -- loading the same file twice produces
  identical results; `dominus_version` is valid semver.

`ValidateFile` composes all four into one report. `dominus-cli
validate-package` runs it.

**Found a real bug on first use**, not a staged one: running this
against Brooklyn's actual shipping fixture found a dead `mesh` reference
carried since Phase 1 (parsed into a component, never consumed, never
checked). Fixed by removing it -- see `../ROADMAP.md` Phase 7 for the
full story.

**Genuinely unresolved:** only checks fields the schema actually stores
(the source vision this phase is scoped from names ~20 identity fields
that don't exist in the schema at all); no hash/signature-based package
integrity; naming consistency doesn't extend to IK chains/retarget maps/
transformations; no combat balance checking. See `../ROADMAP.md` Phase 7
for the complete list.

## BuildPipeline

Pure orchestration, no new validation logic: `BuildPipeline::Run(projectDir)`
recursively discovers every `.dominus` file under a directory and runs
`PackageValidator::ValidateFile` against each, producing one `BuildReport`
(`AllPassed()`, `FailedCount()`, per-file results). `dominus-cli build`
runs it. Real orchestration of Layers 2 (Validation) + 9 (Build System)
from "The Dominus Pipeline" -- explicitly not Layer 3's "compile to
binary formats, stop loading JSON at runtime" claim, which stays the
same open "should Registry become the load path" decision flagged since
the original Registry Prototype phase.

Proved live against this engine's own `tests/fixtures`: found 2 genuine
failures out of 7 real files on the first run, including a pre-existing
fixture (`missing_identity.dominus`) neither targeted nor specifically
recalled going in. See `../ROADMAP.md`'s "The Dominus Pipeline" section
for the full write-up.
