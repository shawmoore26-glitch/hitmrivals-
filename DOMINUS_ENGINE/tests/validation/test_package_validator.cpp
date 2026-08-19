// tests/validation/test_package_validator.cpp
#include "VALIDATION/PackageValidator.h"

#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::AnimationSetComponent;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatIdentityComponent;
using dominus::combat::MoveLoader;
using dominus::combat::MoveSetComponent;
using dominus::core::DominusSerializer;
using dominus::core::IdentityComponent;
using dominus::core::MetaBinObject;
using dominus::core::SkeletonRefComponent;
using dominus::validation::PackageValidator;
using dominus::validation::Severity;
using dominus::validation::ValidationReport;

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

bool HasCategory(const ValidationReport& report, const std::string& category) {
    for (auto& i : report.issues) {
        if (i.category == category) return true;
    }
    return false;
}
}  // namespace

// --- ValidationReport itself -----------------------------------------------

DOMINUS_TEST(ValidationReport_PassedIsTrueWithZeroErrors) {
    ValidationReport report;
    DOMINUS_EXPECT(report.Passed());
    report.Add(Severity::kWarning, "test", "a warning does not block");
    DOMINUS_EXPECT(report.Passed());
}

DOMINUS_TEST(ValidationReport_PassedIsFalseWithAnyError) {
    ValidationReport report;
    report.Add(Severity::kError, "test", "an error blocks");
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(report.ErrorCount() == 1);
}

// --- CheckAssetOwnership -----------------------------------------------------

DOMINUS_TEST(CheckAssetOwnership_FlagsMissingFile) {
    MetaBinObject obj("test_obj", "0.1.0");
    obj.AddComponent<SkeletonRefComponent>(SkeletonRefComponent{"does_not_exist.skel.json"});
    ValidationReport report;
    PackageValidator::CheckAssetOwnership(obj, FixtureDir(), report);
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "asset_ownership"));
}

DOMINUS_TEST(CheckAssetOwnership_PassesForRealFile) {
    MetaBinObject obj("test_obj", "0.1.0");
    obj.AddComponent<SkeletonRefComponent>(SkeletonRefComponent{"brooklyn.skel.json"});
    ValidationReport report;
    PackageValidator::CheckAssetOwnership(obj, FixtureDir(), report);
    DOMINUS_EXPECT(report.Passed());
}

DOMINUS_TEST(CheckAssetOwnership_FlagsMissingSocialGenomeRef) {
    MetaBinObject obj("test_obj", "0.1.0");
    obj.AddComponent<dominus::core::SocialGenomeRefComponent>(
        dominus::core::SocialGenomeRefComponent{"does_not_exist_social.json"});
    ValidationReport report;
    PackageValidator::CheckAssetOwnership(obj, FixtureDir(), report);
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "asset_ownership"));
}

DOMINUS_TEST(CheckAssetOwnership_PassesForRealSocialGenomeRef) {
    MetaBinObject obj("test_obj", "0.1.0");
    obj.AddComponent<dominus::core::SocialGenomeRefComponent>(
        dominus::core::SocialGenomeRefComponent{"brooklyn_social.json"});
    ValidationReport report;
    PackageValidator::CheckAssetOwnership(obj, FixtureDir(), report);
    DOMINUS_EXPECT(report.Passed());
}

// --- CheckNamingConsistency --------------------------------------------------

DOMINUS_TEST(CheckNamingConsistency_FlagsInvalidObjectId) {
    MetaBinObject obj("Brooklyn-Renegade!", "0.1.0");
    ValidationReport report;
    PackageValidator::CheckNamingConsistency(obj, report);
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "naming"));
}

DOMINUS_TEST(CheckNamingConsistency_AcceptsValidSnakeCaseId) {
    MetaBinObject obj("brooklyn_renegade_01", "0.1.0");
    ValidationReport report;
    PackageValidator::CheckNamingConsistency(obj, report);
    DOMINUS_EXPECT(report.Passed());
}

DOMINUS_TEST(CheckNamingConsistency_FlagsMoveNameMismatch) {
    auto jabResult = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(jabResult.ok);

    MetaBinObject obj("valid_id", "0.1.0");
    MoveSetComponent moves;
    // Deliberately insert under a key that does NOT match the loaded
    // move's own internal name field ("jab") -- the exact latent-bug
    // class this check exists to catch.
    moves.moves.emplace("wrong_key_name", *jabResult.value);
    obj.AddComponent<MoveSetComponent>(std::move(moves));

    ValidationReport report;
    PackageValidator::CheckNamingConsistency(obj, report);
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "naming"));
}

