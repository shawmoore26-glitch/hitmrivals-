// tests/reality/test_reality_watcher.cpp
// Milestone 11: Reality Watcher. Every test here uses REAL inotify, a
// REAL background thread, and REAL file writes -- no mocked event
// source. This is the first genuinely end-to-end autonomous loop this
// codebase has: edit reality -> DOMINUS notices -> DOMINUS proves what
// changed -> DOMINUS determines consequences -> DOMINUS rebuilds ->
// DOMINUS persists the new reality.
#include "REALITY/RealityWatcher.h"
#include "REALITY/RealityRebuilder.h"
#include "tests/TestFramework.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

using dominus::reality::RealityRebuilder;
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
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_watcher_" + testName);
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

// A real save: open, write, close -- triggers a real IN_CLOSE_WRITE.
void RealSave(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

// Bounded, polling wait for the watcher to have processed at least
// `minCount` real events -- no fixed sleep pretending to be a
// synchronization primitive. Real async systems get a real bounded
// wait, not a guessed delay.
bool WaitForHistorySize(const RealityWatcher& watcher, size_t minCount,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (watcher.History().size() >= minCount) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return watcher.History().size() >= minCount;
}

}  // namespace

// --- The core proof: save -> observe -> normalize -> verify -> impact
//     -> rebuild -> commit, all real ------------------------------------

DOMINUS_TEST(RealityWatcher_RealSaveTriggersRealRebuildAndAtomicCommit) {
    auto dir = MakeMutableFixtureCopy("core_loop");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // baseline
    std::string beforeRegistry = ReadWholeFile(registryPath);

    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // let Run() reach poll()

    // Edit reality -- a real file save.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    RealSave(dir / "brooklyn_canonical.skel.json", skel + " ");

    DOMINUS_EXPECT(WaitForHistorySize(watcher, 1));
    watcher.Stop();
    runner.join();

    DOMINUS_EXPECT(watcher.History().size() >= 1);
    const auto& report = watcher.History().front();
    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(report.changed_node_id == "brooklyn.skeleton");

    // DOMINUS persisted the new reality -- confirmed on disk, not just
    // in the in-memory report.
    std::string afterRegistry = ReadWholeFile(registryPath);
    DOMINUS_EXPECT(afterRegistry != beforeRegistry);

    // The loop is genuinely idempotent -- asking again finds nothing
    // left pending.
    auto recheck = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(recheck.ok);
    DOMINUS_EXPECT(!recheck.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Non-authoritative files never trigger a rebuild --------------------

DOMINUS_TEST(RealityWatcher_NonAuthoritativeFileTouch_NeverTriggersARebuild) {
    auto dir = MakeMutableFixtureCopy("non_authoritative");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    std::string beforeRegistry = ReadWholeFile(registryPath);

    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // A real save to a file that is NOT a declared authoritative
    // artifact -- this must never reach RealityRebuilder.
    RealSave(dir / "some_unrelated_readme.md", "hello");

    // Give the watcher a real, bounded chance to (correctly) do
    // nothing -- there's no "success" event to wait for here, so wait
    // a fixed real interval and confirm history stayed empty.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    watcher.Stop();
    runner.join();

    DOMINUS_EXPECT(watcher.History().empty());
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == beforeRegistry);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Multiple real, sequential saves -> multiple real rebuilds --------

DOMINUS_TEST(RealityWatcher_MultipleSequentialSaves_EachProducesARealRebuild) {
    auto dir = MakeMutableFixtureCopy("sequential");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    RealSave(dir / "brooklyn_canonical.skel.json", skel + " ");
    DOMINUS_EXPECT(WaitForHistorySize(watcher, 1));

    std::string visual = ReadWholeFile(dir / "brooklyn_visual.json");
    size_t pos = visual.find("\"chaotic\"");
    DOMINUS_EXPECT(pos != std::string::npos);
    visual.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
    RealSave(dir / "brooklyn_visual.json", visual);
    DOMINUS_EXPECT(WaitForHistorySize(watcher, 2));

    watcher.Stop();
    runner.join();

    DOMINUS_EXPECT(watcher.History().size() == 2);
    std::set<std::string> nodeIds;
    for (const auto& r : watcher.History()) {
        DOMINUS_EXPECT(r.ok);
        nodeIds.insert(r.changed_node_id);
    }
    DOMINUS_EXPECT(nodeIds.count("brooklyn.skeleton") == 1);
    DOMINUS_EXPECT(nodeIds.count("brooklyn.visual_genome") == 1);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Graceful shutdown ---------------------------------------------------

DOMINUS_TEST(RealityWatcher_Stop_BeforeRun_ReturnsImmediatelyWithoutHanging) {
    auto dir = MakeMutableFixtureCopy("stop_before_run");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    RealityWatcher watcher(dir, registryPath);
    watcher.Stop();  // called before Run() -- must not cause Run() to hang

    auto start = std::chrono::steady_clock::now();
    std::thread runner([&] { watcher.Run(); });
    runner.join();
    auto elapsed = std::chrono::steady_clock::now() - start;

    DOMINUS_EXPECT(elapsed < std::chrono::milliseconds(2000));  // bounded, not a hang
    DOMINUS_EXPECT(watcher.History().empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(RealityWatcher_Stop_WhileIdle_ReturnsPromptly) {
    auto dir = MakeMutableFixtureCopy("stop_while_idle");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto start = std::chrono::steady_clock::now();
    watcher.Stop();
    runner.join();
    auto elapsed = std::chrono::steady_clock::now() - start;

    DOMINUS_EXPECT(elapsed < std::chrono::milliseconds(2000));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- A real, honest failure: watching a directory that doesn't exist -

DOMINUS_TEST(RealityWatcher_NonexistentDirectory_ThrowsRatherThanSilentlyWatchingNothing) {
    bool threw = false;
    try {
        RealityWatcher watcher("/tmp/dominus_watcher_definitely_does_not_exist_xyz", "/tmp/some_registry.json");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

// --- Rapid successive saves to the same file -- proves the watcher
//     leans on ChangeEventNormalizer's own dedup rather than needing
//     its own batching policy ---------------------------------------

DOMINUS_TEST(RealityWatcher_RapidSuccessiveSavesSameFile_ConvergesToCorrectFinalState) {
    auto dir = MakeMutableFixtureCopy("rapid_saves");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    RealityWatcher watcher(dir, registryPath);
    std::thread runner([&] { watcher.Run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    // Two real, rapid saves to the same file -- whether the watcher
    // sees this as one batch or two, the final state must be correct.
    RealSave(dir / "brooklyn_canonical.skel.json", skel + " ");
    RealSave(dir / "brooklyn_canonical.skel.json", skel + "  ");

    DOMINUS_EXPECT(WaitForHistorySize(watcher, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // let any second event settle

    watcher.Stop();
    runner.join();

    for (const auto& r : watcher.History()) DOMINUS_EXPECT(r.ok);

    // Whatever happened, the registry must now correctly reflect the
    // REAL final file content -- proven by asking directly and getting
    // a clean no-op.
    auto recheck = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(recheck.ok);
    DOMINUS_EXPECT(!recheck.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
