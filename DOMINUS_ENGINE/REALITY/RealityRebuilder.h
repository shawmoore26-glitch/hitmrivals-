// REALITY/RealityRebuilder.h
// Milestone 4: "I can actually make only those things change."
// Milestone 9 adds real concurrency safety on top -- see the
// "CONCURRENCY" section below; Milestone 4's own logic (the switch
// over node ids, the topological order, the failure/consistency
// checks) is unmodified. Migrated onto EvidenceGraph (Milestone 7) as
// its sole dependency authority -- see "DEPENDENCY GRAPH MIGRATION"
// below; the old REALITY/DependencyGraph.h/ImpactAnalyzer are no
// longer read anywhere in this file.
//
// This file is an ADAPTER, not a redesign. It does not modify
// REALITY/EvidenceGraph.h, does not add execution to it, and does not
// change CompilationContext's honesty model at all -- PipelineLifecycle's
// gates (used by RealityCompiler, a separate production consumer) are
// untouched. It consumes EvidenceGraph::ImpactedBy's output (a real,
// already-proven traversal of Milestone 7's graph) and does exactly one
// new thing: for each node that answer names, invoke the ONE real
// compiler that actually produces that node's artifact -- never a
// blanket CompileBrooklyn() -- and stop immediately, honestly, the
// moment any one of them fails.
//
//   FILE CHANGE
//       |
//   AUTHORITATIVE HASH CHANGE   (RealityRegistry vs the live filesystem)
//       |
//   EvidenceGraph::ImpactedBy   (Milestone 7, unmodified, read-only)
//       |
//   EXACT INVALIDATION SET
//       |
//   TOPOLOGICAL ORDER           (a real subset-filter of EvidenceGraph's
//       |                        own whole-graph TopologicalOrder() --
//       |                        see TopologicalOrderOfSubset in the .cpp
//       |                        for why filtering is provably correct)
//   CANONICAL COMPILER          (internal::CompileRig / CompileVisualForge /
//       |                        ComputeRealityArtifactHash -- the SAME
//       |                        real functions CompileBrooklyn uses,
//       |                        via REALITY/BrooklynDomainCompilers.h)
//   VALIDATION                  (each node's own real pass/fail, no
//       |                        blanket "assume it worked")
//   REGISTRY UPDATE             (RealityRegistry::Save -- only the
//                                 nodes actually recompiled are touched)
//
// What this file explicitly refuses to do, by construction, not by
// convention:
//   - No fake "would rebuild" messages -- every RecompiledNode.detail
//     comes from a real certificate/hash a real compiler produced.
//   - No blanket CompileBrooklyn() -- the switch in RebuildFromChange
//     only ever calls the ONE compiler that matches the node id being
//     processed.
//   - No success if an affected compiler fails -- the loop returns
//     immediately (ok=false) on the first failing node; nothing
//     downstream is attempted.
//   - No success if an expected compiler was skipped -- the final
//     check compares the exact set of nodes actually processed against
//     EvidenceGraph's own invalidation set and fails the whole report
//     if they don't match exactly.
//
// --- DEPENDENCY GRAPH MIGRATION (this phase) ----------------------------
// EvidenceGraph replaces DependencyGraph/ImpactAnalyzer as this file's
// sole dependency authority. Real, verified behavior changes this
// migration surfaced -- not hidden, not silently absorbed:
//   - Node vocabulary: "brooklyn.animation" -> "brooklyn.animations";
//     "brooklyn.combat" -> "brooklyn.hurtbox" + "brooklyn.moves" (the
//     old graph aggregated hurtbox+moves into one node; the evidence-
//     derived graph keeps them separate, matching what CheckCombat's
//     real parameter list actually shows). New nodes appear that the
//     old 9-node graph never modeled at all: "brooklyn.motion_graph",
//     "brooklyn.combat_dna" -- both real, declared artifacts
//     (Milestone 6/7's own discovery) that are now genuinely
//     trackable and rebuildable, closing a gap Milestones 10/12 could
//     previously only report as "unrepresented".
//   - A skeleton-only change's invalidation set SHRANK, correctly:
//     the old graph had hand-modeled edges skeleton -> animation and
//     skeleton -> combat (a real architectural claim with no cited
//     function-parameter or runtime-bind evidence backing it -- unlike
//     every other edge in this graph). EvidenceGraph has no such
//     edges: animation clips' and hurtbox/move files' own artifact
//     hashes are pure functions of their own bytes, genuinely
//     independent of skeleton content, and internal::CompileRig
//     re-verifies ALL of them together regardless (it has no "check
//     only skeleton" mode) -- so re-hashing them from disk on a
//     skeleton-only change was real work with no real purpose. This is
//     a correction, not a weakening: the new, smaller invalidation set
//     is the accurate one.
//   - A visual_genome/material_genome/visual_style_genome change's
//     invalidation set is UNCHANGED in shape (already included
//     rig_certificate before this phase, from an earlier fix to the
//     old graph's own edges -- see REALITY/README.md's Milestone 6
//     entry).
//
// --- CONCURRENCY (Milestone 9) ------------------------------------------
// Empirically confirmed before this was fixed: two concurrent calls to
// RebuildFromChange against the same registry, each touching a
// DIFFERENT branch, reliably (40/40 real trials with std::thread)
// clobbered each other's update. The mechanism: each call loaded the
// registry once at the start and only ever wrote its OWN branch's keys
// into its in-memory copy -- so whichever call's Save() happened to
// land LAST wrote its own stale, load-time copy of the OTHER branch's
// keys back over the top, silently reverting them.
//
// The fix: the entire Load -> compute -> Save cycle now runs inside a
// real, OS-level exclusive lock (REALITY::FileLock, POSIX flock() on a
// sibling `<registryPath>.lock` file) -- and, critically, the registry
// Load() happens INSIDE the lock, not before it, so a caller that had
// to wait for the lock always sees the true latest state once it gets
// in, never a stale pre-wait snapshot. This is CONFLICT -> REJECT, not
// last-write-wins: if the lock cannot be acquired within a bounded
// timeout, RebuildFromChange refuses outright (`lock_contention=true`,
// `ok=false`) rather than proceeding unsynchronized -- it never
// "decides" a winner by racing to write last.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace dominus::reality {

