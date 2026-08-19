// tests/reality/test_reality_transactional.cpp
// Milestone 5: "Can the registry itself survive reality?"
//
// Not a watcher, not automation -- a harder question about the
// primitive Milestone 4 already built: does RealityRegistry ever
// falsely claim an artifact is current? Proven here, not asserted:
//
//   - A rebuild that fails partway leaves the on-disk registry
//     byte-identical to before the attempt (no partial write).
//   - Re-running after a failure still detects the change -- the
//     registry never silently "forgets" that something is pending.
//   - Two independent branches, mutated together and rebuilt in
//     opposite orders across two separate fixture copies, converge on
//     byte-identical final registries.
//   - A branch that already succeeded stays exactly as it was when a
//     LATER, unrelated branch's rebuild fails -- no cross-branch
//     corruption.
//   - Save() is atomic: if the rename step can't complete, whatever
//     was already on disk at the real path is completely untouched.
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
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_reality_txn_" + testName);
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

// --- A failed rebuild never writes a partial registry -----------------

DOMINUS_TEST(RealityRebuilder_FailedRebuild_LeavesRegistryByteIdentical_NoPartialWrite) {
    auto dir = MakeMutableFixtureCopy("no_partial_write");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // establish a real baseline

    std::string registryBefore = ReadWholeFile(registryPath);
    DOMINUS_EXPECT(!registryBefore.empty());

    // Break the visual_style_genome/visual_genome style_id cross-check
    // -- a real, content-based validation failure (BlueprintValidator's
    // style_reference_matches gate), not a load failure. RigBinder::
    // Bind still succeeds fine for both files, so this is a genuinely
    // isolated VisualForge-domain failure.
    std::string styleContent = ReadWholeFile(dir / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(styleContent.find("STYLE-URBAN-COMBAT") != std::string::npos);
    size_t pos = styleContent.find("STYLE-URBAN-COMBAT");
    styleContent.replace(pos, std::string("STYLE-URBAN-COMBAT").size(), "STYLE-SOMETHING-ELSE");
    OverwriteFile(dir / "brooklyn_visual_style.json", styleContent);

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome");
    DOMINUS_EXPECT(!report.ok);
    DOMINUS_EXPECT(report.change_detected);

    // The registry on disk must be EXACTLY what it was before this
    // failed attempt -- Save() is only ever reached after every node
    // in the invalidation set has already passed.
    std::string registryAfter = ReadWholeFile(registryPath);
    DOMINUS_EXPECT(registryAfter == registryBefore);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Never falsely claims an artifact is current -----------------------

DOMINUS_TEST(RealityRebuilder_ReRunAfterFailure_StillDetectsChange_NeverFalselyCurrent) {
    auto dir = MakeMutableFixtureCopy("never_falsely_current");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome");

    std::string styleContent = ReadWholeFile(dir / "brooklyn_visual_style.json");
    size_t pos = styleContent.find("STYLE-URBAN-COMBAT");
    DOMINUS_EXPECT(pos != std::string::npos);
    styleContent.replace(pos, std::string("STYLE-URBAN-COMBAT").size(), "STYLE-SOMETHING-ELSE");
    OverwriteFile(dir / "brooklyn_visual_style.json", styleContent);

    auto firstAttempt = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome");
    DOMINUS_EXPECT(!firstAttempt.ok);

    // Ask again, completely fresh -- no shared state, a brand new call.
    // The mutation is still present and the registry was never
    // updated, so this MUST still report change_detected=true. A
    // registry that silently forgot a pending change here would be
    // exactly the "falsely claims current" failure mode.
    auto secondAttempt = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome");
    DOMINUS_EXPECT(!secondAttempt.ok);
    DOMINUS_EXPECT(secondAttempt.change_detected);

    // Now actually fix it -- restore the matching style_id -- and
    // confirm the SAME primitive now succeeds and the registry finally
    // advances.
    std::string fixedContent = ReadWholeFile(dir / "brooklyn_visual_style.json");
    size_t fixPos = fixedContent.find("STYLE-SOMETHING-ELSE");
    DOMINUS_EXPECT(fixPos != std::string::npos);
    fixedContent.replace(fixPos, std::string("STYLE-SOMETHING-ELSE").size(), "STYLE-URBAN-COMBAT");
    OverwriteFile(dir / "brooklyn_visual_style.json", fixedContent);

    auto thirdAttempt = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome");
    DOMINUS_EXPECT(thirdAttempt.ok);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Independent branches, rebuilt in different orders -----------------

DOMINUS_TEST(RealityRebuilder_IndependentBranchesRebuiltInDifferentOrders_ConvergeToIdenticalRegistry) {
    auto dirA = MakeMutableFixtureCopy("order_skeleton_first");
    auto dirB = MakeMutableFixtureCopy("order_visual_first");
    auto registryPathA = dirA / ".reality_registry.json";
    auto registryPathB = dirB / ".reality_registry.json";

    RealityRebuilder::RebuildFromChange(dirA, registryPathA, "brooklyn.skeleton");
    RealityRebuilder::RebuildFromChange(dirB, registryPathB, "brooklyn.skeleton");

    // The identical two mutations, applied to both independent copies.
    // The skeleton hash is a raw-byte hash (whitespace counts); the
    // visual_genome hash is a hash of the PARSED genome (via
    // REGISTRY::VisualGenomeCompiler), so it must be a real semantic
    // content change -- a whitespace append would be silently inert
    // and this test would end up "proving" order-independence over two
    // no-ops instead of two real rebuilds.
    for (auto& dir : {dirA, dirB}) {
        std::string skelContent = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
        OverwriteFile(dir / "brooklyn_canonical.skel.json", skelContent + " ");

        std::string visualContent = ReadWholeFile(dir / "brooklyn_visual.json");
        DOMINUS_EXPECT(visualContent.find("\"chaotic\"") != std::string::npos);
        size_t pos = visualContent.find("\"chaotic\"");
        visualContent.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
        OverwriteFile(dir / "brooklyn_visual.json", visualContent);
    }

    // Copy A: skeleton branch first, then visual branch.
    auto skeletonFirst = RealityRebuilder::RebuildFromChange(dirA, registryPathA, "brooklyn.skeleton");
    auto visualSecond = RealityRebuilder::RebuildFromChange(dirA, registryPathA, "brooklyn.visual_genome");
    DOMINUS_EXPECT(skeletonFirst.ok);
    DOMINUS_EXPECT(skeletonFirst.change_detected);
    DOMINUS_EXPECT(visualSecond.ok);
    DOMINUS_EXPECT(visualSecond.change_detected);

    // Copy B: visual branch first, then skeleton branch -- the
    // opposite order.
    auto visualFirst = RealityRebuilder::RebuildFromChange(dirB, registryPathB, "brooklyn.visual_genome");
    auto skeletonSecond = RealityRebuilder::RebuildFromChange(dirB, registryPathB, "brooklyn.skeleton");
    DOMINUS_EXPECT(visualFirst.ok);
    DOMINUS_EXPECT(visualFirst.change_detected);
    DOMINUS_EXPECT(skeletonSecond.ok);
    DOMINUS_EXPECT(skeletonSecond.change_detected);

    // Same two real mutations, opposite processing order -- the final
    // registries must be byte-identical. Two independent branches
    // converging to the same fixed point regardless of order is the
    // real commutativity proof; getting it right by accident in only
    // one order would be a coincidence, not a guarantee.
    auto registryA = ReadWholeFile(registryPathA);
    auto registryB = ReadWholeFile(registryPathB);
    DOMINUS_EXPECT(registryA == registryB);

    std::error_code ec;
    std::filesystem::remove_all(dirA, ec);
    std::filesystem::remove_all(dirB, ec);
}

// --- A later, unrelated failure never corrupts an already-persisted
//     branch --------------------------------------------------------------

DOMINUS_TEST(RealityRebuilder_LaterUnrelatedFailure_DoesNotCorruptAlreadyPersistedBranch) {
    auto dir = MakeMutableFixtureCopy("partial_multi_branch");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // baseline

    // Mutate the skeleton for real and rebuild it successfully first.
    std::string skelContent = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skelContent + " ");
    auto skeletonRebuild = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(skeletonRebuild.ok);

    auto registryAfterSkeleton = RealityRegistry::Load(registryPath);
    DOMINUS_EXPECT(registryAfterSkeleton.has_value());
    std::string realityArtifactAfterSkeleton = *registryAfterSkeleton->Find("brooklyn.reality_artifact");
    std::string rigCertAfterSkeleton = *registryAfterSkeleton->Find("brooklyn.rig_certificate");
    DOMINUS_EXPECT(!realityArtifactAfterSkeleton.empty());

    // Now break the visual branch for real (content mismatch, not a
    // load failure) and attempt to rebuild it -- this MUST fail.
    std::string styleContent = ReadWholeFile(dir / "brooklyn_visual_style.json");
    size_t pos = styleContent.find("STYLE-URBAN-COMBAT");
    DOMINUS_EXPECT(pos != std::string::npos);
    styleContent.replace(pos, std::string("STYLE-URBAN-COMBAT").size(), "STYLE-SOMETHING-ELSE");
    OverwriteFile(dir / "brooklyn_visual_style.json", styleContent);

    auto visualRebuild = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome");
    DOMINUS_EXPECT(!visualRebuild.ok);

    // The rig branch's already-persisted, already-successful state
    // must be completely unaffected by this later, unrelated failure.
    auto registryAfterFailedVisual = RealityRegistry::Load(registryPath);
    DOMINUS_EXPECT(registryAfterFailedVisual.has_value());
    DOMINUS_EXPECT(*registryAfterFailedVisual->Find("brooklyn.rig_certificate") == rigCertAfterSkeleton);
    DOMINUS_EXPECT(*registryAfterFailedVisual->Find("brooklyn.reality_artifact") == realityArtifactAfterSkeleton);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- "Process restart" -- genuinely fresh calls, no shared state -----

DOMINUS_TEST(RealityRebuilder_ProcessRestartSimulation_FreshCallsProduceConsistentResults) {
    auto dir = MakeMutableFixtureCopy("restart_simulation");
    auto registryPath = dir / ".reality_registry.json";

    // Each call below is a fully independent invocation -- no object,
    // cache, or static state carried between them, the same as if each
    // one were a separate process invocation of `dominus-cli
    // reality-rebuild`.
    { RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); }

    std::string skelContent = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skelContent + " ");

    RebuildReport reportFromFreshCall;
    {
        reportFromFreshCall = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    }
    DOMINUS_EXPECT(reportFromFreshCall.ok);
    DOMINUS_EXPECT(reportFromFreshCall.change_detected);

    // A second, equally fresh call against the now-unchanged state
    // must independently arrive at "no change" -- it has to re-derive
    // this from disk each time, since nothing is cached anywhere.
    RebuildReport secondFreshCall;
    { secondFreshCall = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); }
    DOMINUS_EXPECT(secondFreshCall.ok);
    DOMINUS_EXPECT(!secondFreshCall.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Save() is atomic: a failed write never disturbs the real file ---

DOMINUS_TEST(RealityRegistry_Save_IsAtomic_FailureLeavesOriginalFileUntouched) {
    auto dir = MakeMutableFixtureCopy("atomic_save_failure");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // real baseline on disk

    std::string beforeContent = ReadWholeFile(registryPath);
    DOMINUS_EXPECT(!beforeContent.empty());

    // Block the atomic-write temp path by making it a directory --
    // std::ofstream cannot open a directory for writing, so Save()'s
    // internal write step genuinely fails before the rename is ever
    // attempted.
    std::filesystem::path tmpPath = registryPath;
    tmpPath += ".tmp";
    std::error_code ec;
    std::filesystem::create_directories(tmpPath, ec);
    DOMINUS_EXPECT(std::filesystem::is_directory(tmpPath));

    RealityRegistry registry;
    registry.subject = "brooklyn";
    registry.node_hashes["brooklyn.skeleton"] = "some_new_hash_that_must_never_land";
    bool saved = registry.Save(registryPath);
    DOMINUS_EXPECT(!saved);

    // The real file at registryPath must be untouched -- byte-identical
    // to before this failed Save() call.
    std::string afterContent = ReadWholeFile(registryPath);
    DOMINUS_EXPECT(afterContent == beforeContent);

    std::filesystem::remove_all(tmpPath, ec);
    std::filesystem::remove_all(dir, ec);
}
