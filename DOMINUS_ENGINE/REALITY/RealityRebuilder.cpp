// REALITY/RealityRebuilder.cpp
#include "REALITY/RealityRebuilder.h"

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <set>

#include "REALITY/BrooklynDomainCompilers.h"
#include "REALITY/BrooklynEvidenceGraphBuilder.h"
#include "REALITY/EvidenceGraph.h"
#include "REALITY/FileLock.h"
#include "REALITY/RealityRegistry.h"

namespace dominus::reality {

namespace {

// Real, provable graph fact used here: a filtered subsequence of a
// valid topological order of the WHOLE graph is itself a valid
// topological order of any induced subgraph. Proof sketch: for any
// edge (u,v) in the induced subgraph on `nodeIds`, (u,v) is also an
// edge in the full graph, so u precedes v in any valid full-graph
// order; filtering to keep only `nodeIds` preserves relative order, so
// u still precedes v after filtering. This holds for every edge in the
// induced subgraph, so the filtered list respects all of them.
//
// This is why EvidenceGraph.h did not need a new "subset topological
// order" method added to it -- its existing, unmodified whole-graph
// TopologicalOrder() is sufficient; this function only filters its
// real output. Milestone 3's local Kahn's-algorithm-on-a-subgraph
// helper is retired along with DependencyGraph -- not reimplemented
// here under a different name.
std::optional<std::vector<std::string>> TopologicalOrderOfSubset(const EvidenceGraph& graph,
                                                                   const std::vector<std::string>& nodeIds) {
    auto fullOrder = graph.TopologicalOrder();
    if (!fullOrder.has_value()) return std::nullopt;  // a real cycle in the whole graph

    std::set<std::string> inSet(nodeIds.begin(), nodeIds.end());
    std::vector<std::string> filtered;
    for (const auto& id : *fullOrder) {
        if (inSet.count(id)) filtered.push_back(id);
    }
    // A node in `nodeIds` that isn't actually present in the graph
    // (a real caller bug, not a normal outcome) would silently vanish
    // from `filtered` rather than erroring -- refuse instead of
    // returning a wrong-size order.
    if (filtered.size() != nodeIds.size()) return std::nullopt;
    return filtered;
}

}  // namespace

RebuildReport RealityRebuilder::RebuildFromChange(const std::filesystem::path& fixtureDir,
                                                    const std::filesystem::path& registryPath,
                                                    const std::string& nodeId,
                                                    std::chrono::milliseconds lockTimeout) {
    RebuildReport report;
    report.subject = "brooklyn";
    report.changed_node_id = nodeId;

    // --- CONCURRENCY: exclusive lock around the WHOLE Load -> compute ->
    // Save cycle. CONFLICT -> REJECT: a caller that can't get the lock
    // within `lockTimeout` refuses outright, never proceeds
    // unsynchronized and never silently becomes a last-write-wins race.
    std::filesystem::path lockPath = registryPath;
    lockPath += ".lock";
    FileLock lock(lockPath);
    if (!lock.Acquire(lockTimeout)) {
        report.lock_contention = true;
        report.ok = false;
        report.summary = "could not acquire the exclusive registry lock at '" + lockPath.string() + "' within " +
                          std::to_string(lockTimeout.count()) +
                          "ms -- another process is using this registry; refusing rather than proceeding "
                          "unsynchronized";
        return report;
    }

    // --- FILE CHANGE / AUTHORITATIVE HASH CHANGE ------------------------
    // The FAST structural build -- source hashes + real edges only, no
    // certificate compiles paid for on every call. EvidenceGraph
    // (Milestone 7) is now the sole dependency authority for this
    // production consumer; the old DependencyGraph/ImpactAnalyzer are
    // no longer read anywhere in this file.
    EvidenceGraph currentGraph = BrooklynEvidenceGraphBuilder::BuildStructure(fixtureDir);
    const EvidenceNode* changedNode = currentGraph.FindNode(nodeId);
    if (!changedNode) {
        report.ok = false;
        report.summary = "'" + nodeId + "' is not a node in Brooklyn's dependency graph";
        return report;
    }
    if (changedNode->authority_type != AuthorityType::kSourceFile) {
        report.ok = false;
        report.summary = "'" + nodeId + "' is a derived node (" +
                          std::string(AuthorityTypeName(changedNode->authority_type)) +
                          "); a rebuild must originate from a real source-file change -- a certificate or "
                          "artifact node cannot itself be told it 'changed', only recomputed from what actually did";
        return report;
    }

    // One real node-compile step, shared by both the normal
    // changed-node path and the bootstrap path below -- exactly one
    // place decides how each node id gets its real hash, so bootstrap
    // and incremental rebuilds can never silently diverge in what
    // counts as "compiled".
    auto processNode = [&](const std::string& id, std::map<std::string, std::string>& hashes) -> RecompiledNode {
        const EvidenceNode* node = currentGraph.FindNode(id);
        RecompiledNode result;
        result.node_id = id;

        if (node->authority_type == AuthorityType::kSourceFile) {
            result.passed = !node->artifact_hash.empty();
            result.detail = result.passed ? "source re-hashed from disk: " + node->artifact_hash.substr(0, 16)
                                           : "source file unreadable";
            hashes[id] = node->artifact_hash;
        } else if (id == "brooklyn.rig_certificate") {
            auto cert = internal::CompileRig(fixtureDir);
            result.passed = cert.overall_pass;
            result.detail = "certificate_hash=" + cert.certificate_hash.substr(0, 16) +
                             " overall_pass=" + (cert.overall_pass ? "true" : "false");
            hashes[id] = cert.certificate_hash;
        } else if (id == "brooklyn.visualforge_certificate") {
            auto visualResult = internal::CompileVisualForge(fixtureDir / "brooklyn_canonical.dominus");
            if (!visualResult.has_value()) {
                result.passed = false;
                result.detail = "package build refused: brooklyn_canonical.dominus failed to load, or has no "
                                 "bound VisualGenome";
                hashes[id] = "";
            } else {
                bool pass = visualResult->certificate.structurally_sound && visualResult->certificate.renderable;
                result.passed = pass;
                result.detail = "certificate_hash=" + visualResult->certificate.certificate_hash.substr(0, 16) +
                                 " structurally_sound=" +
                                 (visualResult->certificate.structurally_sound ? "true" : "false") +
                                 " renderable=" + (visualResult->certificate.renderable ? "true" : "false");
                hashes[id] = visualResult->certificate.certificate_hash;
            }
        } else if (id == "brooklyn.reality_artifact") {
            std::string rigHash = hashes.count("brooklyn.rig_certificate") ? hashes["brooklyn.rig_certificate"] : "";
            std::string visualHash =
                hashes.count("brooklyn.visualforge_certificate") ? hashes["brooklyn.visualforge_certificate"] : "";
            if (rigHash.empty() || visualHash.empty()) {
                result.passed = false;
                result.detail = "cannot register: one or both upstream certificate hashes are missing or empty";
            } else {
                std::string newArtifactHash = internal::ComputeRealityArtifactHash(
                    "brooklyn", "DOMINUS_REALITY_COMPILER_V1", rigHash, visualHash);
                result.passed = true;
                result.detail = "artifact_hash=" + newArtifactHash.substr(0, 16);
                hashes[id] = newArtifactHash;
            }
        } else {
            // No real compiler exists for this node -- refuse rather
            // than fabricate a pass. (Unreachable for Brooklyn's fixed
            // 9-node graph; kept explicit so a future node addition
            // fails loudly here instead of silently "succeeding".)
            result.passed = false;
            result.detail = "no real compiler is wired for node '" + id + "' in RealityRebuilder";
        }
        return result;
    };

    auto registryOpt = RealityRegistry::Load(registryPath);
    if (!registryOpt.has_value()) {
        // Honest bootstrap: no baseline exists. A registry entry that
        // only recorded empty certificate/artifact hashes (Milestone
        // 3's fast graph builder never runs the real harnesses) would
        // be useless as a trust boundary for the very first real
        // rebuild -- so bootstrap actually compiles every node once,
        // through the exact same real compilers an incremental rebuild
        // uses, to establish a genuinely valid baseline. This is real
        // work, not a shortcut: there is no way to know a certificate's
        // real hash without actually running its real check.
        std::vector<std::string> allNodeIds;
        for (const auto& node : currentGraph.nodes) allNodeIds.push_back(node.node_id);
        std::sort(allNodeIds.begin(), allNodeIds.end());

        auto topoOrder = TopologicalOrderOfSubset(currentGraph, allNodeIds);
        if (!topoOrder.has_value()) {
            report.ok = false;
            report.summary = "dependency cycle detected while establishing the initial baseline -- refusing";
            return report;
        }

        std::map<std::string, std::string> hashes;
        for (const auto& id : *topoOrder) {
            RecompiledNode result = processNode(id, hashes);
            report.recompiled_order.push_back(id);
            report.results.push_back(result);
            if (!result.passed) {
                report.ok = false;
                report.summary = "could not establish an initial baseline: '" + id + "' failed real validation (" +
                                  result.detail + ")";
                return report;
            }
        }

        RealityRegistry fresh;
        fresh.subject = currentGraph.subject;
        fresh.node_hashes = hashes;
        bool saved = fresh.Save(registryPath);
        report.ok = saved;
        report.change_detected = false;
        report.invalidation_set = allNodeIds;
        report.summary = saved ? "no registry existed at '" + registryPath.string() +
                                      "' -- established a real baseline by compiling all " +
                                      std::to_string(allNodeIds.size()) + " nodes; nothing had 'changed' yet"
                                : "baseline compiled successfully but could not be saved to '" +
                                      registryPath.string() + "'";
        return report;
    }
    RealityRegistry registry = *registryOpt;

    const std::string* previousHash = registry.Find(nodeId);
    bool changed = (previousHash == nullptr) || (*previousHash != changedNode->artifact_hash);
    if (!changed) {
        report.ok = true;
        report.change_detected = false;
        report.summary = "no change detected for '" + nodeId + "' -- nothing recompiled";
        return report;
    }
    report.change_detected = true;

    // --- EvidenceGraph::ImpactedBy -> EXACT INVALIDATION SET ---------------
    // Milestone 7's own graph, its own real method, called read-only.
    // Not reimplemented -- this file does not know how to traverse a
    // dependency edge itself.
    report.invalidation_set = currentGraph.ImpactedBy(nodeId);
    std::sort(report.invalidation_set.begin(), report.invalidation_set.end());

    // --- DEPENDENCY / TOPOLOGICAL ORDER -----------------------------------
    auto topoOrder = TopologicalOrderOfSubset(currentGraph, report.invalidation_set);
    if (!topoOrder.has_value()) {
        report.ok = false;
        report.summary = "dependency cycle detected in the invalidation set -- refusing to guess a rebuild order";
        return report;
    }

    // Working hash table: seeded from the registry (trusted,
    // already-validated state for anything NOT being recompiled this
    // pass) plus the current graph's own freshly-read source hashes
    // for anything the registry doesn't yet know about.
    std::map<std::string, std::string> currentHashes = registry.node_hashes;
    for (const auto& node : currentGraph.nodes) {
        if (!currentHashes.count(node.node_id) && !node.artifact_hash.empty()) currentHashes[node.node_id] = node.artifact_hash;
    }

    // --- CANONICAL COMPILER -> VALIDATION, one real node at a time -------
    for (const auto& id : *topoOrder) {
        RecompiledNode result = processNode(id, currentHashes);
        report.recompiled_order.push_back(id);
        report.results.push_back(result);

        if (!result.passed) {
            report.ok = false;
            report.summary = "rebuild stopped: '" + id + "' failed real validation (" + result.detail +
                              ") -- nothing downstream was attempted";
            return report;
        }
    }

    // No success if an expected compiler was skipped: the set actually
    // processed must exactly equal EvidenceGraph::ImpactedBy's own
    // invalidation set -- same members, same count.
    std::vector<std::string> sortedRecompiled = report.recompiled_order;
    std::sort(sortedRecompiled.begin(), sortedRecompiled.end());
    if (sortedRecompiled != report.invalidation_set) {
        report.ok = false;
        report.summary = "internal consistency failure: the recompiled set does not match EvidenceGraph's "
                          "invalidation set -- refusing to report success";
        return report;
    }

    // --- REGISTRY UPDATE ---------------------------------------------------
    // Only the nodes actually recompiled this pass are touched; every
    // other entry is left byte-identical.
    for (const auto& id : report.invalidation_set) {
        registry.node_hashes[id] = currentHashes[id];
    }
    registry.subject = currentGraph.subject;
    if (!registry.Save(registryPath)) {
        report.ok = false;
        report.summary = "every affected node recompiled and passed, but the registry could not be saved to '" +
                          registryPath.string() + "'";
        return report;
    }

    report.ok = true;
    report.summary =
        "rebuilt " + std::to_string(report.invalidation_set.size()) + " node(s) from '" + nodeId + "', all passed";
    return report;
}

}  // namespace dominus::reality
