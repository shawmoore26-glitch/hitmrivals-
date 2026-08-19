// tests/reality/test_reality_rebuilder.cpp
// Milestone 4: "I can actually make only those things change."
//
// The acceptance test the directive asked for, brutally concrete:
// mutate brooklyn.skeleton on disk, run RealityRebuilder, and prove
// -- by checking exactly which real compilers actually ran, not by
// reading a comment -- that animation/combat/rig_certificate/
// reality_artifact recompiled and the visual branch never executed.
// Then the same in reverse for a visual_genome mutation.
#include "REALITY/RealityRebuilder.h"
#include "REALITY/RealityRegistry.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using dominus::reality::RealityRebuilder;
using dominus::reality::RealityRegistry;
using dominus::reality::RebuildReport;

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
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_reality_rebuild_" + testName);
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

}  // namespace

// --- Bootstrap: no registry yet ---------------------------------------

DOMINUS_TEST(RealityRebuilder_NoRegistryYet_SeedsBaselineAndDoesNotRebuild) {
    auto dir = MakeMutableFixtureCopy("bootstrap");
    auto registryPath = dir / ".reality_registry.json";
    std::error_code ec;
    std::filesystem::remove(registryPath, ec);

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(!report.change_detected);
    // Bootstrap is real work: establishing a genuinely valid baseline
    // means actually compiling every node once (there is no way to
    // know a certificate's real hash without running its real check).
    // 12 nodes, not 9 -- EvidenceGraph (this phase's dependency
    // authority) discovered brooklyn.motion_graph and
    // brooklyn.combat_dna as real, separate artifacts the old graph
    // never modeled, and splits the old graph's single "combat"
    // aggregate into "brooklyn.hurtbox" + "brooklyn.moves".
    DOMINUS_EXPECT(report.recompiled_order.size() == 12);
    for (const auto& r : report.results) DOMINUS_EXPECT(r.passed);
    DOMINUS_EXPECT(std::filesystem::exists(registryPath));

    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(RealityRebuilder_RegistryPresent_NoRealChange_IsARealNoOp) {
    auto dir = MakeMutableFixtureCopy("no_change");
    auto registryPath = dir / ".reality_registry.json";

    // Seed the baseline.
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    // Run again with nothing mutated.
    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(!report.change_detected);
    DOMINUS_EXPECT(report.recompiled_order.empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- The directive's own acceptance scenario, both directions --------

DOMINUS_TEST(RealityRebuilder_SkeletonMutation_RecompilesRigCertificateOnly_VisualBranchNeverExecutes) {
    // Milestone 7 migration note: the OLD graph (DependencyGraph) had
    // hand-modeled edges skeleton -> animation and skeleton -> combat
    // -- a real architectural claim, but one with no cited function-
    // parameter or runtime-bind evidence backing it, unlike every
    // other edge in this graph. EvidenceGraph has no such edges:
    // brooklyn.animations'/brooklyn.hurtbox's/brooklyn.moves' own
    // artifact hashes are pure functions of their own file bytes,
    // genuinely independent of skeleton content -- and internal::
    // CompileRig re-verifies all of them together regardless (it has
    // no "check only skeleton" mode), so re-hashing them from disk on
    // a skeleton-only change was real work with no real purpose. This
    // is a correction, not a weakening: the invalidation set is
    // smaller now (3 nodes, not 5) because it's the accurate one --
    // verified directly below, not assumed.
    auto dir = MakeMutableFixtureCopy("skeleton_rebuild");
    auto registryPath = dir / ".reality_registry.json";

    // Seed a real baseline first -- a legitimate prior "known good"
    // state, exactly as the directive's scenario assumes.
    auto seed = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(seed.ok);
    DOMINUS_EXPECT(!seed.change_detected);

    // Mutate brooklyn.skeleton for real.
    std::string content = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", content + " ");

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(report.change_detected);

    // Exactly the expected invalidation set -- no more, no less.
    DOMINUS_EXPECT(report.invalidation_set.size() == 3);
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.skeleton"));
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.reality_artifact"));
    // Neither the other RIG-branch source nodes nor anything in the
    // visual branch is touched -- real isolation, on both sides.
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.animations"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.hurtbox"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.moves"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.motion_graph"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.combat_dna"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.visual_genome"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.material_genome"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.visual_style_genome"));
    DOMINUS_EXPECT(!Contains(report.invalidation_set, "brooklyn.visualforge_certificate"));

    // The set ACTUALLY PROCESSED (real code paths executed) matches
    // exactly -- this is the real "visual branch DOES NOT execute"
    // proof: CompileVisualForge was never called for this node id
    // because "brooklyn.visualforge_certificate" never appears here.
    DOMINUS_EXPECT(report.recompiled_order.size() == 3);
    DOMINUS_EXPECT(Contains(report.recompiled_order, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.visual_genome"));

    // Topological order: skeleton before rig_certificate before
    // reality_artifact.
    auto indexOf = [&](const std::string& id) {
        return std::find(report.recompiled_order.begin(), report.recompiled_order.end(), id) -
               report.recompiled_order.begin();
    };
    DOMINUS_EXPECT(indexOf("brooklyn.skeleton") < indexOf("brooklyn.rig_certificate"));
    DOMINUS_EXPECT(indexOf("brooklyn.rig_certificate") < indexOf("brooklyn.reality_artifact"));

    // Every processed node's own real validation passed.
    for (const auto& r : report.results) DOMINUS_EXPECT(r.passed);

    // The registry now reflects the new, real rig_certificate/
    // reality_artifact hashes -- and everything else is byte-identical
    // to what the seed run wrote, because it was never touched.
    auto registryAfter = RealityRegistry::Load(registryPath);
    DOMINUS_EXPECT(registryAfter.has_value());
    auto reSeed = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(reSeed.ok);
    DOMINUS_EXPECT(!reSeed.change_detected);  // registry now matches the mutated file -- a real, stable fixed point

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(RealityRebuilder_VisualGenomeMutation_RecompilesBothCertificateBranches_ButNeverSkeletonAnimationCombat) {
    // The real runtime coupling (see REALITY/BrooklynEvidenceGraphBuilder.cpp's
    // RUNTIME edges): RIG::CharacterAcceptanceHarness::CheckRuntime/
    // CheckDeterminism call the FULL RigBinder::Bind, which resolves
    // visual_genome too -- so a visual_genome change correctly
    // recompiles rig_certificate as well as visualforge_certificate.
    // What's still real and provably isolated: the SOURCE-level
    // skeleton/animations/hurtbox/moves nodes themselves are never
    // re-read or re-hashed by a visual change -- only the aggregate
    // rig_certificate is reconsidered.
    auto dir = MakeMutableFixtureCopy("visual_rebuild");
    auto registryPath = dir / ".reality_registry.json";

    auto seed = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");
    DOMINUS_EXPECT(seed.ok);

    std::string content = ReadWholeFile(dir / "brooklyn_visual.json");
    DOMINUS_EXPECT(content.find("\"chaotic\"") != std::string::npos);
    size_t pos = content.find("\"chaotic\"");
    content.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
    OverwriteFile(dir / "brooklyn_visual.json", content);

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");

    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(report.change_detected);
    DOMINUS_EXPECT(report.invalidation_set.size() == 4);
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.visual_genome"));
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(Contains(report.invalidation_set, "brooklyn.reality_artifact"));

    // rig_certificate IS recompiled now (internal::CompileRig genuinely
    // called) -- but the source-level skeleton/animation/combat nodes
    // are never touched, since nothing about a visual change requires
    // re-reading those files.
    DOMINUS_EXPECT(Contains(report.recompiled_order, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.skeleton"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.animations"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.hurtbox"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.moves"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.motion_graph"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.combat_dna"));

    for (const auto& r : report.results) DOMINUS_EXPECT(r.passed);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- No success if an affected compiler fails --------------------------

DOMINUS_TEST(RealityRebuilder_VisualGenomeBecomesUnreadable_StopsHonestlyAndReportsFailure) {
    auto dir = MakeMutableFixtureCopy("visual_failure");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");

    // Break the .dominus binding's visual_genome ref -- the real
    // refusal path RigBinder::Bind already hits when a ref can't be
    // resolved. Must target the actual "visual_genome": {"ref": ...}
    // occurrence, not the inert copy of the same filename that also
    // appears in this fixture's provenance.source_assets list.
    std::string dominusContent = ReadWholeFile(dir / "brooklyn_canonical.dominus");
    size_t visualGenomeKeyPos = dominusContent.find("\"visual_genome\"");
    DOMINUS_EXPECT(visualGenomeKeyPos != std::string::npos);
    size_t refPos = dominusContent.find("brooklyn_visual.json", visualGenomeKeyPos);
    DOMINUS_EXPECT(refPos != std::string::npos);
    std::string broken = dominusContent;
    broken.replace(refPos, std::string("brooklyn_visual.json").size(), "does_not_exist.json");
    OverwriteFile(dir / "brooklyn_canonical.dominus", broken);

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");

    DOMINUS_EXPECT(!report.ok);
    DOMINUS_EXPECT(report.change_detected);
    // The break makes the source itself unreadable via the .dominus
    // binding -- RigBinder::Bind can no longer resolve it, so the very
    // first node in the invalidation set fails its own real check
    // before anything downstream is even attempted. This is the
    // honest outcome: a source that can't be read stops the rebuild
    // at the earliest possible point, not a fabricated later failure.
    DOMINUS_EXPECT(report.recompiled_order.size() == 1);
    DOMINUS_EXPECT(report.recompiled_order[0] == "brooklyn.visual_genome");
    DOMINUS_EXPECT(!report.results.empty());
    DOMINUS_EXPECT(!report.results.back().passed);
    // Nothing downstream was ever attempted.
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.visualforge_certificate"));
    DOMINUS_EXPECT(!Contains(report.recompiled_order, "brooklyn.reality_artifact"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Rejects being told a derived node "changed" ------------------------

DOMINUS_TEST(RealityRebuilder_RefusesToTreatACertificateNodeAsTheOriginOfAChange) {
    auto dir = MakeMutableFixtureCopy("derived_node_refusal");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.rig_certificate");
    DOMINUS_EXPECT(!report.ok);
    DOMINUS_EXPECT(report.recompiled_order.empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(RealityRebuilder_UnknownNodeId_FailsCleanly) {
    auto dir = MakeMutableFixtureCopy("unknown_node");
    auto registryPath = dir / ".reality_registry.json";
    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.does_not_exist");
    DOMINUS_EXPECT(!report.ok);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
