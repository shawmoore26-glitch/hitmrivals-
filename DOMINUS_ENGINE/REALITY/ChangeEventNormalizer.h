// REALITY/ChangeEventNormalizer.h
// Milestone 10: Change Event Normalization & Transaction Boundaries.
//
// The constitutional rule this file exists to enforce: a filesystem
// event is never authoritative evidence of a Reality change. Only a
// verified change in canonical source state can enter the Reality
// Compiler. This file is the boundary that enforces it -- it is
// deliberately NOT a filesystem watcher. It accepts a batch of raw,
// already-observed path events (from any source: a real inotify
// watcher, a manual list in a test, a directory scan) and is the
// "dumb watcher, authoritative compiler" boundary made real:
//
//   OS events
//       |
//   Event Collector        (this file accepts RawFileEvent batches)
//       |
//   Debounce / coalesce    (multiple events for the same real file ->
//       |                    one candidate -- handles bursts, duplicate
//       |                    events, and rename/create/delete sequences
//       |                    that are really one logical mutation)
//   Canonical Source Identity (BrooklynEvidenceGraphBuilder::
//       |                    DiscoverArtifactFiles -- the SAME real,
//       |                    evidence-derived discovery Milestone 7
//       |                    already built, reused here, not
//       |                    reimplemented. A path that doesn't match
//       |                    any declared artifact ref is REJECTED,
//       |                    not silently trusted and not silently
//       |                    dropped -- it's a named, reported refusal.)
//   Content/hash verification (re-reads the CURRENT file NOW and
//       |                    compares against RealityRebuilder's own
//       |                    notion of the last known-good hash --
//       |                    never trusts the event's claimed kind or
//       |                    timing. This is what makes "event before
//       |                    write finished" harmless: whatever the
//       |                    event claimed, only the state on disk AT
//       |                    VERIFICATION TIME matters.)
//   Logical ChangeSet      (the real, deduplicated, verified result)
//       |
//   RealityRebuilder       (Milestone 4-9, completely unmodified --
//                            this file calls it once per verified node,
//                            never decides "file X changed, therefore
//                            rebuild Y" itself; that decision already
//                            belongs to EvidenceGraph::ImpactedBy/RealityRebuilder)
//
// What this file does NOT do, on purpose: it does not watch the
// filesystem (no inotify, no polling loop) -- it is a pure function of
// whatever RawFileEvent batch it's handed. It does not decide impact
// (RealityRebuilder's own EvidenceGraph::ImpactedBy still owns that).
// It does not invent a node id for a file it doesn't recognize.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "REALITY/RealityRebuilder.h"

namespace dominus::reality {

enum class RawEventKind { kCreated, kModified, kDeleted, kRenamed, kUnknown };

inline const char* RawEventKindName(RawEventKind kind) {
    switch (kind) {
        case RawEventKind::kCreated: return "CREATED";
        case RawEventKind::kModified: return "MODIFIED";
        case RawEventKind::kDeleted: return "DELETED";
        case RawEventKind::kRenamed: return "RENAMED";
        case RawEventKind::kUnknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

// One raw, unverified, possibly-noisy observation. Nothing about this
// struct is trusted on its own -- `kind` in particular is informational
// only; verification always re-reads the actual file, never branches
// on what kind of event was claimed.
struct RawFileEvent {
    std::filesystem::path path;
    RawEventKind kind = RawEventKind::kUnknown;
};

// A path that could not be canonicalized -- not a declared
// authoritative artifact reference in brooklyn_canonical.dominus. A
// named refusal, not a silent drop and not a silent pass-through.
struct RejectedEvent {
    std::filesystem::path path;
    std::string reason;
};

// A path that DID canonicalize to a real artifact, but re-verification
// found its content hash unchanged from RealityRebuilder's own
// last-known-good value -- real noise (a duplicate event, a touch with
// no content change, a burst that resolved to nothing new), correctly
// excluded from the ChangeSet rather than triggering a no-op rebuild.
struct UnchangedArtifact {
    std::string node_id;             // a real EvidenceGraph node id -- RealityRebuilder's own vocabulary
    std::string evidence_node_id;    // identical to node_id as of this phase -- kept as a separate field
                                       // for interface stability, see NormalizedChangeSet's own comment
};

// As of RealityRebuilder's migration onto EvidenceGraph (Milestone 7
// authority), every real, declared artifact this file discovers has a
// real corresponding RealityRebuilder node -- this struct's one
// remaining honest use is a genuine internal inconsistency (evidence
// discovery and graph construction disagreeing about what the
// .dominus declares), which should never happen but is refused rather
// than silently skipped if it somehow did. See REALITY/README.md for
// the migration record.
struct UnrepresentedArtifact {
    std::string evidence_node_id;
    std::string detail;
};

// A real, verified, actionable change -- ready to hand to
// RealityRebuilder.
struct VerifiedChange {
    std::string node_id;           // a real EvidenceGraph node id -- RealityRebuilder's own vocabulary
    std::string evidence_node_id;  // identical to node_id as of this phase, see UnchangedArtifact's comment
    std::string detail;
};

struct NormalizedChangeSet {
    int raw_events_received = 0;
    int distinct_paths_after_dedup = 0;

    std::vector<RejectedEvent> rejected;
    std::vector<UnchangedArtifact> unchanged;
    std::vector<UnrepresentedArtifact> unrepresented;
    std::vector<VerifiedChange> verified;
};

class ChangeEventNormalizer {
public:
    // Canonicalize -> debounce/coalesce -> verify. Pure and read-only:
    // never touches the registry, never calls RealityRebuilder. Safe
    // to call repeatedly to inspect what a batch of raw events would
    // actually mean before acting on any of it.
    static NormalizedChangeSet Normalize(const std::filesystem::path& fixtureDir,
                                          const std::filesystem::path& registryPath,
                                          const std::vector<RawFileEvent>& events);

    // The full pipeline: Normalize(), then call
    // RealityRebuilder::RebuildFromChange once per verified node, in
    // sorted (deterministic) order. Each call is independently locked
    // (Milestone 9) -- this function does not wrap them in one bigger
    // transaction; it is a real, ordinary sequence of the same,
    // already-proven-safe primitive.
    static std::vector<RebuildReport> ProcessEvents(
        const std::filesystem::path& fixtureDir, const std::filesystem::path& registryPath,
        const std::vector<RawFileEvent>& events,
        std::chrono::milliseconds lockTimeout = std::chrono::milliseconds(5000));
};

}  // namespace dominus::reality
