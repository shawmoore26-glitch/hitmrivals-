// tests/rig/test_rig_profile.cpp
// RIG Phase 3: rig compatibility/migration architecture. RigProfile is
// explicit, authored legacy->canonical mapping data -- never invented.
// RigProfileValidator turns "is_rigged=false" into a real diagnostic:
// mapped/missing/extra/hierarchy_conflicts/unresolved. Two real
// scenarios, proven honestly: canonical_biped's identity profile
// reaches the full lifecycle (UNMAPPED->MAPPED->VALIDATED->COMPATIBLE
// ->ACTIVE); Brooklyn's real, honest profile legitimately stops at
// MAPPED, with every gap named, not hidden or forced through.
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "RIG/RigProfile.h"
#include "RIG/RigProfileLoader.h"
#include "RIG/RigProfileValidator.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::animation::SkeletonLoader;
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

bool Contains(const std::vector<std::string>& items, const std::string& needle) {
    for (const auto& s : items) {
        if (s.find(needle) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

// --- RigProfileLoader ---------------------------------------------------

DOMINUS_TEST(RigProfileLoader_LoadsRealBrooklynProfile) {
    auto dir = FixtureDir();
    auto result = RigProfileLoader::LoadFromFile(dir / "brooklyn_rig_profile.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->profile_id == "brooklyn_legacy_v1");
    DOMINUS_EXPECT(result.value->entity_id == "brooklyn");
    DOMINUS_EXPECT(result.value->mappings.size() == 7);
    DOMINUS_EXPECT(result.value->CanonicalFor("torso").value() == "chest");
}

DOMINUS_TEST(RigProfileLoader_RejectsMissingMappingsArray) {
    auto dir = FixtureDir();
    // Any existing non-conforming JSON file works to prove the failure path.
    auto result = RigProfileLoader::LoadFromFile(dir / "brooklyn_visual.json");
    DOMINUS_EXPECT(!result.ok);
}

// --- RigProfileValidator: the honest, partial Brooklyn case --------------

DOMINUS_TEST(RigProfileValidator_BrooklynsRealProfileReportsGenuineGaps_NotInventedCoverage) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto profile = RigProfileLoader::LoadFromFile(dir / "brooklyn_rig_profile.json");
    DOMINUS_EXPECT(skel.ok && profile.ok);

    auto report = RigProfileValidator::Validate(*skel.value, *profile.value);

    // 7 real, resolved mappings.
    DOMINUS_EXPECT(report.mapped.size() == 7);
    DOMINUS_EXPECT(Contains(report.mapped, "torso -> chest"));

    // Genuinely missing: every canonical bone Brooklyn's simplified rig
    // never had a sub-segment for (pelvis, spine_01-03, neck,
    // clavicle_L/R, upperarm_L/R, forearm_L/R, thigh_L/R, calf_L/R,
    // toe_L/R) -- 24 canonical bones minus the 7 actually mapped = 17.
    DOMINUS_EXPECT(report.missing.size() == 17);
    DOMINUS_EXPECT(Contains(report.missing, "pelvis"));
    DOMINUS_EXPECT(Contains(report.missing, "neck"));
    DOMINUS_EXPECT(Contains(report.missing, "upperarm_R"));

    // Zero legacy bones left unmapped -- every one of Brooklyn's 7 real
    // bones got a real mapping entry.
    DOMINUS_EXPECT(report.extra.empty());

    // Real hierarchy conflicts: chest's legacy parent (root) maps to
    // canonical root, not the required spine_03; head's legacy parent
    // (torso) maps to chest, not the required neck; hand_R/hand_L's
    // legacy parent (torso) maps to chest, not forearm_R/forearm_L;
    // foot_R/foot_L's legacy parent (root) maps to root, not
    // thigh_R/thigh_L. Every mapped bone except root itself conflicts.
    DOMINUS_EXPECT(report.hierarchy_conflicts.size() == 6);
    DOMINUS_EXPECT(Contains(report.hierarchy_conflicts, "chest"));
    DOMINUS_EXPECT(Contains(report.hierarchy_conflicts, "head"));

    // Zero unresolved -- every mapping entry names a real legacy bone
    // and a real canonical name; the PROBLEM is structural (hierarchy),
    // not a typo in the profile itself.
    DOMINUS_EXPECT(report.unresolved.empty());

    DOMINUS_EXPECT(!report.valid);  // honest: this profile is not valid yet
}

DOMINUS_TEST(RigProfileValidator_DetectsAnUnresolvedLegacyBoneReference) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    DOMINUS_EXPECT(skel.ok);

    RigProfile profile;
    profile.profile_id = "broken";
    profile.mappings.push_back({"does_not_exist_on_brooklyn", "chest"});

    auto report = RigProfileValidator::Validate(*skel.value, profile);
    DOMINUS_EXPECT(!report.valid);
    DOMINUS_EXPECT(report.unresolved.size() == 1);
    DOMINUS_EXPECT(Contains(report.unresolved, "does_not_exist_on_brooklyn"));
}

DOMINUS_TEST(RigProfileValidator_DetectsAnUnknownCanonicalBoneReference) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    DOMINUS_EXPECT(skel.ok);

    RigProfile profile;
    profile.profile_id = "broken";
    profile.mappings.push_back({"torso", "not_a_real_canonical_name"});

    auto report = RigProfileValidator::Validate(*skel.value, profile);
    DOMINUS_EXPECT(!report.valid);
    DOMINUS_EXPECT(report.unresolved.size() == 1);
}

// --- RigProfileValidator: the real, fully-compatible case ----------------

DOMINUS_TEST(RigProfileValidator_IdentityProfileOnAConformantSkeletonIsFullyValid) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "canonical_biped.skel.json");
    auto profile = RigProfileLoader::LoadFromFile(dir / "canonical_biped_rig_profile.json");
    DOMINUS_EXPECT(skel.ok && profile.ok);
    DOMINUS_EXPECT(profile.value->mappings.size() == 24);

    auto report = RigProfileValidator::Validate(*skel.value, *profile.value);
    DOMINUS_EXPECT(report.mapped.size() == 24);
    DOMINUS_EXPECT(report.missing.empty());
    DOMINUS_EXPECT(report.extra.empty());
    DOMINUS_EXPECT(report.hierarchy_conflicts.empty());
    DOMINUS_EXPECT(report.unresolved.empty());
    DOMINUS_EXPECT(report.valid);
}

