// REALITY/RealityWatcher.h
// Milestone 11: Reality Watcher. Deliberately thin -- everything
// intelligent already exists underneath it (Milestones 1-10).
// Milestone 12 (Reconciler) added one more thing this class delegates
// to, at startup, rather than implements: recovering awareness of
// changes that happened while the process wasn't running. This
// file's ENTIRE job:
//
//   process starts
//         |
//   Reconciler::Reconcile()  (Milestone 12 -- a full scan against the
//         |                   registry, catching offline changes)
//   OS filesystem notification (real inotify)
//         |
//   RawFileEvent            (this file's only translation logic:
//         |                  inotify's wd/mask/name -> a path + kind)
//   ChangeEventNormalizer   (Milestone 10, unmodified)
//         |
//   VerifiedChangeSet
//         |
//   RealityRebuilder        (Milestone 4-9, unmodified)
//
// This file contains NO:
//   - dependency logic (EvidenceGraph::ImpactedBy, owned by RealityRebuilder)
//   - rebuild logic (internal::CompileRig/CompileVisualForge, owned by
//     REALITY/BrooklynDomainCompilers.h)
//   - artifact classification logic (DiscoverArtifactFiles, owned by
//     BrooklynEvidenceGraphBuilder)
//   - authority decisions (PipelineLifecycle, owned by CompilationContext)
//   - registry mutation logic (RealityRegistry::Save, owned by RealityRegistry)
//   - special cases for Brooklyn (this class watches a directory; it
//     has no idea what a "skeleton" or "genome" is)
//   - its own hash system (Sha256 lives in REGISTRY, already used by
//     everything downstream of this file)
//
// Deliberately boring, per direction: one directory (non-recursive --
// Brooklyn's fixtures are flat), one inotify file descriptor, one
// "queue" (whatever the kernel already batched into a single read()),
// no parallel rebuild workers, graceful shutdown via a self-pipe.
#pragma once

#include <atomic>
#include <filesystem>
#include <vector>

#include "REALITY/ChangeEventNormalizer.h"
#include "REALITY/RealityRebuilder.h"
#include "REALITY/Reconciler.h"

namespace dominus::reality {

class RealityWatcher {
public:
    // Opens inotify and registers the watch on `watchDir` IMMEDIATELY,
    // synchronously, in the constructor -- so that by the time Run()
    // is called (even from a background thread), the watch is already
    // active and no caller has to guess when it's "ready." Throws
    // std::runtime_error on a real OS failure (inotify_init1 /
    // inotify_add_watch / pipe failing) -- refuses to construct a
    // watcher that silently isn't watching anything.
    RealityWatcher(std::filesystem::path watchDir, std::filesystem::path registryPath);
    ~RealityWatcher();

    RealityWatcher(const RealityWatcher&) = delete;
    RealityWatcher& operator=(const RealityWatcher&) = delete;

    // Blocks the calling thread. If `reconcileOnStart` is true
    // (default), the FIRST thing this does -- before registering any
    // interest in live events -- is call Reconciler::Reconcile()
    // (Milestone 12): a full scan of every declared authoritative
    // artifact against the registry, catching any change that
    // happened while this process wasn't running to observe it via
    // inotify. Filesystem events are ephemeral; source state is
    // persistent -- this is what keeps a restart from permanently
    // losing awareness of an offline change. Reconciliation's results
    // are appended to History() exactly like any live event's would
    // be; there is no separate code path or separate "kind" of
    // RebuildReport for it.
    //
    // After reconciliation, each loop iteration: waits (poll) for
    // either a real inotify event or a Stop() signal; on an event,
    // drains whatever the kernel has already batched into one read()
    // -- this is the entirety of this class's "batching policy," no
    // debounce timer and no coalescing logic of its own. Multiple raw
    // events collapse into one real change only because
    // ChangeEventNormalizer does that, downstream, exactly as it did
    // in Milestone 10. Each raw inotify_event is translated into a
    // RawFileEvent (path + kind -- the ONLY logic this class
    // performs), and the whole batch is handed to
    // ChangeEventNormalizer::ProcessEvents in one call. Returns when
    // Stop() is called or a real, unrecoverable OS error occurs.
    void Run(bool reconcileOnStart = true);

    // Thread-safe. Signals Run() to return after its current iteration
    // completes -- an in-flight ProcessEvents call (which may be
    // holding Milestone 9's real registry lock) is allowed to finish;
    // this never interrupts mid-rebuild.
    void Stop();

    // Every real RebuildReport this watcher's ProcessEvents calls have
    // produced so far, in order. Real reports, not synthesized --
    // exposed for tests and the CLI demo to inspect what actually
    // happened.
    const std::vector<RebuildReport>& History() const { return history_; }

private:
    void HandleReadyEvents();

    std::filesystem::path watchDir_;
    std::filesystem::path registryPath_;
    int inotifyFd_ = -1;
    int watchDescriptor_ = -1;
    int stopPipeRead_ = -1;
    int stopPipeWrite_ = -1;
    std::atomic<bool> stopRequested_{false};
    std::vector<RebuildReport> history_;
};

}  // namespace dominus::reality
