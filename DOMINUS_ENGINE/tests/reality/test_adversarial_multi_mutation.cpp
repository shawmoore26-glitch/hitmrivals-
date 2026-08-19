// tests/reality/test_adversarial_multi_mutation.cpp
// Milestone 8: "prove that the dependency graph itself can survive
// adversarial mutations without inventing dependencies or missing real
// ones." Two real authority artifacts (brooklyn_canonical.skel.json
// and brooklyn_visual.json) are mutated SIMULTANEOUSLY, the graph is
// constructed and the full rebuild pipeline is run in OPPOSITE orders
// across two independent fixture copies, and the resulting registries
// and artifact hashes are checked for byte/hash identity.
//
// This file also does something Milestones 5-7 never did:
// cross-validates two independent CODE PATHS to the same value.
// Migration note (this phase): at the time this file was written,
// RealityRebuilder was still driven by the OLD DependencyGraph
// (Milestone 4/5/6), genuinely independent of the NEW EvidenceGraph
// (Milestone 7) -- a real cross-MODEL check. RealityRebuilder has
// since migrated onto EvidenceGraph as its sole dependency authority,
// so there is now only one graph model; the remaining, still-real
// claim (see SelectiveRebuild_ArtifactHash_MatchesIndependentFullEvidenceGraphRebuild
// below) is that RealityRebuilder's INCREMENTAL, registry-trusting
// computation agrees with a FULL, from-scratch, non-incremental
// rebuild -- both ultimately call the identical internal::CompileRig/
// CompileVisualForge/ComputeRealityArtifactHash functions
// (BrooklynDomainCompilers.h), so their answers for rig_certificate/
// visualforge_certificate/reality_artifact hashes MUST agree -- this
// is a real, structural guarantee, not a coincidence, and this test
// proves it holds rather than assuming it.
#include "REALITY/BrooklynEvidenceGraphBuilder.h"
#include "REALITY/EvidenceGraph.h"
#include "REALITY/RealityRebuilder.h"
#include "REALITY/RealityRegistry.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using dominus::reality::BrooklynEvidenceGraphBuilder;
using dominus::reality::EvidenceGraph;
using dominus::reality::RealityRebuilder;
using dominus::reality::RealityRegistry;

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

