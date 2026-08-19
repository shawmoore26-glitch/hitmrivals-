// tests/rig/test_brooklyn_acceptance.cpp
// Canonical Character Acceptance Harness: the missing piece between
// "COMPATIBLE" (a human's reasoned belief) and "ACTIVE" (a real,
// derived fact). Runs all 5 real checks against Brooklyn's actual
// legacy and canonical fixtures, generates a real AcceptanceCertificate
// (never hand-typed), and proves the RigProfileLifecycle gate genuinely
// refuses ACCEPTED/ACTIVE without one.
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "RIG/AcceptanceCertificate.h"
#include "RIG/CharacterAcceptanceHarness.h"
#include "RIG/RigProfile.h"
#include "RIG/RigProfileLoader.h"
#include "RIG/RigProfileValidator.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::rig::AcceptanceCertificateForge;
using dominus::rig::BoneMap;
using dominus::rig::CharacterAcceptanceHarness;
using dominus::rig::ClipPairs;
using dominus::rig::RigProfile;
using dominus::rig::RigProfileLifecycle;
using dominus::rig::RigProfileLoader;
using dominus::rig::RigProfileState;
using dominus::rig::RigProfileValidator;

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

BoneMap BrooklynBoneMap() {
    return {{"root", "root"}, {"torso", "chest"}, {"head", "head"},   {"arm_r", "hand_R"},
            {"arm_l", "hand_L"}, {"leg_r", "foot_R"}, {"leg_l", "foot_L"}};
}

ClipPairs BrooklynClipPairs(const std::filesystem::path& dir) {
    std::vector<std::string> names = {"idle",     "attack",     "air_combo",  "block_impact", "combo_starter",
                                       "counter",  "dodge",      "knockback",  "knockdown",    "knockdown_recovery",
                                       "launcher", "stagger",    "transformation"};
    ClipPairs pairs;
    for (const auto& n : names) {
        pairs.push_back({dir / ("brooklyn_" + n + ".clip.json"), dir / ("canonical_brooklyn_" + n + ".clip.json")});
    }
    return pairs;
}
}  // namespace

// --- Each individual check, against Brooklyn's real fixtures --------------

