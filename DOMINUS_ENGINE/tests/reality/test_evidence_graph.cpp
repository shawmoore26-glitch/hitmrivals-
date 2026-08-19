// tests/reality/test_evidence_graph.cpp
// Milestone 6: the graph model becomes evidence-derived. Two halves,
// tested separately:
//
//   - REALITY/EvidenceGraph.h itself: a generic container. Proven with
//     small, synthetic graphs that have never heard of Brooklyn --
//     duplicate-node refusal, dangling-edge refusal, self-dependency
//     refusal, cycle detection, topological order, and hash
//     determinism regardless of insertion order.
//   - REALITY/BrooklynEvidenceGraphBuilder: the real, concrete
//     evidence-derivation for Brooklyn -- every node has a real hash,
//     every edge is one this file's own comments cite to a real
//     function signature or binder behavior, and the Milestone 5
//     coupling is now a real, modeled RUNTIME edge instead of a
//     documented-but-unmodeled gap.
#include "REALITY/BrooklynEvidenceGraphBuilder.h"
#include "REALITY/EvidenceGraph.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <filesystem>

using dominus::reality::AuthorityType;
using dominus::reality::BrooklynEvidenceGraphBuilder;
using dominus::reality::EdgeType;
using dominus::reality::EvidenceEdge;
using dominus::reality::EvidenceGraph;
using dominus::reality::EvidenceNode;

namespace {

std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}

bool Contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

}  // namespace

// =====================================================================
// EvidenceGraph itself -- generic, synthetic, no Brooklyn involved
// =====================================================================

DOMINUS_TEST(EvidenceGraph_AddNode_RefusesDuplicateIds) {
    EvidenceGraph graph;
    DOMINUS_EXPECT(graph.AddNode({"a", "a.json", "hash1", AuthorityType::kSourceFile, "PRESENT"}));
    DOMINUS_EXPECT(!graph.AddNode({"a", "a2.json", "hash2", AuthorityType::kSourceFile, "PRESENT"}));
    DOMINUS_EXPECT(graph.nodes.size() == 1);
    // The original node is untouched -- a refused duplicate never
    // overwrites.
    DOMINUS_EXPECT(graph.FindNode("a")->artifact_hash == "hash1");
}

DOMINUS_TEST(EvidenceGraph_AddEdge_RefusesDanglingEndpoints_RecordsUndeclared) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "hash1", AuthorityType::kSourceFile, "PRESENT"});
    DOMINUS_EXPECT(!graph.AddEdge({"a", "b", EdgeType::kInput, "reason", "evidence"}));
    DOMINUS_EXPECT(graph.edges.empty());
    DOMINUS_EXPECT(graph.undeclared.size() == 1);
    DOMINUS_EXPECT(graph.undeclared[0].to == "b");
}

DOMINUS_TEST(EvidenceGraph_AddEdge_RefusesSelfDependency) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "hash1", AuthorityType::kSourceFile, "PRESENT"});
    DOMINUS_EXPECT(!graph.AddEdge({"a", "a", EdgeType::kInput, "reason", "evidence"}));
    DOMINUS_EXPECT(graph.edges.empty());
    DOMINUS_EXPECT(graph.undeclared.size() == 1);
}

DOMINUS_TEST(EvidenceGraph_Validate_DetectsCycles) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graph.AddNode({"b", "b.json", "h2", AuthorityType::kSourceFile, "PRESENT"});
    graph.AddNode({"c", "c.json", "h3", AuthorityType::kSourceFile, "PRESENT"});
    DOMINUS_EXPECT(graph.AddEdge({"a", "b", EdgeType::kInput, "r", "e"}));
    DOMINUS_EXPECT(graph.AddEdge({"b", "c", EdgeType::kInput, "r", "e"}));
    DOMINUS_EXPECT(graph.AddEdge({"c", "a", EdgeType::kInput, "r", "e"}));  // closes the cycle

    auto report = graph.Validate();
    DOMINUS_EXPECT(!report.no_cycles);
    DOMINUS_EXPECT(!report.ok);
    DOMINUS_EXPECT(!graph.TopologicalOrder().has_value());
}

DOMINUS_TEST(EvidenceGraph_Validate_PassesOnARealDag) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graph.AddNode({"b", "b.json", "h2", AuthorityType::kCertificate, "PRESENT"});
    graph.AddNode({"c", "c.json", "h3", AuthorityType::kArtifact, "PRESENT"});
    graph.AddEdge({"a", "b", EdgeType::kInput, "r", "e"});
    graph.AddEdge({"b", "c", EdgeType::kDerived, "r", "e"});

    auto report = graph.Validate();
    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(report.no_duplicate_nodes);
    DOMINUS_EXPECT(report.no_dangling_edges);
    DOMINUS_EXPECT(report.no_self_dependencies);
    DOMINUS_EXPECT(report.no_cycles);
    DOMINUS_EXPECT(report.issues.empty());
}

