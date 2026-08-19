// tests/reality/test_change_event_normalizer.cpp
// Milestone 10: Change Event Normalization & Transaction Boundaries.
// "OS events != Reality changes." Every scenario below is fed a real,
// noisy batch of RawFileEvents and checked against real files on
// disk -- never a synthetic in-memory graph standing in for the
// engine.
#include "REALITY/ChangeEventNormalizer.h"
#include "REALITY/RealityRebuilder.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using dominus::reality::ChangeEventNormalizer;
using dominus::reality::NormalizedChangeSet;
using dominus::reality::RawEventKind;
using dominus::reality::RawFileEvent;
using dominus::reality::RealityRebuilder;

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
    std::filesystem::path dest = std::filesystem::temp_directory_path() / ("dominus_normalizer_" + testName);
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

bool HasVerified(const NormalizedChangeSet& set, const std::string& nodeId) {
    for (const auto& v : set.verified)
        if (v.node_id == nodeId) return true;
    return false;
}

bool Contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

bool HasUnchanged(const NormalizedChangeSet& set, const std::string& nodeId) {
    for (const auto& u : set.unchanged)
        if (u.node_id == nodeId) return true;
    return false;
}

}  // namespace

// --- Duplicate events -------------------------------------------------

DOMINUS_TEST(Normalizer_DuplicateEvents_CollapseToOneVerifiedChange) {
    auto dir = MakeMutableFixtureCopy("duplicates");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    std::vector<RawFileEvent> events;
    for (int i = 0; i < 6; i++) {
        events.push_back({dir / "brooklyn_canonical.skel.json", RawEventKind::kModified});
    }

    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(changeSet.raw_events_received == 6);
    DOMINUS_EXPECT(changeSet.distinct_paths_after_dedup == 1);
    DOMINUS_EXPECT(changeSet.verified.size() == 1);
    DOMINUS_EXPECT(HasVerified(changeSet, "brooklyn.skeleton"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Rename/create/delete sequences (one logical mutation, several
//     OS-reported events, different kinds, same underlying file) -----

DOMINUS_TEST(Normalizer_RenameCreateDeleteSequence_StillCollapsesToOneChange) {
    auto dir = MakeMutableFixtureCopy("rename_sequence");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    std::string visual = ReadWholeFile(dir / "brooklyn_visual.json");
    size_t pos = visual.find("\"chaotic\"");
    visual.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
    OverwriteFile(dir / "brooklyn_visual.json", visual);

    // A typical editor's real save sequence for one logical edit:
    // delete the old inode, create a new one, then a final modify.
    std::vector<RawFileEvent> events = {
        {dir / "brooklyn_visual.json", RawEventKind::kDeleted},
        {dir / "brooklyn_visual.json", RawEventKind::kCreated},
        {dir / "brooklyn_visual.json", RawEventKind::kModified},
    };

    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(changeSet.distinct_paths_after_dedup == 1);
    DOMINUS_EXPECT(changeSet.verified.size() == 1);
    DOMINUS_EXPECT(HasVerified(changeSet, "brooklyn.visual_genome"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Temp-file / non-authoritative events ------------------------------

DOMINUS_TEST(Normalizer_TempAndUnknownFiles_AreRejectedNotSilentlyDroppedOrTrusted) {
    auto dir = MakeMutableFixtureCopy("temp_files");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    std::vector<RawFileEvent> events = {
        {dir / "brooklyn_visual.json.swp", RawEventKind::kCreated},
        {dir / ".brooklyn_visual.json~", RawEventKind::kModified},
        {dir / "some_random_readme.md", RawEventKind::kCreated},
        {"/etc/passwd", RawEventKind::kModified},  // adversarial: a real file, but not an authoritative artifact
    };

    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(changeSet.rejected.size() == 4);
    DOMINUS_EXPECT(changeSet.verified.empty());
    DOMINUS_EXPECT(changeSet.unrepresented.empty());
    for (const auto& r : changeSet.rejected) DOMINUS_EXPECT(!r.reason.empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Bursts of changes, mixed real and noise ---------------------------

DOMINUS_TEST(Normalizer_BurstOfMixedEvents_OnlyRealChangesSurviveVerification) {
    auto dir = MakeMutableFixtureCopy("burst");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    // Only skeleton actually changes; material_genome and
    // visual_style_genome are named in the burst but never touched.
    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    std::vector<RawFileEvent> events = {
        {dir / "brooklyn_canonical.skel.json", RawEventKind::kModified},
        {dir / "brooklyn_jacket_material.json", RawEventKind::kModified},   // named but unchanged
        {dir / "brooklyn_visual_style.json", RawEventKind::kModified},      // named but unchanged
        {dir / "canonical_brooklyn_move_jab.json", RawEventKind::kModified},  // named but unchanged (part of "moves")
        {dir / "does_not_exist_at_all.json", RawEventKind::kCreated},       // pure noise, unknown
    };

    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(changeSet.verified.size() == 1);
    DOMINUS_EXPECT(HasVerified(changeSet, "brooklyn.skeleton"));
    DOMINUS_EXPECT(HasUnchanged(changeSet, "brooklyn.material_genome"));
    DOMINUS_EXPECT(HasUnchanged(changeSet, "brooklyn.visual_style_genome"));
    DOMINUS_EXPECT(HasUnchanged(changeSet, "brooklyn.moves"));  // move_jab resolves into the moves group
    DOMINUS_EXPECT(changeSet.rejected.size() == 1);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- The gap this file used to document as a real limitation is now
//     closed: motion_graph/combat_dna are real, declared artifacts
//     RealityRebuilder can now genuinely act on, since it migrated
//     onto EvidenceGraph's own node vocabulary as its dependency
//     authority. What used to be reported as "unrepresented" is now a
//     real, verified, actionable change end to end. -----------------

DOMINUS_TEST(Normalizer_MotionGraphAndCombatDna_AreNowRealVerifiedActionableChanges) {
    auto dir = MakeMutableFixtureCopy("motion_graph_now_real");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");  // baseline

    std::string motionGraph = ReadWholeFile(dir / "brooklyn_motion_graph.json");
    OverwriteFile(dir / "brooklyn_motion_graph.json", motionGraph + " ");

    std::vector<RawFileEvent> events = {{dir / "brooklyn_motion_graph.json", RawEventKind::kModified}};

    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    // No longer rejected, no longer unrepresented -- a real, verified change.
    DOMINUS_EXPECT(changeSet.rejected.empty());
    DOMINUS_EXPECT(changeSet.unrepresented.empty());
    DOMINUS_EXPECT(changeSet.verified.size() == 1);
    DOMINUS_EXPECT(HasVerified(changeSet, "brooklyn.motion_graph"));

    // Driving it through the full pipeline produces a real, successful
    // rebuild -- this genuinely could not happen before this phase.
    auto reports = ChangeEventNormalizer::ProcessEvents(dir, registryPath, events);
    DOMINUS_EXPECT(reports.size() == 1);
    DOMINUS_EXPECT(reports[0].ok);
    DOMINUS_EXPECT(reports[0].changed_node_id == "brooklyn.motion_graph");
    // motion_graph feeds rig_certificate via a real RUNTIME edge
    // (RigBinder::Bind resolving MotionGraphRefComponent) -> reality_artifact.
    DOMINUS_EXPECT(Contains(reports[0].invalidation_set, "brooklyn.rig_certificate"));
    DOMINUS_EXPECT(Contains(reports[0].invalidation_set, "brooklyn.reality_artifact"));

    // A stable, real fixed point -- asking again finds nothing pending.
    auto recheckEvents = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(recheckEvents.verified.empty());
    DOMINUS_EXPECT(HasUnchanged(recheckEvents, "brooklyn.motion_graph"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Event before write finished / stale claims: verification always
//     re-reads current disk state, never trusts the event -------------

DOMINUS_TEST(Normalizer_NeverTrustsEventClaims_AlwaysReReadsCurrentState) {
    auto dir = MakeMutableFixtureCopy("stale_claims");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    // An event claims skeleton was "deleted" -- but the file is
    // actually still there, untouched. Verification must go by the
    // real, current disk state, not the event's claimed kind.
    std::vector<RawFileEvent> events = {{dir / "brooklyn_canonical.skel.json", RawEventKind::kDeleted}};
    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(changeSet.verified.empty());
    DOMINUS_EXPECT(HasUnchanged(changeSet, "brooklyn.skeleton"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// --- Full pipeline: Normalize -> ProcessEvents -> real RealityRebuilder
//     calls, reusing Milestone 4-9 completely unmodified --------------

DOMINUS_TEST(Normalizer_ProcessEvents_DrivesRealityRebuilderOnlyForVerifiedChanges) {
    auto dir = MakeMutableFixtureCopy("process_events");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");
    std::string visual = ReadWholeFile(dir / "brooklyn_visual.json");
    size_t pos = visual.find("\"chaotic\"");
    visual.replace(pos, std::string("\"chaotic\"").size(), "\"grim\"");
    OverwriteFile(dir / "brooklyn_visual.json", visual);

    std::vector<RawFileEvent> events = {
        {dir / "brooklyn_canonical.skel.json", RawEventKind::kModified},
        {dir / "brooklyn_canonical.skel.json", RawEventKind::kModified},  // duplicate
        {dir / "brooklyn_visual.json", RawEventKind::kModified},
        {dir / "an_editor_swapfile.tmp", RawEventKind::kCreated},  // noise
        {dir / "brooklyn_motion_graph.json", RawEventKind::kModified},  // real but unrepresented
    };

    auto reports = ChangeEventNormalizer::ProcessEvents(dir, registryPath, events);

    // Exactly two real rebuilds happened -- one per verified node, not
    // one per raw event (5) and not one per distinct path (4).
    DOMINUS_EXPECT(reports.size() == 2);
    for (const auto& r : reports) {
        DOMINUS_EXPECT(r.ok);
        DOMINUS_EXPECT(r.change_detected);
    }

    // The real, downstream effect actually landed -- confirmed by
    // asking RealityRebuilder directly, the same way any other test in
    // this codebase verifies persisted state.
    auto recheckSkeleton = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    DOMINUS_EXPECT(recheckSkeleton.ok);
    DOMINUS_EXPECT(!recheckSkeleton.change_detected);
    auto recheckVisual = RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.visual_genome");
    DOMINUS_EXPECT(recheckVisual.ok);
    DOMINUS_EXPECT(!recheckVisual.change_detected);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(Normalizer_EmptyEventBatch_ProducesEmptyChangeSet_NeverAFabricatedChange) {
    auto dir = MakeMutableFixtureCopy("empty_batch");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");

    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, {});
    DOMINUS_EXPECT(changeSet.raw_events_received == 0);
    DOMINUS_EXPECT(changeSet.distinct_paths_after_dedup == 0);
    DOMINUS_EXPECT(changeSet.verified.empty());
    DOMINUS_EXPECT(changeSet.rejected.empty());
    DOMINUS_EXPECT(changeSet.unrepresented.empty());

    auto reports = ChangeEventNormalizer::ProcessEvents(dir, registryPath, {});
    DOMINUS_EXPECT(reports.empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

DOMINUS_TEST(Normalizer_Normalize_IsReadOnly_NeverTouchesTheRegistry) {
    auto dir = MakeMutableFixtureCopy("read_only");
    auto registryPath = dir / ".reality_registry.json";
    RealityRebuilder::RebuildFromChange(dir, registryPath, "brooklyn.skeleton");
    std::string beforeContent = ReadWholeFile(registryPath);

    std::string skel = ReadWholeFile(dir / "brooklyn_canonical.skel.json");
    OverwriteFile(dir / "brooklyn_canonical.skel.json", skel + " ");

    std::vector<RawFileEvent> events = {{dir / "brooklyn_canonical.skel.json", RawEventKind::kModified}};
    auto changeSet = ChangeEventNormalizer::Normalize(dir, registryPath, events);
    DOMINUS_EXPECT(!changeSet.verified.empty());

    // Normalize() alone must never write -- only ProcessEvents (via
    // RealityRebuilder) does.
    DOMINUS_EXPECT(ReadWholeFile(registryPath) == beforeContent);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