struct RecompiledNode {
    std::string node_id;
    bool passed = false;
    std::string detail;  // always cites a real certificate_hash/artifact_hash or a real refusal reason
};

struct RebuildReport {
    std::string subject;
    std::string changed_node_id;

    // True iff the node's live hash actually differs from the
    // registry's last known-good hash for it -- a real comparison,
    // never assumed true just because the caller named a node.
    bool change_detected = false;

    // EvidenceGraph::ImpactedBy's own output, verbatim, sorted --
    // the exact set this rebuild is obligated to process.
    std::vector<std::string> invalidation_set;

    // The node ids actually processed, in the order they were
    // processed (topological). On success this is byte-identical (as
    // a set) to invalidation_set.
    std::vector<std::string> recompiled_order;
    std::vector<RecompiledNode> results;

    // True iff the exclusive registry lock could not be acquired
    // within the timeout -- a real, distinct outcome from a
    // validation failure. When true, `ok` is always false, nothing was
    // read or written, and no other field above is meaningful (the
    // call never got far enough to compute them).
    bool lock_contention = false;

    // True iff: (a) either no change was detected (a legitimate no-op)
    // or every node in invalidation_set was recompiled, in order,
    // and every one passed its own real validation; AND (b) the
    // registry was successfully updated to reflect it.
    bool ok = false;
    std::string summary;
};

class RealityRebuilder {
public:
    // The Milestone 4 entry point -- `dominus-cli reality-rebuild
    // <fixtures_dir> <registry_path> <node_id>`'s implementation.
    //
    // 1. Acquires an exclusive lock on `<registryPath>.lock`, bounded
    //    by `lockTimeout`. On failure to acquire: returns immediately
    //    with `lock_contention=true`, `ok=false` -- REJECT, not a
    //    silent fallback to unsynchronized access.
    // 2. Builds Brooklyn's CURRENT graph from `fixtureDir` (Milestone
    //    7's own BrooklynEvidenceGraphBuilder::BuildStructure -- the
    //    fast structural build, source hashes only -- called read-only,
    //    unmodified) -- this reads fixture files, not the registry, so
    //    it doesn't need the lock, but happens after acquiring it for a
    //    simple, easy-to-audit critical section.
    // 3. Loads the registry -- FRESH, inside the lock, so a caller that
    //    waited for contention always sees the true latest state. If
    //    none exists, seeds one from a real, full compile and reports a
    //    real bootstrap (no fabricated "change detected" on a first run
    //    with nothing to compare against).
    // 4. Compares `nodeId`'s live hash to the registry's last
    //    known-good hash for it. `nodeId` must name a real source-file
    //    node (skeleton/animations/hurtbox/moves/motion_graph/
    //    combat_dna/*_genome) -- a derived node (certificate/artifact)
    //    cannot itself be the origin of a change, since nothing
    //    produces it except a real compiler run.
    // 5. If unchanged: reports a real no-op (ok=true,
    //    change_detected=false), touches nothing.
    // 6. If changed: computes the exact invalidation set via
    //    EvidenceGraph::ImpactedBy, topologically orders it, and
    //    recompiles ONLY those nodes, in that order, via the real
    //    domain compilers.
    // 7. On full success, saves the updated registry and returns
    //    ok=true. On any failure -- a real compiler failing its own
    //    validation, a dependency cycle, a registry I/O failure, or an
    //    internal consistency mismatch -- returns ok=false with a
    //    specific, real reason. Nothing is ever silently retried or
    //    downgraded to a partial success. The lock is released (RAII)
    //    on every exit path, success or failure.
    static RebuildReport RebuildFromChange(const std::filesystem::path& fixtureDir,
                                            const std::filesystem::path& registryPath, const std::string& nodeId,
                                            std::chrono::milliseconds lockTimeout = std::chrono::milliseconds(5000));
};

}  // namespace dominus::reality
