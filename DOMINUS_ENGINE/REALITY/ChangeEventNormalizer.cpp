// REALITY/ChangeEventNormalizer.cpp
#include "REALITY/ChangeEventNormalizer.h"

#include <algorithm>
#include <map>
#include <set>

#include "REALITY/BrooklynEvidenceGraphBuilder.h"
#include "REALITY/EvidenceGraph.h"
#include "REALITY/RealityRegistry.h"

namespace dominus::reality {

NormalizedChangeSet ChangeEventNormalizer::Normalize(const std::filesystem::path& fixtureDir,
                                                       const std::filesystem::path& registryPath,
                                                       const std::vector<RawFileEvent>& events) {
    NormalizedChangeSet result;
    result.raw_events_received = static_cast<int>(events.size());

    // --- Canonical Source Identity ---------------------------------------
    // The SAME real, evidence-derived discovery Milestone 7 built --
    // reused, not reimplemented.
    auto artifactFiles = BrooklynEvidenceGraphBuilder::DiscoverArtifactFiles(fixtureDir);

    // Build filename -> evidence_node_id the same discovery already
    // implies, once, for O(1) lookups below.
    std::map<std::string, std::string> filenameToEvidenceNode;
    for (const auto& [nodeId, filenames] : artifactFiles) {
        for (const auto& filename : filenames) filenameToEvidenceNode[filename] = nodeId;
    }

    // --- Debounce / coalesce -----------------------------------------------
    // Every raw event is matched by filename (not full path -- a real
    // event's path is whatever the OS/watcher reported; what matters is
    // whether the FILE it names is a declared artifact). Multiple
    // events for the same file -- duplicates, bursts, a rename/create/
    // delete sequence an editor produces for one logical save -- all
    // collapse into the same single evidence_node_id candidate.
    std::set<std::string> candidateEvidenceNodes;
    std::set<std::string> distinctFilenamesSeen;
    for (const auto& event : events) {
        std::string filename = event.path.filename().string();
        if (distinctFilenamesSeen.count(filename)) continue;  // already resolved (or already rejected) this pass
        distinctFilenamesSeen.insert(filename);

        auto it = filenameToEvidenceNode.find(filename);
        if (it == filenameToEvidenceNode.end()) {
            result.rejected.push_back(
                {event.path, "'" + filename + "' is not a declared authoritative artifact reference in "
                                                "brooklyn_canonical.dominus"});
            continue;
        }
        candidateEvidenceNodes.insert(it->second);
    }
    result.distinct_paths_after_dedup = static_cast<int>(distinctFilenamesSeen.size());

    // --- Content/hash verification ------------------------------------------
    // Never trusts the event's claimed kind or timing -- always
    // re-derives the CURRENT real state and compares against
    // RealityRebuilder's own notion of the last known-good hash.
    //
    // As of this phase, RealityRebuilder's own dependency authority IS
    // EvidenceGraph (Milestone 7) -- the two node vocabularies are the
    // SAME vocabulary now, not two vocabularies bridged by a
    // translation table. Every real, declared artifact this builder
    // discovers is, by construction, a real node RealityRebuilder can
    // act on -- there is no longer a genuine "unrepresented" case (the
    // field stays on NormalizedChangeSet for the one remaining honest
    // use: a real internal inconsistency between DiscoverArtifactFiles
    // and BuildStructure, which should never happen but is refused
    // rather than silently skipped if it somehow did).
    EvidenceGraph currentGraph = BrooklynEvidenceGraphBuilder::BuildStructure(fixtureDir);
    auto registryOpt = RealityRegistry::Load(registryPath);

    for (const auto& nodeId : candidateEvidenceNodes) {
        const EvidenceNode* node = currentGraph.FindNode(nodeId);
        if (!node) {
            result.unrepresented.push_back(
                {nodeId, "DiscoverArtifactFiles named '" + nodeId +
                             "' but BuildStructure's graph has no such node -- refusing to guess"});
            continue;
        }

        const std::string* previousHash = registryOpt.has_value() ? registryOpt->Find(nodeId) : nullptr;
        bool changed = (previousHash == nullptr) || (*previousHash != node->artifact_hash);

        if (!changed) {
            result.unchanged.push_back({nodeId, nodeId});
        } else {
            result.verified.push_back({nodeId, nodeId,
                                        "live hash " + node->artifact_hash.substr(0, 12) +
                                            " differs from registry's last known-good value"});
        }
    }

    std::sort(result.verified.begin(), result.verified.end(),
              [](const VerifiedChange& a, const VerifiedChange& b) { return a.node_id < b.node_id; });

    return result;
}

std::vector<RebuildReport> ChangeEventNormalizer::ProcessEvents(const std::filesystem::path& fixtureDir,
                                                                  const std::filesystem::path& registryPath,
                                                                  const std::vector<RawFileEvent>& events,
                                                                  std::chrono::milliseconds lockTimeout) {
    NormalizedChangeSet changeSet = Normalize(fixtureDir, registryPath, events);

    std::vector<RebuildReport> reports;
    for (const auto& change : changeSet.verified) {
        reports.push_back(RealityRebuilder::RebuildFromChange(fixtureDir, registryPath, change.node_id, lockTimeout));
    }
    return reports;
}

}  // namespace dominus::reality
