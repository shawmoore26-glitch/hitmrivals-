// REALITY/EvidenceGraph.h
// Milestone 7: the graph model becomes evidence-derived. This was
// originally a NEW, richer graph living alongside the old
// REALITY/DependencyGraph.h -- as of the dependency graph migration
// (see REALITY/README.md), it is now the SOLE production dependency
// authority: RealityRebuilder, RealityCompiler, and the CLI all derive
// their dependency/impact information from this file. The old
// DependencyGraph.h has been fully retired and removed.
//
// The rule this file exists to enforce: "no manually typed dependency
// edges where the repository already contains evidence from which the
// edge can be derived." EvidenceGraph itself is a plain, generic
// container -- it does not know Brooklyn exists. It cannot invent an
// edge; every edge a caller adds must cite a `reason` (why this
// dependency is real) and `evidence` (the concrete source of truth --
// a file path + key, or a function signature). See
// REALITY/BrooklynEvidenceGraphBuilder.h for where those citations
// actually come from for Brooklyn specifically.
//
// The five edge types are deliberately distinct, per the directive:
//   AUTHORITY  -- this node IS the authoritative record for that fact
//                 (reserved for future use; no Brooklyn edge uses it
//                 yet -- see "genuinely unresolved" below)
//   INPUT      -- a real function parameter: some Check*/Build*
//                 function takes this artifact directly by name
//   DERIVED    -- a certificate/artifact composed FROM other
//                 certificates (rig_certificate + visualforge_certificate
//                 -> reality_artifact)
//   RUNTIME    -- consumed only through a whole-object bind (RigBinder::
//                 Bind / CombatBinder::Bind resolving every ref on the
//                 object), never as a named parameter to a specific
//                 check -- this is exactly the category Milestone 5's
//                 RIG/VisualForge coupling belongs in, modeled properly
//                 here instead of being a documented-but-unmodeled gap
//   EVIDENCE   -- reserved; a future edge type for "this fact was used
//                 to justify that fact" chains (not used by Brooklyn yet)
//   PROVENANCE -- reserved for lineage edges (e.g. brooklyn_canonical's
//                 own provenance.parent_entities -> brooklyn); not used
//                 by the certificate graph itself yet
//
// "Reserved, not used yet" is stated plainly rather than filled with a
// placeholder edge -- an edge that doesn't cite real evidence would be
// exactly the fabrication this whole file exists to prevent.
#pragma once

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "REGISTRY/Hash/Sha256.h"

namespace dominus::reality {

enum class AuthorityType { kSourceFile, kCertificate, kArtifact };

inline const char* AuthorityTypeName(AuthorityType t) {
    switch (t) {
        case AuthorityType::kSourceFile: return "SourceFile";
        case AuthorityType::kCertificate: return "Certificate";
        case AuthorityType::kArtifact: return "Artifact";
    }
    return "Unknown";
}

enum class EdgeType { kAuthority, kInput, kDerived, kRuntime, kEvidence, kProvenance };

inline const char* EdgeTypeName(EdgeType t) {
    switch (t) {
        case EdgeType::kAuthority: return "AUTHORITY";
        case EdgeType::kInput: return "INPUT";
        case EdgeType::kDerived: return "DERIVED";
        case EdgeType::kRuntime: return "RUNTIME";
        case EdgeType::kEvidence: return "EVIDENCE";
        case EdgeType::kProvenance: return "PROVENANCE";
    }
    return "UNKNOWN";
}

struct EvidenceNode {
    std::string node_id;
    std::string artifact_id;      // the real file/ref this node names, e.g. "brooklyn_visual.json"
    std::string artifact_hash;    // real, already-computed -- empty means "could not be computed", never invented
    AuthorityType authority_type = AuthorityType::kSourceFile;
    std::string state = "PRESENT";  // "PRESENT" | "MISSING" -- real, reflects whether the hash was obtainable
};

struct EvidenceEdge {
    std::string from;
    std::string to;
    EdgeType edge_type = EdgeType::kInput;
    std::string reason;     // human-readable: why this dependency is real
    std::string evidence;   // concrete citation: file path + key, or a function signature
};

// A dependency a caller tried to record but couldn't attach to real
// nodes on both ends -- recorded here instead of silently becoming a
// dangling or phantom edge. This is the mechanism that makes "unknown
// dependency is UNDECLARED, not inferred" real rather than aspirational.
struct UndeclaredDependency {
    std::string from;
    std::string to;
    std::string detail;
};

struct GraphValidationReport {
    bool no_duplicate_nodes = true;
    bool no_dangling_edges = true;
    bool no_self_dependencies = true;
    bool no_cycles = true;
    bool ok = true;
    std::vector<std::string> issues;
};

class EvidenceGraph {
public:
    std::string subject;
    std::vector<EvidenceNode> nodes;
    std::vector<EvidenceEdge> edges;
    std::vector<UndeclaredDependency> undeclared;