DOMINUS_TEST(EvidenceGraph_TopologicalOrder_RespectsRealEdges) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graph.AddNode({"b", "b.json", "h2", AuthorityType::kCertificate, "PRESENT"});
    graph.AddNode({"c", "c.json", "h3", AuthorityType::kArtifact, "PRESENT"});
    graph.AddEdge({"a", "b", EdgeType::kInput, "r", "e"});
    graph.AddEdge({"b", "c", EdgeType::kDerived, "r", "e"});

    auto order = graph.TopologicalOrder();
    DOMINUS_EXPECT(order.has_value());
    auto indexOf = [&](const std::string& id) { return std::find(order->begin(), order->end(), id) - order->begin(); };
    DOMINUS_EXPECT(indexOf("a") < indexOf("b"));
    DOMINUS_EXPECT(indexOf("b") < indexOf("c"));
}

DOMINUS_TEST(EvidenceGraph_ImpactedBy_TransitiveClosure) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graph.AddNode({"b", "b.json", "h2", AuthorityType::kCertificate, "PRESENT"});
    graph.AddNode({"c", "c.json", "h3", AuthorityType::kArtifact, "PRESENT"});
    graph.AddNode({"d", "d.json", "h4", AuthorityType::kSourceFile, "PRESENT"});  // independent branch
    graph.AddEdge({"a", "b", EdgeType::kInput, "r", "e"});
    graph.AddEdge({"b", "c", EdgeType::kDerived, "r", "e"});

    auto impacted = graph.ImpactedBy("a");
    DOMINUS_EXPECT(Contains(impacted, "a"));
    DOMINUS_EXPECT(Contains(impacted, "b"));
    DOMINUS_EXPECT(Contains(impacted, "c"));
    DOMINUS_EXPECT(!Contains(impacted, "d"));  // an unrelated artifact is never falsely invalidated
}

DOMINUS_TEST(EvidenceGraph_DependentsOf_And_DependenciesOf_AreDirectOnly) {
    EvidenceGraph graph;
    graph.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graph.AddNode({"b", "b.json", "h2", AuthorityType::kCertificate, "PRESENT"});
    graph.AddNode({"c", "c.json", "h3", AuthorityType::kArtifact, "PRESENT"});
    graph.AddEdge({"a", "b", EdgeType::kInput, "r", "e"});
    graph.AddEdge({"b", "c", EdgeType::kDerived, "r", "e"});

    auto dependentsOfA = graph.DependentsOf("a");
    DOMINUS_EXPECT(dependentsOfA.size() == 1 && dependentsOfA[0] == "b");  // NOT c -- that's indirect

    auto dependenciesOfC = graph.DependenciesOf("c");
    DOMINUS_EXPECT(dependenciesOfC.size() == 1 && dependenciesOfC[0] == "b");  // NOT a -- that's indirect
}

DOMINUS_TEST(EvidenceGraph_GraphHash_IsIndependentOfInsertionOrder) {
    EvidenceGraph graphA;
    graphA.subject = "test";
    graphA.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graphA.AddNode({"b", "b.json", "h2", AuthorityType::kCertificate, "PRESENT"});
    graphA.AddEdge({"a", "b", EdgeType::kInput, "r", "e"});

    EvidenceGraph graphB;
    graphB.subject = "test";
    graphB.AddNode({"b", "b.json", "h2", AuthorityType::kCertificate, "PRESENT"});  // opposite order
    graphB.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    graphB.AddEdge({"a", "b", EdgeType::kInput, "r", "e"});

    DOMINUS_EXPECT(graphA.GraphHash() == graphB.GraphHash());
}

DOMINUS_TEST(EvidenceGraph_GraphHash_ChangesWithRealContentDifference) {
    EvidenceGraph graphA;
    graphA.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});

    EvidenceGraph graphB;
    graphB.AddNode({"a", "a.json", "h1_DIFFERENT", AuthorityType::kSourceFile, "PRESENT"});

    DOMINUS_EXPECT(graphA.GraphHash() != graphB.GraphHash());
}

