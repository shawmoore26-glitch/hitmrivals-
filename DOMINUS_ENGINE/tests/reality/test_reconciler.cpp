// tests/reality/test_reconciler.cpp
// Milestone 12: Recovery / Reconciliation on restart. "Filesystem
// events are ephemeral; source state is persistent." Every test here
// simulates a real offline period -- files mutated with NO watcher
// running and NO events ever generated -- then proves reconciliation
// (a full scan, not a new authority system) recovers correctly.
#include "REALITY/Reconciler.h"
#include "REALITY/RealityRebuilder.h"
#include "REALITY/RealityRegistry.h"
#include "REALITY/RealityWatcher.h"
#include "tests/TestFramework.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

using dominus::reality::Reconciler;
using dominus::reality::RealityRebuilder;
using dominus::reality::RealityRegistry;
using dominus::reality::RealityWatcher;

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
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_reconciler_" + testName);
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

bool HasVerified(const dominus::reality::NormalizedChangeSet& set, const std::string& nodeId) {
    for (const auto& v : set.verified)
        if (v.node_id == nodeId) return true;
    return false;
}

}  // namespace

// --- Scan() is read-only, mirrors ChangeEventNormalizer's own
//     honesty about not touching the registry --------------------------

DOMINUS_TEST(Reconciler_Scan_IsReadOnly_NeverTouchesTheRegistry) {
    auto dir = MakeMutableFixtureCopy("scan_read_only");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    std::string beforeContent = ReadWholeFile(registryPath);

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    auto scanResult = Reconciler::Scan(dir, registryPath);
    DOMINUS_EXPECT(HasVerified(scanResult, "brooklyn.skeleton"));
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == beforeContent);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- The core scenario: a real offline mutation, no watcher running,
//     no event ever generated, then a real reconciliation finds and
//     fixes it correctly --------------------------------------------