DOMINUS_TEST(CharacterAcceptanceHarness_Skeleton_PassesForBrooklyn) {
    auto dir = FixtureDir();
    auto section = CharacterAcceptanceHarness::CheckSkeleton(dir / "brooklyn.skel.json",
                                                              dir / "brooklyn_canonical.skel.json", BrooklynBoneMap());
    DOMINUS_EXPECT(section.name == "Skeleton");
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(CharacterAcceptanceHarness_Animation_PassesAllThirteenClipsForBrooklyn) {
    auto dir = FixtureDir();
    auto section = CharacterAcceptanceHarness::CheckAnimation(dir / "brooklyn.skel.json",
                                                                dir / "brooklyn_canonical.skel.json",
                                                                BrooklynBoneMap(), BrooklynClipPairs(dir));
    DOMINUS_EXPECT(section.name == "Animation");
    DOMINUS_EXPECT(section.passed);
    DOMINUS_EXPECT(section.detail.find("13/13") != std::string::npos);
}

DOMINUS_TEST(CharacterAcceptanceHarness_Combat_PassesForBrooklyn) {
    auto dir = FixtureDir();
    auto section = CharacterAcceptanceHarness::CheckCombat(
        dir / "brooklyn_canonical.skel.json", dir / "brooklyn_canonical_hurtboxes.json",
        dir / "canonical_brooklyn_move_jab.json");
    DOMINUS_EXPECT(section.name == "Combat");
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(CharacterAcceptanceHarness_Runtime_PassesForBrooklyn) {
    auto dir = FixtureDir();
    auto section = CharacterAcceptanceHarness::CheckRuntime(dir / "brooklyn_canonical.dominus", dir,
                                                              dir / "brooklyn_canonical_hurtboxes.json", "jab");
    DOMINUS_EXPECT(section.name == "Runtime");
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(CharacterAcceptanceHarness_Determinism_PassesForBrooklyn) {
    auto dir = FixtureDir();
    auto section = CharacterAcceptanceHarness::CheckDeterminism(dir / "brooklyn_canonical.dominus", dir,
                                                                  dir / "brooklyn_canonical_hurtboxes.json", "jab");
    DOMINUS_EXPECT(section.name == "Determinism");
    DOMINUS_EXPECT(section.passed);
}

// --- A genuine failure case: mismatched bone map produces a real FAIL -----

DOMINUS_TEST(CharacterAcceptanceHarness_Skeleton_FailsHonestlyForAWrongBoneMap) {
    auto dir = FixtureDir();
    BoneMap wrongMap = {{"torso", "head"}};  // deliberately wrong correspondence
    auto section =
        CharacterAcceptanceHarness::CheckSkeleton(dir / "brooklyn.skel.json", dir / "brooklyn_canonical.skel.json",
                                                    wrongMap);
    DOMINUS_EXPECT(!section.passed);  // the harness does not paper over a real mismatch
}

// --- The full certificate, generated, never hand-typed --------------------

DOMINUS_TEST(BrooklynAcceptance_FullCertificateAllSectionsPass) {
    auto dir = FixtureDir();
    std::vector<dominus::rig::AcceptanceSection> sections = {
        CharacterAcceptanceHarness::CheckSkeleton(dir / "brooklyn.skel.json", dir / "brooklyn_canonical.skel.json",
                                                    BrooklynBoneMap()),
        CharacterAcceptanceHarness::CheckAnimation(dir / "brooklyn.skel.json", dir / "brooklyn_canonical.skel.json",
                                                     BrooklynBoneMap(), BrooklynClipPairs(dir)),
        CharacterAcceptanceHarness::CheckCombat(dir / "brooklyn_canonical.skel.json",
                                                  dir / "brooklyn_canonical_hurtboxes.json",
                                                  dir / "canonical_brooklyn_move_jab.json"),
        CharacterAcceptanceHarness::CheckRuntime(dir / "brooklyn_canonical.dominus", dir,
                                                   dir / "brooklyn_canonical_hurtboxes.json", "jab"),
        CharacterAcceptanceHarness::CheckDeterminism(dir / "brooklyn_canonical.dominus", dir,
                                                       dir / "brooklyn_canonical_hurtboxes.json", "jab"),
    };

    auto cert = AcceptanceCertificateForge::Generate("brooklyn", "brooklyn_canonical_identity_v1", sections);

    DOMINUS_EXPECT(cert.sections.size() == 5);
    DOMINUS_EXPECT(cert.overall_pass);
    DOMINUS_EXPECT(cert.certificate_hash.size() == 64);
    for (const auto& s : cert.sections) {
        DOMINUS_EXPECT(s.passed);
    }
}

DOMINUS_TEST(BrooklynAcceptance_CertificateWithARealFailureIsNotOverallPass) {
    std::vector<dominus::rig::AcceptanceSection> sections = {
        {"Skeleton", true, "ok"},
        {"Animation", false, "a real, genuine failure"},
        {"Combat", true, "ok"},
    };
    auto cert = AcceptanceCertificateForge::Generate("x", "y", sections);
    DOMINUS_EXPECT(!cert.overall_pass);  // one real failure means the whole certificate fails
}

// --- The full lifecycle, end to end, driven by the real certificate -------

DOMINUS_TEST(BrooklynAcceptance_FullLifecycleFromAuthoredToActive) {
    auto dir = FixtureDir();
    auto canonicalSkel = dominus::animation::SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    DOMINUS_EXPECT(canonicalSkel.ok);

    RigProfile profile;
    profile.profile_id = "brooklyn_canonical_identity_v1";
    profile.entity_id = "brooklyn";
    for (const auto& bone : canonicalSkel.value->Bones()) {
        profile.mappings.push_back({bone.name, bone.name});
    }
    DOMINUS_EXPECT(profile.state == RigProfileState::kAuthored);

    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToMapped(profile));
    auto report = RigProfileValidator::Validate(*canonicalSkel.value, profile);
    DOMINUS_EXPECT(report.valid);
    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToValidated(profile, report.valid));
    DOMINUS_EXPECT(RigProfileLifecycle::DeclareCompatible(
        profile, "structural migration map derived and verified in the bone-rig-migration phase"));
    DOMINUS_EXPECT(profile.state == RigProfileState::kCompatible);

    // Run the real harness -- this IS the acceptance evidence.
    std::vector<dominus::rig::AcceptanceSection> sections = {
        CharacterAcceptanceHarness::CheckSkeleton(dir / "brooklyn.skel.json", dir / "brooklyn_canonical.skel.json",
                                                    BrooklynBoneMap()),
        CharacterAcceptanceHarness::CheckAnimation(dir / "brooklyn.skel.json", dir / "brooklyn_canonical.skel.json",
                                                     BrooklynBoneMap(), BrooklynClipPairs(dir)),
        CharacterAcceptanceHarness::CheckCombat(dir / "brooklyn_canonical.skel.json",
                                                  dir / "brooklyn_canonical_hurtboxes.json",
                                                  dir / "canonical_brooklyn_move_jab.json"),
        CharacterAcceptanceHarness::CheckRuntime(dir / "brooklyn_canonical.dominus", dir,
                                                   dir / "brooklyn_canonical_hurtboxes.json", "jab"),
        CharacterAcceptanceHarness::CheckDeterminism(dir / "brooklyn_canonical.dominus", dir,
                                                       dir / "brooklyn_canonical_hurtboxes.json", "jab"),
    };
    auto cert = AcceptanceCertificateForge::Generate(profile.entity_id, profile.profile_id, sections);
    DOMINUS_EXPECT(cert.overall_pass);

    // ACCEPTED: mechanical, gated on the real certificate.
    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToAccepted(profile, cert.overall_pass, cert.certificate_hash));
    DOMINUS_EXPECT(profile.state == RigProfileState::kAccepted);
    DOMINUS_EXPECT(profile.accepted_certificate_hash == cert.certificate_hash);

    // ACTIVE: mechanical, requires the SAME certificate hash.
    DOMINUS_EXPECT(RigProfileLifecycle::DeclareActive(
        profile, cert.certificate_hash,
        "cutover approved -- brooklyn_canonical.dominus is now the production reference; "
        "legacy brooklyn.dominus retained for rollback"));
    DOMINUS_EXPECT(profile.state == RigProfileState::kActive);

    // Brooklyn is now the first canonical production character --
    // proven, not asserted. Note: this test proves the MECHANISM;
    // production brooklyn.dominus itself is untouched by this test
    // (see the roadmap for the deliberate, separate cutover decision).
}

DOMINUS_TEST(BrooklynAcceptance_CannotReachAcceptedWithAFailingCertificate) {
    RigProfile profile;
    profile.mappings.push_back({"root", "root"});
    RigProfileLifecycle::AdvanceToMapped(profile);
    RigProfileLifecycle::AdvanceToValidated(profile, true);
    RigProfileLifecycle::DeclareCompatible(profile, "reason");

    std::vector<dominus::rig::AcceptanceSection> failingSections = {
        {"Skeleton", true, "ok"},
        {"Animation", false, "a genuine, real mismatch found"},
    };
    auto cert = AcceptanceCertificateForge::Generate("x", "y", failingSections);
    DOMINUS_EXPECT(!cert.overall_pass);

    DOMINUS_EXPECT(!RigProfileLifecycle::AdvanceToAccepted(profile, cert.overall_pass, cert.certificate_hash));
    DOMINUS_EXPECT(profile.state == RigProfileState::kCompatible);  // correctly stuck, not silently advanced
}