DOMINUS_TEST(EvidenceGraph_ChangedNodes_DetectsOnlyRealHashDifferences) {
    EvidenceGraph before;
    before.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    before.AddNode({"b", "b.json", "h2", AuthorityType::kSourceFile, "PRESENT"});
    before.AddNode({"c", "c.json", "h3", AuthorityType::kCertificate, "PRESENT"});

    EvidenceGraph after;
    after.AddNode({"a", "a.json", "h1_NEW", AuthorityType::kSourceFile, "PRESENT"});  // changed
    after.AddNode({"b", "b.json", "h2", AuthorityType::kSourceFile, "PRESENT"});      // unchanged
    after.AddNode({"c", "c.json", "h3", AuthorityType::kCertificate, "PRESENT"});     // unchanged

    auto changed = EvidenceGraph::ChangedNodes(before, after);
    DOMINUS_EXPECT(changed.size() == 1);
    DOMINUS_EXPECT(changed[0] == "a");
}

DOMINUS_TEST(EvidenceGraph_ChangedNodes_IgnoresNodesNotPresentInBothSnapshots) {
    EvidenceGraph before;
    before.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    EvidenceGraph after;
    after.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    after.AddNode({"b", "b.json", "h2", AuthorityType::kSourceFile, "PRESENT"});  // new, not "changed"
    DOMINUS_EXPECT(EvidenceGraph::ChangedNodes(before, after).empty());
}

DOMINUS_TEST(EvidenceGraph_ImpactOfChanges_UnionsImpactedByAcrossAllChangedNodes) {
    EvidenceGraph before;
    before.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    before.AddNode({"b", "b.json", "h2", AuthorityType::kSourceFile, "PRESENT"});
    before.AddNode({"cert", "cert", "h3", AuthorityType::kCertificate, "PRESENT"});
    before.AddNode({"art", "art", "h4", AuthorityType::kArtifact, "PRESENT"});
    before.AddEdge({"a", "cert", EdgeType::kInput, "r", "e"});
    before.AddEdge({"b", "cert", EdgeType::kInput, "r", "e"});
    before.AddEdge({"cert", "art", EdgeType::kDerived, "r", "e"});

    EvidenceGraph after = before;
    // Mutate BOTH independent source nodes simultaneously.
    for (auto& n : after.nodes) {
        if (n.node_id == "a") n.artifact_hash = "h1_NEW";
        if (n.node_id == "b") n.artifact_hash = "h2_NEW";
    }

    auto changed = EvidenceGraph::ChangedNodes(before, after);
    DOMINUS_EXPECT(changed.size() == 2);

    auto impact = EvidenceGraph::ImpactOfChanges(before, after);
    DOMINUS_EXPECT(Contains(impact, "a"));
    DOMINUS_EXPECT(Contains(impact, "b"));
    DOMINUS_EXPECT(Contains(impact, "cert"));
    DOMINUS_EXPECT(Contains(impact, "art"));
}

DOMINUS_TEST(EvidenceGraph_ImpactOfChanges_NoChanges_ReportsNoImpact) {
    EvidenceGraph before;
    before.AddNode({"a", "a.json", "h1", AuthorityType::kSourceFile, "PRESENT"});
    EvidenceGraph after = before;
    DOMINUS_EXPECT(EvidenceGraph::ImpactOfChanges(before, after).empty());
}

// =====================================================================
// BrooklynEvidenceGraphBuilder -- real evidence, real artifacts
// =====================================================================

DOMINUS_TEST(BrooklynEvidenceGraph_BuildsFromRealArtifacts_NoUndeclaredCitations) {
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    DOMINUS_EXPECT(graph.subject == "brooklyn");
    DOMINUS_EXPECT(!graph.nodes.empty());
    // Every citation this builder writes must actually resolve to a
    // real discovered node -- an empty `undeclared` list here means
    // every hand-written edge citation matched real evidence, not a
    // typo silently swallowed.
    DOMINUS_EXPECT(graph.undeclared.empty());
}

DOMINUS_TEST(BrooklynEvidenceGraph_EveryNodeHasARealHash) {
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    DOMINUS_EXPECT(graph.nodes.size() == 12);
    for (const auto& n : graph.nodes) {
        DOMINUS_EXPECT(!n.artifact_hash.empty());
        DOMINUS_EXPECT(n.state == "PRESENT");
    }
}

DOMINUS_TEST(BrooklynEvidenceGraph_EveryEdgeHasRealSourceAndEvidence) {
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    DOMINUS_EXPECT(!graph.edges.empty());
    for (const auto& e : graph.edges) {
        DOMINUS_EXPECT(!e.reason.empty());
        DOMINUS_EXPECT(!e.evidence.empty());
        // Every edge citation names a real node on both ends -- proven
        // structurally, not just asserted.
        DOMINUS_EXPECT(graph.FindNode(e.from) != nullptr);
        DOMINUS_EXPECT(graph.FindNode(e.to) != nullptr);
    }
}