    const EvidenceNode* FindNode(const std::string& id) const {
        for (const auto& n : nodes) {
            if (n.node_id == id) return &n;
        }
        return nullptr;
    }

    // Refuses a duplicate node_id outright -- returns false rather than
    // silently overwriting or creating a second node with the same id
    // (which would make "no duplicate nodes" a lie).
    bool AddNode(EvidenceNode node) {
        if (FindNode(node.node_id) != nullptr) return false;
        nodes.push_back(std::move(node));
        return true;
    }

    // Refuses an edge whose `from` or `to` doesn't already exist as a
    // real node -- records it in `undeclared` instead of inserting a
    // dangling edge or fabricating a phantom node to make it fit. This
    // is the concrete mechanism behind "if runtime code establishes a
    // dependency that isn't represented in artifact metadata, that
    // should be reported as an undeclared dependency, not silently
    // inserted as graph truth" -- and behind "no self-dependencies".
    bool AddEdge(EvidenceEdge edge) {
        bool fromExists = FindNode(edge.from) != nullptr;
        bool toExists = FindNode(edge.to) != nullptr;
        if (edge.from == edge.to) {
            undeclared.push_back({edge.from, edge.to, "refused: self-dependency (" + edge.reason + ")"});
            return false;
        }
        if (!fromExists || !toExists) {
            std::string detail = "refused: ";
            if (!fromExists) detail += "'" + edge.from + "' is not a known node; ";
            if (!toExists) detail += "'" + edge.to + "' is not a known node; ";
            detail += edge.reason;
            undeclared.push_back({edge.from, edge.to, detail});
            return false;
        }
        edges.push_back(std::move(edge));
        return true;
    }

    // Real, comprehensive validation -- re-derives every guarantee
    // AddNode/AddEdge already enforce at insertion time, plus the one
    // thing that CAN'T be checked incrementally at insertion (a cycle
    // formed by the Nth edge, given the first N-1 were each individually
    // fine). Defense in depth, not redundant theater: a caller who
    // constructs a graph by any other means (e.g. deserializing one)
    // still gets a real check.
    GraphValidationReport Validate() const {
        GraphValidationReport report;

        std::set<std::string> seenIds;
        for (const auto& n : nodes) {
            if (!seenIds.insert(n.node_id).second) {
                report.no_duplicate_nodes = false;
                report.issues.push_back("duplicate node id: '" + n.node_id + "'");
            }
        }

        for (const auto& e : edges) {
            if (e.from == e.to) {
                report.no_self_dependencies = false;
                report.issues.push_back("self-dependency: '" + e.from + "' -> itself");
            }
            if (FindNode(e.from) == nullptr) {
                report.no_dangling_edges = false;
                report.issues.push_back("dangling edge: from '" + e.from + "' (no such node)");
            }
            if (FindNode(e.to) == nullptr) {
                report.no_dangling_edges = false;
                report.issues.push_back("dangling edge: to '" + e.to + "' (no such node)");
            }
        }

        if (!TopologicalOrder().has_value()) {
            report.no_cycles = false;
            report.issues.push_back("cycle detected in the edge set");
        }

        report.ok = report.no_duplicate_nodes && report.no_dangling_edges && report.no_self_dependencies &&
                    report.no_cycles;
        return report;
    }

    // Direct forward edges: nodes `nodeId` itself points to.
    std::vector<std::string> DependentsOf(const std::string& nodeId) const {
        std::set<std::string> result;
        for (const auto& e : edges) {
            if (e.from == nodeId) result.insert(e.to);
        }
        return std::vector<std::string>(result.begin(), result.end());
    }

    // Direct backward edges: nodes that point to `nodeId`.
    std::vector<std::string> DependenciesOf(const std::string& nodeId) const {
        std::set<std::string> result;
        for (const auto& e : edges) {
            if (e.to == nodeId) result.insert(e.from);
        }
        return std::vector<std::string>(result.begin(), result.end());
    }

    // Every node transitively downstream of `nodeId`, including itself
    // -- a real BFS over the real edge list. Unknown node ids return
    // just themselves.
    std::vector<std::string> ImpactedBy(const std::string& nodeId) const {
        std::set<std::string> visited;
        std::deque<std::string> queue;
        queue.push_back(nodeId);
        visited.insert(nodeId);
        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop_front();
            for (const auto& e : edges) {
                if (e.from != current) continue;
                if (visited.count(e.to)) continue;
                visited.insert(e.to);
                queue.push_back(e.to);
            }
        }
        std::vector<std::string> result(visited.begin(), visited.end());
        std::sort(result.begin(), result.end());
        return result;
    }

