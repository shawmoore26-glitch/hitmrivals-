// tests/validation/test_build_pipeline.cpp
#include "VALIDATION/BuildPipeline.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::validation::BuildPipeline;

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

std::filesystem::path TempDir(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}
}  // namespace

DOMINUS_TEST(BuildPipeline_EmptyProjectPassesWithZeroFiles) {
    auto dir = TempDir("dominus_build_empty_test");
    auto report = BuildPipeline::Run(dir);
    DOMINUS_EXPECT(report.results.empty());
    DOMINUS_EXPECT(report.AllPassed());
    std::filesystem::remove_all(dir);
}

DOMINUS_TEST(BuildPipeline_NonexistentDirectoryReportsZeroFilesNotACrash) {
    auto report = BuildPipeline::Run("/tmp/dominus_this_project_does_not_exist_12345");
    DOMINUS_EXPECT(report.results.empty());
    DOMINUS_EXPECT(report.AllPassed());  // nothing to fail
}

DOMINUS_TEST(BuildPipeline_RealFixtureDirectoryFindsKnownBrokenFiles) {
    // The real proof: running against the engine's own fixtures
    // directory, which is known to contain both valid entities
    // (brooklyn.dominus) and deliberately broken ones
    // (broken_missing_skeleton.dominus, missing_identity.dominus).
    auto report = BuildPipeline::Run(FixtureDir());
    DOMINUS_EXPECT(report.results.size() >= 5);  // at least brooklyn, flare_stalker, impossible_beast, + broken ones
    DOMINUS_EXPECT(!report.AllPassed());
    DOMINUS_EXPECT(report.FailedCount() >= 2);

    bool foundBrooklynPass = false;
    bool foundBrokenSkeletonFail = false;
    for (auto& r : report.results) {
        if (r.file_path.find("brooklyn.dominus") != std::string::npos) {
            DOMINUS_EXPECT(r.passed);
            foundBrooklynPass = true;
        }
        if (r.file_path.find("broken_missing_skeleton.dominus") != std::string::npos) {
            DOMINUS_EXPECT(!r.passed);
            DOMINUS_EXPECT(!r.error_messages.empty());
            foundBrokenSkeletonFail = true;
        }
    }
    DOMINUS_EXPECT(foundBrooklynPass);
    DOMINUS_EXPECT(foundBrokenSkeletonFail);
}

DOMINUS_TEST(BuildPipeline_CleanedProjectPassesCompletely) {
    // Copy the entire real fixtures directory MINUS the two files known
    // to be deliberately broken -- proving the pipeline isn't hardcoded
    // to always find a failure, and correctly reports BUILD PASSED when
    // every .dominus file it finds genuinely is valid.
    auto dir = TempDir("dominus_build_clean_test");
    auto fixtureDir = FixtureDir();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(fixtureDir)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name == "broken_missing_skeleton.dominus" || name == "missing_identity.dominus") continue;
        std::filesystem::copy_file(entry.path(), dir / name, std::filesystem::copy_options::overwrite_existing, ec);
    }

    auto report = BuildPipeline::Run(dir);
    DOMINUS_EXPECT(report.results.size() >= 4);  // brooklyn, flare_stalker, generic_biped, ik_test_rig, impossible_beast
    DOMINUS_EXPECT(report.AllPassed());

    std::filesystem::remove_all(dir);
}