DOMINUS_TEST(BrooklynEvidenceGraph_NoDuplicateNodes_NoDanglingEdges_NoSelfDependencies_NoCycles) {
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    auto report = graph.Validate();
    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(report.issues.empty());
}

DOMINUS_TEST(BrooklynEvidenceGraph_TopologicalOrder_Succeeds) {
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    auto order = graph.TopologicalOrder();
    DOMINUS_EXPECT(order.has_value());
    DOMINUS_EXPECT(order->size() == graph.nodes.size());
}

DOMINUS_TEST(BrooklynEvidenceGraph_ChangingVisualGenome_ImpactsVisualForgeAndRealityAndNowRigToo) {
    // The corrected, honest answer this milestone exists to produce:
    // visual_genome is a real RUNTIME input to rig_certificate too
    // (CheckRuntime's full RigBinder::Bind), not just an INPUT to
    // visualforge_certificate. The old Milestone 3 graph didn't know
    // this; this graph does, because the edge is derived from real
    // binder evidence instead of a hand-picked node list.
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    auto impacted = graph.ImpactedBy("brooklyn.visual_genome");
    DOMINUS_EXPECT(Contains(impacted, "brooklyn.visual_genome"));
    DOMINUS_EXPECT(Contains(impacted, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(Contains(impacted, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(Contains(impacted, "brooklyn.reality_artifact"));
}

DOMINUS_TEST(BrooklynEvidenceGraph_ChangingAnimation_ImpactsRigCertificateNotVisualForge) {
    // "combat" has no separate artifact/certificate of its own in this
    // codebase -- CheckCombat is one section composing rig_certificate,
    // not a standalone hashed artifact -- so animation's real consumer
    // is rig_certificate, not a "combat" node that doesn't exist as a
    // real, separately-hashed thing.
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    auto impacted = graph.ImpactedBy("brooklyn.animations");
    DOMINUS_EXPECT(Contains(impacted, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(Contains(impacted, "brooklyn.reality_artifact"));
    DOMINUS_EXPECT(!Contains(impacted, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(!Contains(impacted, "brooklyn.visual_genome"));
}

DOMINUS_TEST(BrooklynEvidenceGraph_ChangingUnrelatedArtifact_NeverInvalidatesTheWholeGraph) {
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    // combat_dna is real, present, and genuinely only reaches
    // rig_certificate/reality_artifact -- it must never appear to
    // impact the visual branch.
    auto impacted = graph.ImpactedBy("brooklyn.combat_dna");
    DOMINUS_EXPECT(!Contains(impacted, "brooklyn.visual_genome"));
    DOMINUS_EXPECT(!Contains(impacted, "brooklyn.material_genome"));
    DOMINUS_EXPECT(!Contains(impacted, "brooklyn.visual_style_genome"));
    DOMINUS_EXPECT(!Contains(impacted, "brooklyn.visualforge_certificate"));
}

DOMINUS_TEST(BrooklynEvidenceGraph_MotionGraphAndCombatDna_AreRuntimeOnly_NeverInputEdges) {
    // Real, verified evidence: neither motion_graph nor combat_dna is a
    // direct parameter to ANY CheckX function -- they're only reachable
    // through CheckRuntime's full bind. This test would fail if the
    // builder ever mis-cited one of these as a declared INPUT (a
    // stronger, more certain claim than what the evidence supports).
    auto graph = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    for (const auto& e : graph.edges) {
        if (e.from == "brooklyn.motion_graph" || e.from == "brooklyn.combat_dna") {
            DOMINUS_EXPECT(e.edge_type == EdgeType::kRuntime);
        }
    }
}

DOMINUS_TEST(BrooklynEvidenceGraph_TwoIndependentBuilds_AreHashIdentical) {
    auto graphA = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    auto graphB = BrooklynEvidenceGraphBuilder::Build(FixtureDir());
    DOMINUS_EXPECT(graphA.GraphHash() == graphB.GraphHash());
    DOMINUS_EXPECT(graphA.nodes.size() == graphB.nodes.size());
    DOMINUS_EXPECT(graphA.edges.size() == graphB.edges.size());
}

DOMINUS_TEST(BrooklynEvidenceGraph_MissingDominusFile_RefusesHonestly) {
    auto graph = BrooklynEvidenceGraphBuilder::Build("/tmp/dominus_evidence_graph_nonexistent_dir_xyz");
    DOMINUS_EXPECT(graph.nodes.empty());
    DOMINUS_EXPECT(!graph.undeclared.empty());
}
