// REALITY/Reconciler.h
// Milestone 12: Recovery / Reconciliation on restart.
//
// The problem this file exists to solve: filesystem events are
// ephemeral; source state is persistent. If DOMINUS was stopped (or
// crashed) while an authoritative file changed, no inotify event for
// that change will ever arrive -- it happened while nobody was
// listening. Without this file, that change stays permanently
// unnoticed until something else happens to touch the same file
// again.
//
// The fix is deliberately NOT a new authority system. It is a
// DIFFERENT EVENT SOURCE feeding the exact same pipeline Milestone 10
// already built and proved: instead of inotify handing
// ChangeEventNormalizer a batch of raw OS-observed events, this file
// hands it a synthetic batch covering EVERY declared authoritative
// artifact -- a full scan instead of a stream. Canonicalization,
// deduplication, verification, impact analysis, selective
// recompilation, locking, and atomic persistence are all still owned
// by the exact same code (ChangeEventNormalizer, RealityRebuilder) --
// this file contains none of that logic itself, the same discipline
// RealityWatcher (Milestone 11) already follows for the live-event
// path.
//
//   process restarts
//         |
//   registry exists (or doesn't -- RealityRebuilder's own bootstrap
//         |          path, Milestone 4, handles that case unmodified)
//   Reconciler::Reconcile()
//         |
//   enumerate every declared authoritative artifact file
//         |          (BrooklynEvidenceGraphBuilder::DiscoverArtifactFiles
//         |           -- the SAME real discovery Milestone 7/10 built)
//   synthesize one RawFileEvent per file
//         |
//   ChangeEventNormalizer::Normalize   (Milestone 10, unmodified)
//         |
//   RealityRebuilder::RebuildFromChange, once per verified change
//         |          (Milestone 4-9, unmodified)
//   atomic commit
//         |
//   watching resumes
//
// The files remain authoritative. The registry remains derived
// evidence. The graph remains dependency authority. This file remains
// merely a second way of asking "what does the registry not yet know
// about" -- a full inspection instead of a live stream.
#pragma once

#include <chrono>
#include <filesystem>
#include <vector>

#include "REALITY/ChangeEventNormalizer.h"
#include "REALITY/RealityRebuilder.h"

namespace dominus::reality {

class Reconciler {
public:
    // Enumerates every real, declared authoritative artifact file via
    // BrooklynEvidenceGraphBuilder::DiscoverArtifactFiles (not a
    // hand-maintained list -- the same evidence-derived discovery
    // Milestone 7 built), synthesizes one RawFileEvent per file (kind
    // is irrelevant here and always reported as kUnknown -- reconciled
    // events don't claim to know HOW a file changed while nobody was
    // watching, only that it should be checked), and runs the whole
    // batch through ChangeEventNormalizer::Normalize -- read-only,
    // never touches the registry. Safe to call repeatedly to inspect
    // what reconciliation WOULD find before acting on any of it.
    static NormalizedChangeSet Scan(const std::filesystem::path& fixtureDir, const std::filesystem::path& registryPath);

    // Scan(), then drive RealityRebuilder::RebuildFromChange once per
    // verified change -- "wake up, inspect reality, and say here's
    // what changed while I wasn't watching." Each call is
    // independently locked (Milestone 9); reconciliation does not
    // introduce a new, bigger transaction spanning multiple nodes.
    static std::vector<RebuildReport> Reconcile(
        const std::filesystem::path& fixtureDir, const std::filesystem::path& registryPath,
        std::chrono::milliseconds lockTimeout = std::chrono::milliseconds(5000));
};

}  // namespace dominus::reality
