// tests/reality/test_concurrent_registry_safety.cpp
// Milestone 9: Concurrent Registry Safety. "What happens when two
// independent Dominus processes attempt to rebuild and persist the
// same reality simultaneously?"
//
// The behavior was established BEFORE it was fixed, empirically, not
// assumed: two real std::threads (each opening their OWN file
// descriptor -- flock() locks are tied to the open file description,
// not the process, so this genuinely exercises the same OS-level
// contention two real separate processes would hit) racing
// RealityRebuilder::RebuildFromChange against the same registry, with
// DIFFERENT independent branches, produced a lost update in 40/40
// real trials. The mechanism: each call loaded the registry once,
// touched only its own branch's keys in memory, and whichever call's
// Save() landed last overwrote the other's already-persisted update
// with its own stale, load-time copy of those keys.
//
// The fix (REALITY/FileLock.h, a real POSIX flock()-based exclusive
// lock) wraps the entire Load -> compute -> Save cycle, with Load()
// happening fresh, INSIDE the lock. Re-running the identical 40-trial
// reproduction after the fix: 0/40 mismatches.
//
// The contract chosen, per explicit direction: CONFLICT -> REJECT, not
// last-write-wins. A caller that cannot acquire the lock within a
// bounded timeout refuses outright (`lock_contention=true`) rather
// than proceeding unsynchronized. In this specific domain (a build/
// compile system where the correct final state is always a pure
// function of the current files on disk, never two competing valid
// answers), "conflict" concretely means lock contention -- there is no
// scenario here where two DIFFERENT final states could both be
// legitimately correct for the same file state, so REJECT is
// implemented as "refuse to proceed without exclusive access," not as
// "detect two divergent commits and pick neither."
#include "REALITY/FileLock.h"
#include "REALITY/RealityRebuilder.h"
#include "REALITY/RealityRegistry.h"
#include "CORE/Serialization/MiniJson.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

using dominus::reality::FileLock;
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
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_concurrency_" + testName);
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

bool IsValidJson(const std::string& text) {
    if (text.empty()) return false;
    try {
        auto v = dominus::core::json::Value::Parse(text);
        return v.IsObject();
    } catch (...) {
        return false;
    }
}

}  // namespace

// =====================================================================
// 1 & 2. Two processes racing the same registry, same target
// =====================================================================

DOMINUS_TEST(Concurrency_SameTarget_BothThreadsSucceed_SecondFindsNoOpOrAppliesReal) {
    auto dir = MakeMutableFixtureCopy("same_target");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // baseline

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    RebuildReport a, b;
    std::thread th1([&] { a = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); });
    std::thread th2([&] { b = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); });
    th1.join();
    th2.join();

    // Both racing for the SAME target must both end up ok -- whichever
    // gets the lock first does the real work; the other, once it gets
    // the lock, re-Loads FRESH and correctly finds no change left to
    // make (the first one already made it). Neither corrupts the
    // registry or falsely reports failure.
    DOMINUS_EXPECT(a.ok);
    DOMINUS_EXPECT(b.ok);
    DOMINUS_EXPECT(!a.lock_contention);
    DOMINUS_EXPECT(!b.lock_contention);

    auto registry = RealityRegistry::Load(registryPath);
    DOMINUS_EXPECT(registry.has_value());
    DOMINUS_EXPECT(IsValidJson(ReadWholeFile(registryPath)));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// 3. Different mutations, overlapping downstream target (skeleton and
//    visual_genome both feed rig_certificate/reality_artifact)
// =====================================================================