// --- Lifecycle: the real, full path -----------------------------------

DOMINUS_TEST(RigProfileLifecycle_FullPathToAcceptedOnARealCompliantProfile) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "canonical_biped.skel.json");
    auto profile = RigProfileLoader::LoadFromFile(dir / "canonical_biped_rig_profile.json");
    DOMINUS_EXPECT(skel.ok && profile.ok);
    RigProfile p = *profile.value;
    DOMINUS_EXPECT(p.state == RigProfileState::kAuthored);

    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToMapped(p));
    DOMINUS_EXPECT(p.state == RigProfileState::kMapped);

    auto report = RigProfileValidator::Validate(*skel.value, p);
    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToValidated(p, report.valid));
    DOMINUS_EXPECT(p.state == RigProfileState::kValidated);

    DOMINUS_EXPECT(RigProfileLifecycle::DeclareCompatible(
        p, "identity mapping on an already-canonical skeleton -- structurally trivial"));
    DOMINUS_EXPECT(p.state == RigProfileState::kCompatible);
    DOMINUS_EXPECT(!p.compatible_reason.empty());

    // ACCEPTED/ACTIVE now require a real AcceptanceCertificate -- see
    // RIG/AcceptanceCertificate.h and tests/rig/test_brooklyn_migration.cpp
    // for the full, evidence-based promotion. A synthetic passing
    // certificate is enough to prove the GATE here without re-running
    // the whole harness in this unrelated test.
    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToAccepted(p, /*certificateOverallPass=*/true, "fakehash123"));
    DOMINUS_EXPECT(p.state == RigProfileState::kAccepted);

    DOMINUS_EXPECT(RigProfileLifecycle::DeclareActive(p, "fakehash123", "verified test fixture, safe to activate"));
    DOMINUS_EXPECT(p.state == RigProfileState::kActive);
    DOMINUS_EXPECT(!p.active_note.empty());
}