std::filesystem::path MakeMutableFixtureCopy(const std::string& testName) {
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_adversarial_" + testName);
    std::error_code ec;
    std::filesystem::remove_all(dest, ec);
    std::filesystem::create_directories(dest, ec);
    for (const auto& entry : std::filesystem::directory_iterator(FixtureDir())) {
        if (entry.is_regular_file()) {
            std::filesystem::copy_file(entry.path(), dest / entry.path().filename(),
                                        std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
    return dest;
}

std::string ReadWholeFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void OverwriteFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

bool Contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

// Applies BOTH real mutations this test suite uses throughout, to one
// fixture directory: a raw-byte skeleton edit and a real semantic
// visual_genome content edit. Both are independently real -- neither
// depends on the other, and they land in genuinely separate branches
// of the graph (skeleton -> rig_certificate only; visual_genome ->
// both rig_certificate [RUNTIME] and visualforge_certificate [INPUT]).
void ApplyBothMutations(const std::filesystem::path& dir) {
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    std::string visual = ReadWholeFile(dir / "brooklyn_visual.json");
    size_t pos = visual.find("\"chaotic\"");
    if (pos != std::string::npos) {
        visual.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
    } else {
        pos = visual.find("\"grim\"");
        visual.replace(pos, std::string("\"grim\"").size(), "\"chaotic\"");
    }
    OverwriteFile(dir / "brooklyn_visual.json", visual);
}

}  // namespace

// =====================================================================
// The graph alone: simultaneous multi-artifact mutation, verified
// against real, empirically-confirmed ground truth -- not assumed.
// =====================================================================

DOMINUS_TEST(EvidenceGraph_SimultaneousSkeletonAndVisualMutation_ChangedNodesMatchesRealBehavior) {
    auto dir = MakeMutableFixtureCopy("evidence_changed");
    auto beforeGraph = BrooklynEvidenceGraphBuilder::Build(dir);

    ApplyBothMutations(dir);

    auto afterGraph = BrooklynEvidenceGraphBuilder::Build(dir);
    auto changed = EvidenceGraph::ChangedNodes(beforeGraph, afterGraph);

    // The real, empirically-verified result: skeleton and visual_genome
    // changed (their raw-byte/parsed hashes genuinely differ), and so
    // does visualforge_certificate (its real package hash is sensitive
    // to the content edit) and reality_artifact (derived from it).
    DOMINUS_EXPECT(Contains(changed, "brooklyn.skeleton"));
    DOMINUS_EXPECT(Contains(changed, "brooklyn.visual_genome"));
    DOMINUS_EXPECT(Contains(changed, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(Contains(changed, "brooklyn.reality_artifact"));

    // The real, non-obvious finding this test exists to lock in:
    // rig_certificate's OWN hash does NOT change here -- CheckSkeleton/
    // CheckAnimation/CheckCombat/CheckRuntime/CheckDeterminism all
    // re-derive identical pass/fail and detail text from the
    // whitespace-only skeleton edit (the parsed bone data is
    // unchanged), and the genome content edit doesn't touch anything
    // rig_certificate's own sections read directly. This is real
    // engine behavior, not a test artifact -- rig_certificate is
    // absent from ChangedNodes precisely because it's true.
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.rig_certificate"));

    // Untouched branches stay untouched.
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.material_genome"));
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.visual_style_genome"));
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.motion_graph"));
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.combat_dna"));
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.moves"));
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.animations"));
    DOMINUS_EXPECT(!Contains(changed, "brooklyn.hurtbox"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(EvidenceGraph_SimultaneousMutation_ImpactOfChanges_ConservativelyIncludesRigCertificateAnyway) {
    // The exact principle stated in the directive: "this dependency
    // changed, therefore conservatively re-verify the dependent" --
    // even though rig_certificate's OWN hash didn't move (see the test
    // above), it is structurally downstream of the genuinely-changed
    // skeleton node, so ImpactOfChanges correctly still names it as
    // something that needed re-verification. The graph doesn't get
    // credit for "guessing right" that skeleton's edit was harmless --
    // it re-verified anyway, honestly, because that's what a real
    // dependency means.
    auto dir = MakeMutableFixtureCopy("evidence_impact");
    auto beforeGraph = BrooklynEvidenceGraphBuilder::Build(dir);
    ApplyBothMutations(dir);
    auto afterGraph = BrooklynEvidenceGraphBuilder::Build(dir);

    auto impact = EvidenceGraph::ImpactOfChanges(beforeGraph, afterGraph);
    DOMINUS_EXPECT(Contains(impact, "brooklyn.skeleton"));
    DOMINUS_EXPECT(Contains(impact, "brooklyn.visual_genome"));
    DOMINUS_EXPECT(Contains(impact, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(Contains(impact, "brooklyn.reality_artifact"));
    DOMINUS_EXPECT(Contains(impact, "brooklyn.rig_certificate"));  // conservative inclusion, proven above to be real

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// The full pipeline: simultaneous multi-branch mutation, opposite
// rebuild orders, registry + artifact hash convergence
// =====================================================================

DOMINUS_TEST(RealityRebuilder_SimultaneousMultiMutation_OppositeRebuildOrders_ConvergeToIdenticalRegistry) {
    auto dirA = MakeMutableFixtureCopy("multi_order_a");
    auto dirB = MakeMutableFixtureCopy("multi_order_b");
    auto registryPathA = dirA / ".reality_registry.json";
    auto registryPathB = dirB / ".reality_registry.json";

    // Real, independent baselines on both copies.
    auto bootstrapA = RealityRebuilder::RebuildFromChange(dirA, registryPathA, "brooklyn.skeleton");
    auto bootstrapB = RealityRebuilder::RebuildFromChange(dirB, registryPathB, "brooklyn.skeleton");
    DOMINUS_EXPECT(bootstrapA.ok);
    DOMINUS_EXPECT(bootstrapB.ok);

    // The identical simultaneous two-branch mutation, applied to both
    // independent copies.
    ApplyBothMutations(dirA);
    ApplyBothMutations(dirB);

    // Copy A: skeleton branch first, then visual branch.
    auto skeletonFirst = RealityRebuilder::RebuildFromChange(dirA, registryPathA, "brooklyn.skeleton");
    auto visualSecond = RealityRebuilder::RebuildFromChange(dirA, registryPathA, "brooklyn.visual_genome");
    DOMINUS_EXPECT(skeletonFirst.ok);
    DOMINUS_EXPECT(skeletonFirst.change_detected);
    DOMINUS_EXPECT(visualSecond.ok);
    DOMINUS_EXPECT(visualSecond.change_detected);

    // Copy B: visual branch first, then skeleton branch -- the
    // opposite order, same two real mutations.
    auto visualFirst = RealityRebuilder::RebuildFromChange(dirB, registryPathB, "brooklyn.visual_genome");
    auto skeletonSecond = RealityRebuilder::RebuildFromChange(dirB, registryPathB, "brooklyn.skeleton");
    DOMINUS_EXPECT(visualFirst.ok);
    DOMINUS_EXPECT(visualFirst.change_detected);
    DOMINUS_EXPECT(skeletonSecond.ok);
    DOMINUS_EXPECT(skeletonSecond.change_detected);

    // The real, central claim: two completely independent fixture
    // copies, mutated identically, rebuilt in opposite orders through
    // the real selective-recompilation pipeline, converge on a
    // byte-identical on-disk registry.
    std::string registryContentA = ReadWholeFile(registryPathA);
    std::string registryContentB = ReadWholeFile(registryPathB);
    DOMINUS_EXPECT(registryContentA == registryContentB);

    // And explicitly, the artifact hash specifically -- the value the
    // directive named directly ("verify the resulting registry and
    // artifact hashes are identical").
    auto finalRegistryA = RealityRegistry::Load(registryPathA);
    auto finalRegistryB = RealityRegistry::Load(registryPathB);
    DOMINUS_EXPECT(finalRegistryA.has_value());
    DOMINUS_EXPECT(finalRegistryB.has_value());
    std::string artifactHashA = *finalRegistryA->Find("brooklyn.reality_artifact");
    std::string artifactHashB = *finalRegistryB->Find("brooklyn.reality_artifact");
    DOMINUS_EXPECT(!artifactHashA.empty());
    DOMINUS_EXPECT(artifactHashA == artifactHashB);

    std::error_code ec;
    std::filesystem::remove_all(dirA, ec);
    std::filesystem::remove_all(dirB, ec);
}

DOMINUS_TEST(SelectiveRebuild_ArtifactHash_MatchesIndependentFullEvidenceGraphRebuild) {
    // Migration note: this test originally proved a genuine cross-model
    // guarantee (Milestone 3/6's DependencyGraph, driving
    // RealityRebuilder, vs. Milestone 7's EvidenceGraph, two
    // INDEPENDENTLY built graph models). As of RealityRebuilder's
    // migration onto EvidenceGraph (this phase), there is only one
    // graph model -- so that premise is no longer literally true, and
    // this comment says so rather than leaving a stale claim in place.
    //
    // What remains real and worth proving: RealityRebuilder computes
    // brooklyn.reality_artifact INCREMENTALLY -- only recompiling the
    // nodes a real change's invalidation set actually names, trusting
    // the registry for everything else. BrooklynEvidenceGraphBuilder::
    // Build() computes it FRESH, unconditionally, from scratch, every
    // time, with no incremental logic at all. These are two genuinely
    // different CODE PATHS to the same value (one selective and
    // registry-trusting, one exhaustive and stateless) -- both
    // ultimately call the same internal::CompileRig/CompileVisualForge/
    // ComputeRealityArtifactHash functions, so they MUST agree for the
    // same files. This test proves that holds, on a real simultaneous
    // multi-branch mutation, rather than assuming it.
    auto dir = MakeMutableFixtureCopy("cross_model");
    auto registryPath = dir / ".reality_registry.json";

    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // baseline
    ApplyBothMutations(dir);
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");

    auto registry = RealityRegistry::Load(registryPath);
    DOMINUS_EXPECT(registry.has_value());
    std::string incrementalArtifactHash = *registry->Find("brooklyn.reality_artifact");

    // A completely independent, from-scratch, non-incremental
    // reconstruction from the same post-mutation, post-rebuild files.
    auto freshEvidenceGraph = BrooklynEvidenceGraphBuilder::Build(dir);
    std::string fullRebuildArtifactHash = freshEvidenceGraph.FindNode("brooklyn.reality_artifact")->artifact_hash;

    DOMINUS_EXPECT(!incrementalArtifactHash.empty());
    DOMINUS_EXPECT(!fullRebuildArtifactHash.empty());
    DOMINUS_EXPECT(incrementalArtifactHash == fullRebuildArtifactHash);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(EvidenceGraph_ConstructedIndependentlyTwiceAfterMultiMutation_IsHashIdentical) {
    // "Construct the graph independently in different orders" for the
    // NEW graph model directly: two completely independent builds of
    // the post-mutation state, from two separate fixture copies that
    // underwent identical mutations, must be graph-hash identical --
    // the builder's own internal iteration order (a std::map, always
    // sorted by key) makes this deterministic by construction, and
    // this test proves that holds under a real multi-artifact mutation
    // rather than the single-artifact case Milestone 7 already covered.
    auto dirA = MakeMutableFixtureCopy("graph_identical_a");
    auto dirB = MakeMutableFixtureCopy("graph_identical_b");
    ApplyBothMutations(dirA);
    ApplyBothMutations(dirB);

    auto graphA = BrooklynEvidenceGraphBuilder::Build(dirA);
    auto graphB = BrooklynEvidenceGraphBuilder::Build(dirB);

    DOMINUS_EXPECT(graphA.GraphHash() == graphB.GraphHash());
    DOMINUS_EXPECT(graphA.undeclared.empty());
    DOMINUS_EXPECT(graphB.undeclared.empty());
    auto reportA = graphA.Validate();
    auto reportB = graphB.Validate();
    DOMINUS_EXPECT(reportA.ok);
    DOMINUS_EXPECT(reportB.ok);

    std::error_code ec;
    std::filesystem::remove_all(dirA, ec);
    std::filesystem::remove_all(dirB, ec);
}