    // Whole-graph topological order via Kahn's algorithm. std::nullopt
    // on a real cycle -- never guesses an order for one.
    std::optional<std::vector<std::string>> TopologicalOrder() const {
        std::vector<std::string> allIds;
        for (const auto& n : nodes) allIds.push_back(n.node_id);
        std::sort(allIds.begin(), allIds.end());

        std::map<std::string, int> inDegree;
        for (const auto& id : allIds) inDegree[id] = 0;
        for (const auto& e : edges) {
            if (inDegree.count(e.to)) inDegree[e.to]++;
        }

        std::deque<std::string> ready;
        for (const auto& id : allIds) {
            if (inDegree[id] == 0) ready.push_back(id);
        }

        std::vector<std::string> order;
        while (!ready.empty()) {
            std::string current = ready.front();
            ready.pop_front();
            order.push_back(current);
            for (const auto& e : edges) {
                if (e.from != current) continue;
                if (--inDegree.at(e.to) == 0) ready.push_back(e.to);
            }
        }

        if (order.size() != allIds.size()) return std::nullopt;
        return order;
    }

    // A deterministic hash of the whole graph's content -- sorted by
    // node_id / (from,to,edge_type) before hashing, specifically so
    // that insertion order never affects the result. This is the real
    // mechanism behind "two independently constructed graphs are byte/
    // hash identical" and "opposite insertion order produces an
    // identical graph".
    std::string GraphHash() const {
        std::vector<EvidenceNode> sortedNodes = nodes;
        std::sort(sortedNodes.begin(), sortedNodes.end(),
                  [](const EvidenceNode& a, const EvidenceNode& b) { return a.node_id < b.node_id; });

        std::vector<EvidenceEdge> sortedEdges = edges;
        std::sort(sortedEdges.begin(), sortedEdges.end(), [](const EvidenceEdge& a, const EvidenceEdge& b) {
            if (a.from != b.from) return a.from < b.from;
            if (a.to != b.to) return a.to < b.to;
            return a.edge_type < b.edge_type;
        });

        std::ostringstream out;
        out << "subject=" << subject << ";";
        for (const auto& n : sortedNodes) {
            out << "[node id=" << n.node_id << ";artifact=" << n.artifact_id << ";hash=" << n.artifact_hash
                << ";type=" << AuthorityTypeName(n.authority_type) << ";state=" << n.state << "]";
        }
        for (const auto& e : sortedEdges) {
            out << "[edge from=" << e.from << ";to=" << e.to << ";type=" << EdgeTypeName(e.edge_type) << "]";
        }
        return registry::Sha256::Hash(out.str());
    }

    // --- Snapshot comparison: pure, generic, no Brooklyn knowledge --------
    // Static, comparing two independently-built snapshots, never
    // mutating either one -- the same discipline the retired
    // DependencyGraph.h's ImpactAnalyzer established for the older
    // graph. Added here (not as a separate class) because EvidenceGraph
    // is the only thing that knows its own node/edge shape; splitting
    // it out would just mean re-exposing every field as public anyway.

    // Every node id present in BOTH snapshots whose hash actually
    // differs. A node present in one snapshot but not the other is real
    // information (added/removed), not silently folded into "changed" --
    // callers that care can diff `nodes` directly.
    static std::vector<std::string> ChangedNodes(const EvidenceGraph& before, const EvidenceGraph& after) {
        std::vector<std::string> changed;
        for (const auto& afterNode : after.nodes) {
            const auto* beforeNode = before.FindNode(afterNode.node_id);
            if (!beforeNode) continue;
            if (beforeNode->artifact_hash != afterNode.artifact_hash) changed.push_back(afterNode.node_id);
        }
        std::sort(changed.begin(), changed.end());
        return changed;
    }

    // The union of `after.ImpactedBy(id)` for every node that actually
    // changed between the two snapshots -- uses `after`'s own edge
    // list (the graph shape as it exists now), same as the old
    // ImpactAnalyzer::ImpactOfChanges did for the other graph.
    static std::vector<std::string> ImpactOfChanges(const EvidenceGraph& before, const EvidenceGraph& after) {
        std::set<std::string> impacted;
        for (const auto& id : ChangedNodes(before, after)) {
            for (const auto& affected : after.ImpactedBy(id)) impacted.insert(affected);
        }
        std::vector<std::string> result(impacted.begin(), impacted.end());
        std::sort(result.begin(), result.end());
        return result;
    }
};

}  // namespace dominus::reality