DOMINUS_TEST(Concurrency_DifferentBranches_OverlappingDownstream_BothPersist_NoLostUpdate) {
    // The exact scenario that was empirically 40/40 broken before the
    // fix. Re-proven here as a permanent regression test.
    auto dir = MakeMutableFixtureCopy("different_branches");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    ApplyBothMutations(dir);

    RebuildReport a, b;
    std::thread th1([&] { a = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); });
    std::thread th2([&] { b = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome"); });
    th1.join();
    th2.join();

    DOMINUS_EXPECT(a.ok);
    DOMINUS_EXPECT(b.ok);

    // Ground truth: the same two mutations, rebuilt sequentially on a
    // separate, unraced copy.
    auto seqDir = MakeMutableFixtureCopy("different_branches_seq");
    auto seqRegistry = seqDir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(seqDir, seqRegistry, "brooklyn.skeleton");
    ApplyBothMutations(seqDir);
    RealityRebuilder::RebuildFromChange(seqDir, seqRegistry, "brooklyn.skeleton");
    RealityRebuilder::RebuildFromChange(seqDir, seqRegistry, "brooklyn.visual_genome");

    DOMINUS_EXPECT(ReadWholeFile(registryPath) == ReadWholeFile(seqRegistry));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::remove_all(seqDir, ec);
}

// =====================================================================
// 4. One succeeds while the other genuinely fails
// =====================================================================

DOMINUS_TEST(Concurrency_OneSucceedsOneFails_FailureDoesNotCorruptTheSuccess) {
    auto dir = MakeMutableFixtureCopy("one_fails");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    // A real, isolated VisualForge-only failure (BlueprintValidator's
    // style_reference_matches gate -- see Milestone 5) racing against
    // a genuinely valid skeleton mutation.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");
    std::string style = ReadWholeFile(dir / "brooklyn_visual_style.json");
    size_t pos = style.find("STYLE-URBAN-COMBAT");
    DOMINUS_EXPECT(pos != std::string::npos);
    style.replace(pos, std::string("STYLE-URBAN-COMBAT").size(), "STYLE-SOMETHING-ELSE");
    OverwriteFile(dir / "brooklyn_visual_style.json", style);

    RebuildReport succeeds, fails;
    std::thread th1(
        [&] { succeeds = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); });
    std::thread th2(
        [&] { fails = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_style_genome"); });
    th1.join();
    th2.join();

    DOMINUS_EXPECT(succeeds.ok);
    DOMINUS_EXPECT(!fails.ok);
    DOMINUS_EXPECT(!fails.lock_contention);  // a real validation failure, not contention

    // The successful branch's update genuinely landed -- proven by
    // asking again and getting a clean no-op for that specific node.
    auto recheck = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(recheck.ok);
    DOMINUS_EXPECT(!recheck.change_detected);

    DOMINUS_EXPECT(IsValidJson(ReadWholeFile(registryPath)));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// 5. A "crash" during persistence -- an abandoned lock, mid-critical-
//    section, must never deadlock or corrupt a later caller
// =====================================================================

DOMINUS_TEST(Concurrency_AbandonedLock_NeverDeadlocksOrCorruptsALaterCaller) {
    auto dir = MakeMutableFixtureCopy("abandoned_lock");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    std::string beforeContent = ReadWholeFile(registryPath);

    auto lockPath = registryPath;
    lockPath += ".lock";

    // Simulate a crash mid-critical-section: acquire the lock, as a
    // real holder would, then let the FileLock go out of scope WITHOUT
    // ever writing anything -- standing in for a process that died
    // after acquiring the lock but before it finished its work. A
    // real OS-level flock() is released automatically when its file
    // descriptor closes (including on process termination), which is
    // exactly what the FileLock destructor does here.
    {
        FileLock crashedHolder(lockPath);
        DOMINUS_EXPECT(crashedHolder.Acquire(std::chrono::milliseconds(1000)));
        // ... "crash" here -- scope ends, destructor releases the lock.
    }

    // A later caller must be able to proceed normally -- no deadlock,
    // no corruption from the abandoned attempt.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");
    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(report.ok);
    DOMINUS_EXPECT(!report.lock_contention);
    DOMINUS_EXPECT(report.change_detected);
    DOMINUS_EXPECT(ReadWholeFile(registryPath) != beforeContent);  // real progress was made
    DOMINUS_EXPECT(IsValidJson(ReadWholeFile(registryPath)));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// 6. Final registry is never syntactically corrupt -- stress test
// =====================================================================

DOMINUS_TEST(Concurrency_StressTest_FinalRegistryNeverSyntacticallyCorrupt) {
    auto dir = MakeMutableFixtureCopy("stress");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    const int kRounds = 15;
    for (int i = 0; i < kRounds; i++) {
        std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
        OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");
        std::string visual = ReadWholeFile(dir / "brooklyn_visual.json");
        std::string target = (visual.find("\"chaotic\"") != std::string::npos) ? "\"chaotic\"" : "\"grim\"";
        std::string replacement = (target == "\"chaotic\"") ? "\"grim\"" : "\"chaotic\"";
        size_t pos = visual.find(target);
        visual.replace(pos, target.size(), replacement);
        OverwriteFile(dir / "brooklyn_visual.json", visual);

        std::thread th1([&] { RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); });
        std::thread th2([&] { RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome"); });
        th1.join();
        th2.join();

        DOMINUS_EXPECT(IsValidJson(ReadWholeFile(registryPath)));
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// 7. No process falsely reports success based on stale state
// =====================================================================

DOMINUS_TEST(Concurrency_LockHeldByAnotherHolder_RefusesRatherThanUsingStaleState) {
    auto dir = MakeMutableFixtureCopy("stale_refusal");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    std::string beforeContent = ReadWholeFile(registryPath);

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    auto lockPath = registryPath;
    lockPath += ".lock";
    FileLock externalHolder(lockPath);
    DOMINUS_EXPECT(externalHolder.Acquire(std::chrono::milliseconds(1000)));

    // With the lock genuinely held elsewhere, a real rebuild attempt
    // (short timeout, so this test doesn't stall) must refuse
    // outright, never proceed on a stale pre-contention snapshot.
    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton",
                                                        std::chrono::milliseconds(200));
    DOMINUS_EXPECT(!report.ok);
    DOMINUS_EXPECT(report.lock_contention);
    // Nothing was read or written -- the registry is byte-identical to
    // before this refused attempt.
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == beforeContent);

    externalHolder.Release();

    // Once released, the same call succeeds normally.
    auto retried = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(retried.ok);
    DOMINUS_EXPECT(!retried.lock_contention);
    DOMINUS_EXPECT(retried.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// 8. Final state: valid committed state OR an explicitly reported
//    conflict -- never a silent third thing
// =====================================================================

DOMINUS_TEST(Concurrency_EveryOutcome_IsEitherCommittedOrExplicitConflict_NeverSilent) {
    auto dir = MakeMutableFixtureCopy("explicit_outcomes");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    auto lockPath = registryPath;
    lockPath += ".lock";
    FileLock externalHolder(lockPath);
    DOMINUS_EXPECT(externalHolder.Acquire(std::chrono::milliseconds(1000)));

    auto report = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton",
                                                        std::chrono::milliseconds(150));
    externalHolder.Release();

    // Every RebuildReport this codebase can produce falls into exactly
    // one of two buckets: a real committed outcome (report.ok == true,
    // lock_contention == false) or an explicit, named conflict
    // (report.ok == false, and either lock_contention == true, or a
    // real, non-empty `summary` explaining a validation/consistency
    // failure). There is no third, silent state.
    DOMINUS_EXPECT(!report.summary.empty());
    if (report.ok) {
        DOMINUS_EXPECT(!report.lock_contention);
    } else {
        DOMINUS_EXPECT(report.lock_contention || !report.summary.empty());
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// 9. Re-running after a race produces deterministic convergence
// =====================================================================

DOMINUS_TEST(Concurrency_RerunAfterRace_ConvergesToAStableFixedPoint) {
    auto dir = MakeMutableFixtureCopy("convergence");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    ApplyBothMutations(dir);

    std::thread th1([&] { RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton"); });
    std::thread th2([&] { RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome"); });
    th1.join();
    th2.join();

    std::string afterRace = ReadWholeFile(registryPath);

    // Ask again, for both branches, sequentially -- a stable fixed
    // point means both report no further change, and the file doesn't
    // move.
    auto reSkeleton = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    auto reVisual = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");
    DOMINUS_EXPECT(reSkeleton.ok);
    DOMINUS_EXPECT(!reSkeleton.change_detected);
    DOMINUS_EXPECT(reVisual.ok);
    DOMINUS_EXPECT(!reVisual.change_detected);
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == afterRace);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// =====================================================================
// FileLock itself, isolated
// =====================================================================

DOMINUS_TEST(FileLock_TryAcquire_FailsWhileAnotherHolderHoldsIt) {
    auto dir = MakeMutableFixtureCopy("filelock_basic");
    auto lockPath = dir / "test.lock";

    FileLock first(lockPath);
    DOMINUS_EXPECT(first.TryAcquire());

    FileLock second(lockPath);
    DOMINUS_EXPECT(!second.TryAcquire());

    first.Release();
    DOMINUS_EXPECT(second.TryAcquire());  // free once the first releases

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(FileLock_Acquire_TimesOutRatherThanBlockingForever) {
    auto dir = MakeMutableFixtureCopy("filelock_timeout");
    auto lockPath = dir / "test.lock";

    FileLock holder(lockPath);
    DOMINUS_EXPECT(holder.Acquire());

    FileLock contender(lockPath);
    auto start = std::chrono::steady_clock::now();
    bool acquired = contender.Acquire(std::chrono::milliseconds(100));
    auto elapsed = std::chrono::steady_clock::now() - start;

    DOMINUS_EXPECT(!acquired);
    DOMINUS_EXPECT(elapsed < std::chrono::milliseconds(2000));  // bounded, not a hang

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