DOMINUS_TEST(CheckNamingConsistency_PassesWhenMoveKeyMatchesInternalName) {
    auto jabResult = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    MetaBinObject obj("valid_id", "0.1.0");
    MoveSetComponent moves;
    moves.moves.emplace("jab", *jabResult.value);  // matches internal name field
    obj.AddComponent<MoveSetComponent>(std::move(moves));

    ValidationReport report;
    PackageValidator::CheckNamingConsistency(obj, report);
    DOMINUS_EXPECT(report.Passed());
}

// --- CheckIdentityCompleteness ----------------------------------------------

DOMINUS_TEST(CheckIdentityCompleteness_FlagsMissingIdentityComponent) {
    MetaBinObject obj("no_identity", "0.1.0");
    ValidationReport report;
    PackageValidator::CheckIdentityCompleteness(obj, report);
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "identity"));
}

DOMINUS_TEST(CheckIdentityCompleteness_FlagsEmptyDisplayName) {
    MetaBinObject obj("blank_name", "0.1.0");
    obj.AddComponent<IdentityComponent>(IdentityComponent{"", "SomeFaction"});
    ValidationReport report;
    PackageValidator::CheckIdentityCompleteness(obj, report);
    DOMINUS_EXPECT(!report.Passed());
}

DOMINUS_TEST(CheckIdentityCompleteness_FlagsMovesWithoutCombatGenome) {
    auto jabResult = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    MetaBinObject obj("props_with_moves", "0.1.0");
    obj.AddComponent<IdentityComponent>(IdentityComponent{"Something", "none"});
    MoveSetComponent moves;
    moves.moves.emplace("jab", *jabResult.value);
    obj.AddComponent<MoveSetComponent>(std::move(moves));  // moves present, no CombatIdentityComponent at all

    ValidationReport report;
    PackageValidator::CheckIdentityCompleteness(obj, report);
    DOMINUS_EXPECT(!report.Passed());
}

DOMINUS_TEST(CheckIdentityCompleteness_NoComplaintForNonCombatEntityWithNoMoves) {
    MetaBinObject obj("plain_prop", "0.1.0");
    obj.AddComponent<IdentityComponent>(IdentityComponent{"A Rock", "none"});
    ValidationReport report;
    PackageValidator::CheckIdentityCompleteness(obj, report);
    DOMINUS_EXPECT(report.Passed());  // info-level note only, not an error
}

// --- CheckDeterministicRebuild -----------------------------------------------

DOMINUS_TEST(CheckDeterministicRebuild_PassesForValidFile) {
    ValidationReport report;
    PackageValidator::CheckDeterministicRebuild(FixtureDir() / "brooklyn.dominus", report);
    DOMINUS_EXPECT(report.Passed());
}

DOMINUS_TEST(CheckDeterministicRebuild_FlagsMissingFile) {
    ValidationReport report;
    PackageValidator::CheckDeterministicRebuild(FixtureDir() / "does_not_exist.dominus", report);
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "integrity"));
}

// --- ValidateFile: the full pipeline -----------------------------------------

DOMINUS_TEST(ValidateFile_BrooklynPassesCompletely) {
    // The real proof this validator earns its place: Brooklyn's actual
    // shipping fixture, checked end to end, comes back completely clean.
    auto report = PackageValidator::ValidateFile(FixtureDir() / "brooklyn.dominus", FixtureDir());
    DOMINUS_EXPECT(report.Passed());
    DOMINUS_EXPECT(report.ErrorCount() == 0);
}

DOMINUS_TEST(ValidateFile_CatchesMissingAssetAndFailedBindGracefully) {
    // Proves the validator degrades gracefully -- a broken package
    // produces a REPORT, not a crash, and still runs every check it can.
    auto report = PackageValidator::ValidateFile(FixtureDir() / "broken_missing_skeleton.dominus", FixtureDir());
    DOMINUS_EXPECT(!report.Passed());
    DOMINUS_EXPECT(HasCategory(report, "asset_ownership"));
    DOMINUS_EXPECT(report.ErrorCount() >= 1);
}