DOMINUS_TEST(Reconciler_OfflineMutation_NoEventEverGenerated_ReconciliationStillFindsIt) {
    auto dir = MakeMutableFixtureCopy("offline_single");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // "watching" was active here

    // The "process crashes" -- nothing more happens, no watcher, no
    // inotify, no events. Then, entirely offline, a real file changes.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    // "Process restarts." Reconcile() is the very first thing it does.
    auto reports = Reconciler::Reconcile(dir, registryPath);
    DOMINUS_EXPECT(reports.size() == 1);
    DOMINUS_EXPECT(reports[0].ok);
    DOMINUS_EXPECT(reports[0].changed_node_id == "brooklyn.skeleton");

    // A real, stable fixed point -- asking again finds nothing left.
    auto recheck = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(recheck.ok);
    DOMINUS_EXPECT(!recheck.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Multiple independent offline mutations, all recovered in one
//     reconciliation pass ------------------------------------------------

DOMINUS_TEST(Reconciler_MultipleOfflineMutations_AllRecoveredInOnePass) {
    auto dir = MakeMutableFixtureCopy("offline_multi");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    // Two real, independent files changed while completely offline.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");
    std::string visual = ReadWholeFile(dir / "brooklyn_visual.json");
    size_t pos = visual.find("\"chaotic\"");
    DOMINUS_EXPECT(pos != std::string::npos);
    visual.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
    OverwriteFile(dir / "brooklyn_visual.json", visual);

    auto reports = Reconciler::Reconcile(dir, registryPath);
    DOMINUS_EXPECT(reports.size() == 2);
    std::set<std::string> recovered;
    for (const auto& r : reports) {
        DOMINUS_EXPECT(r.ok);
        recovered.insert(r.changed_node_id);
    }
    DOMINUS_EXPECT(recovered.count("brooklyn.skeleton") == 1);
    DOMINUS_EXPECT(recovered.count("brooklyn.visual_genome") == 1);

    // Cross-check against the sequential ground truth this codebase
    // has used since Milestone 8 -- the reconciled registry must be
    // byte-identical to a normal, live sequential rebuild of the same
    // two mutations.
    auto seqDir = MakeMutableFixtureCopy("offline_multi_seq");
    auto seqRegistry = seqDir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(seqDir, seqRegistry, "brooklyn.skeleton");
    std::string seqSkel = ReadWholeFile(seqDir / "brooklyn_canonical.skel.json");
    OverwriteFile(seqDir / "brooklyn_canonical.skel.json", seqSkel + " ");
    std::string seqVisual = ReadWholeFile(seqDir / "brooklyn_visual.json");
    size_t seqPos = seqVisual.find("\"chaotic\"");
    seqVisual.replace(seqPos, std::string("\"chaotic\"").size(), "\"grim\"");
    OverwriteFile(seqDir / "brooklyn_visual.json", seqVisual);
    RealityRebuilder::RebuildFromChange(seqDir, seqRegistry, "brooklyn.skeleton");
    RealityRebuilder::RebuildFromChange(seqDir, seqRegistry, "brooklyn.visual_genome");

    DOMINUS_EXPECT(ReadWholeFile(registryPath) == ReadWholeFile(seqRegistry));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::remove_all(seqDir, ec);
}

// --- No offline mutation at all: reconciliation is a real, honest
//     no-op ------------------------------------------------------------

DOMINUS_TEST(Reconciler_NoOfflineMutation_ProducesNoRebuilds) {
    auto dir = MakeMutableFixtureCopy("no_mutation");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    std::string beforeContent = ReadWholeFile(registryPath);

    auto reports = Reconciler::Reconcile(dir, registryPath);
    DOMINUS_EXPECT(reports.empty());
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == beforeContent);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- No registry at all yet: reconciliation on a totally fresh
//     directory falls through to RealityRebuilder's own real
//     bootstrap path, unmodified ----------------------------------------

DOMINUS_TEST(Reconciler_NoRegistryYet_FallsThroughToRealBootstrap) {
    auto dir = MakeMutableFixtureCopy("no_registry");
    auto registryPath = dir / ".reality_registry.json";
    std::error_code ec;
    DOMINUS_EXPECT(!std::filesystem::exists(registryPath));

    auto reports = Reconciler::Reconcile(dir, registryPath);
    // Every declared source artifact is "changed" relative to a
    // nonexistent registry -- each drives its own real
    // RebuildFromChange call, and the first one bootstraps the full
    // real baseline (Milestone 4's own, unmodified logic).
    DOMINUS_EXPECT(!reports.empty());
    for (const auto& r : reports) DOMINUS_EXPECT(r.ok);
    DOMINUS_EXPECT(std::filesystem::exists(registryPath));

    std::filesystem::remove_all(dir, ec);
}

// --- The architecture is protected: files remain authoritative, the
//     registry remains derived evidence -- reconciliation NEVER
//     invents a change that isn't a real, verified hash difference ---

DOMINUS_TEST(Reconciler_NeverFabricatesAChange_OnlyRealVerifiedDifferences) {
    auto dir = MakeMutableFixtureCopy("no_fabrication");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    // Touch a file's mtime without changing its content -- a real,
    // common false-positive trap for anything that trusts timestamps
    // instead of content hashes.
    auto skelPath = dir / "brooklyn_canonical.skel.json";
    std::string content = ReadWholeFile(skelPath);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    OverwriteFile(skelPath, content);  // byte-identical rewrite

    auto reports = Reconciler::Reconcile(dir, registryPath);
    DOMINUS_EXPECT(reports.empty());  // no real content difference -- no fabricated rebuild

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Full integration: RealityWatcher's own restart-time
//     reconciliation, with a real offline mutation and NO event
//     source involved at all before Run() starts ------------------------

DOMINUS_TEST(RealityWatcher_RestartAfterOfflineMutation_ReconcilesBeforeWatchingResumes) {
    auto dir = MakeMutableFixtureCopy("watcher_restart");
    auto registryPath = dir / ".reality_registry.json";

    // "First session": DOMINUS was watching, established a baseline.
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    // "DOMINUS WATCHER process crashes" -- no watcher object exists at
    // all right now. While it's gone, a real file changes, completely
    // offline.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    // "restart": a brand new RealityWatcher, Run() with its default
    // reconcileOnStart=true.
    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(); });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (watcher.History().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    watcher.Stop();
    runner.join();

    // The offline change was recovered via reconciliation -- BEFORE
    // any live event could possibly have fired, since nothing touched
    // the file after the watcher started.
    DOMINUS_EXPECT(!watcher.History().empty());
    DOMINUS_EXPECT(watcher.History().front().ok);
    DOMINUS_EXPECT(watcher.History().front().changed_node_id == "brooklyn.skeleton");

    auto recheck = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(recheck.ok);
    DOMINUS_EXPECT(!recheck.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(RealityWatcher_ReconcileOnStartFalse_SkipsReconciliation) {
    auto dir = MakeMutableFixtureCopy("watcher_no_reconcile");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");
    std::string registryBefore = ReadWholeFile(registryPath);

    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(/*reconcileOnStart=*/false); });
    std::this_thread::sleep_for(std::chrono::milliseconds(400));  // real, bounded wait -- nothing SHOULD happen

    watcher.Stop();
    runner.join();

    // With reconciliation explicitly disabled, the offline change is
    // never recovered by Run() alone -- proving reconciliation is a
    // real, optional, separate step, not baked unconditionally into
    // the event loop.
    DOMINUS_EXPECT(watcher.History().empty());
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == registryBefore);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