DOMINUS_TEST(RigProfileLifecycle_ActiveRefusesAMismatchedCertificateHash) {
    RigProfile p;
    p.mappings.push_back({"a", "root"});
    RigProfileLifecycle::AdvanceToMapped(p);
    RigProfileLifecycle::AdvanceToValidated(p, true);
    RigProfileLifecycle::DeclareCompatible(p, "reason");
    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToAccepted(p, true, "hash_A"));
    // Trying to activate citing a DIFFERENT certificate than the one
    // that earned acceptance must fail -- prevents silently swapping
    // in unrelated "evidence".
    DOMINUS_EXPECT(!RigProfileLifecycle::DeclareActive(p, "hash_B", "note"));
    DOMINUS_EXPECT(p.state == RigProfileState::kAccepted);
}

DOMINUS_TEST(RigProfileLifecycle_AcceptedRequiresAPassingCertificate) {
    RigProfile p;
    p.mappings.push_back({"a", "root"});
    RigProfileLifecycle::AdvanceToMapped(p);
    RigProfileLifecycle::AdvanceToValidated(p, true);
    RigProfileLifecycle::DeclareCompatible(p, "reason");
    DOMINUS_EXPECT(!RigProfileLifecycle::AdvanceToAccepted(p, /*certificateOverallPass=*/false, "hash"));
    DOMINUS_EXPECT(p.state == RigProfileState::kCompatible);  // correctly stuck, not silently advanced
}

DOMINUS_TEST(RigProfileLifecycle_BrooklynsRealProfileHonestlyStopsAtMapped) {
    // The correct, honest outcome for THIS phase: Brooklyn's real
    // profile has real mappings (reaches MAPPED) but cannot honestly
    // reach VALIDATED because real missing bones and real hierarchy
    // conflicts exist. Migrating Brooklyn onto full compliance is a
    // later, separate phase -- not forced here.
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto profile = RigProfileLoader::LoadFromFile(dir / "brooklyn_rig_profile.json");
    DOMINUS_EXPECT(skel.ok && profile.ok);
    RigProfile p = *profile.value;

    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToMapped(p));
    DOMINUS_EXPECT(p.state == RigProfileState::kMapped);

    auto report = RigProfileValidator::Validate(*skel.value, p);
    DOMINUS_EXPECT(!report.valid);
    DOMINUS_EXPECT(!RigProfileLifecycle::AdvanceToValidated(p, report.valid));
    DOMINUS_EXPECT(p.state == RigProfileState::kMapped);  // correctly stuck here, not silently advanced
}

DOMINUS_TEST(RigProfileLifecycle_CannotSkipStates) {
    RigProfile p;
    p.mappings.push_back({"a", "root"});
    DOMINUS_EXPECT(!RigProfileLifecycle::AdvanceToValidated(p, true));  // still AUTHORED, can't skip MAPPED
    DOMINUS_EXPECT(p.state == RigProfileState::kAuthored);
    DOMINUS_EXPECT(!RigProfileLifecycle::DeclareCompatible(p, "reason"));
    DOMINUS_EXPECT(!RigProfileLifecycle::AdvanceToAccepted(p, true, "hash"));
    DOMINUS_EXPECT(!RigProfileLifecycle::DeclareActive(p, "hash", "note"));
}

DOMINUS_TEST(RigProfileLifecycle_CompatibleAndActiveRequireARealNonEmptyReason) {
    RigProfile p;
    p.mappings.push_back({"a", "root"});
    RigProfileLifecycle::AdvanceToMapped(p);
    RigProfileLifecycle::AdvanceToValidated(p, true);
    DOMINUS_EXPECT(!RigProfileLifecycle::DeclareCompatible(p, ""));  // empty reason rejected
    DOMINUS_EXPECT(p.state == RigProfileState::kValidated);
}

DOMINUS_TEST(RigProfileLifecycle_MappedRequiresAtLeastOneRealMapping) {
    RigProfile p;  // zero mappings
    DOMINUS_EXPECT(!RigProfileLifecycle::AdvanceToMapped(p));
    DOMINUS_EXPECT(p.state == RigProfileState::kAuthored);
}
