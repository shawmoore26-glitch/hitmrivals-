// tests/rig/test_brooklyn_migration.cpp
// RIG Phase 4: migrate Brooklyn to the canonical skeleton without
// destroying his existing animation/combat behavior.
//
// "Do not make Brooklyn conform by weakening the canonical contract.
// Make Brooklyn conform by transforming the source reality."
//
// The transformation, derived once and applied mechanically to every
// clip: each old rigid bone (torso/head/arm_r/arm_l/leg_r/leg_l) had a
// fixed REST offset from its parent. In the canonical skeleton, that
// exact offset is attributed entirely to ONE new intermediate bone
// (pelvis carries torso's old (0,40); neck carries head's old (0,30);
// clavicle_L/R carry arm_l/arm_r's old shoulder offsets; thigh_L/R
// carry leg_l/leg_r's old hip offsets, adjusted for now being
// parented to pelvis instead of root), every other intermediate bone
// stays at identity (0,0,0 rotation), and the terminal canonical bone
// (chest/head/hand_L/hand_R/foot_L/foot_R) gets the OLD bone's
// keyframe values with that same rest offset SUBTRACTED from position
// only -- rotation and scale carry over unchanged. Because every
// intermediate bone has zero rotation, ComposeWorld's rotation-then-
// translate math (ANIMATION/SkeletonSystem/Transform2D.h) guarantees
// this subtraction is exactly invertible: the resulting world-space
// pose is IDENTICAL, not merely close, to the original at every
// terminal bone, for every keyframe, proven below by direct
// computation rather than asserted.
//
// This was derived and verified in Python first (multiple time
// samples per clip, including out-of-range/looping cases) before this
// permanent C++ test was written -- this file is the checked-in,
// always-run version of that same proof.
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"
#include "COMBAT/HitSystem/HurtboxLoader.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "RIG/RigAuthorityValidator.h"
#include "RIG/RigProfile.h"
#include "RIG/RigProfileValidator.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <unordered_map>

using dominus::animation::AnimationClip;
using dominus::animation::AnimationClipLoader;
using dominus::animation::AnimationPlayer;
using dominus::animation::Skeleton;
using dominus::animation::SkeletonLoader;
using dominus::combat::HurtboxLoader;
using dominus::combat::MoveLoader;
using dominus::rig::RigAuthorityValidator;
using dominus::rig::RigProfile;
using dominus::rig::RigProfileLifecycle;
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

bool NearlyEqual(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }

// The old_bone -> canonical_bone correspondence this migration proves
// equivalent -- the exact same table the migration script used.
const std::unordered_map<std::string, std::string>& OldToNewTerminal() {
    static const std::unordered_map<std::string, std::string> kMap = {
        {"root", "root"}, {"torso", "chest"}, {"head", "head"},   {"arm_r", "hand_R"},
        {"arm_l", "hand_L"}, {"leg_r", "foot_R"}, {"leg_l", "foot_L"},
    };
    return kMap;
}

// Every real clip this migration covers, old name -> canonical name.
const std::vector<std::pair<std::string, std::string>>& ClipPairs() {
    static const std::vector<std::pair<std::string, std::string>> kPairs = {
        {"brooklyn_idle.clip.json", "canonical_brooklyn_idle.clip.json"},
        {"brooklyn_attack.clip.json", "canonical_brooklyn_attack.clip.json"},
        {"brooklyn_air_combo.clip.json", "canonical_brooklyn_air_combo.clip.json"},
        {"brooklyn_block_impact.clip.json", "canonical_brooklyn_block_impact.clip.json"},
        {"brooklyn_combo_starter.clip.json", "canonical_brooklyn_combo_starter.clip.json"},
        {"brooklyn_counter.clip.json", "canonical_brooklyn_counter.clip.json"},
        {"brooklyn_dodge.clip.json", "canonical_brooklyn_dodge.clip.json"},
        {"brooklyn_knockback.clip.json", "canonical_brooklyn_knockback.clip.json"},
        {"brooklyn_knockdown.clip.json", "canonical_brooklyn_knockdown.clip.json"},
        {"brooklyn_knockdown_recovery.clip.json", "canonical_brooklyn_knockdown_recovery.clip.json"},
        {"brooklyn_launcher.clip.json", "canonical_brooklyn_launcher.clip.json"},
        {"brooklyn_stagger.clip.json", "canonical_brooklyn_stagger.clip.json"},
        {"brooklyn_transformation.clip.json", "canonical_brooklyn_transformation.clip.json"},
    };
    return kPairs;
}
}  // namespace

// --- Bind pose equivalence ---------------------------------------------

DOMINUS_TEST(BrooklynMigration_CanonicalSkeletonMatchesLegacyBindPoseExactly) {
    auto dir = FixtureDir();
    auto legacy = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto canonical = SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    DOMINUS_EXPECT(legacy.ok && canonical.ok);
    DOMINUS_EXPECT(canonical.value->BoneCount() == 24);

    auto legacyWorld = legacy.value->ComputeBindPoseWorld();
    auto canonicalWorld = canonical.value->ComputeBindPoseWorld();

    for (const auto& [oldName, newName] : OldToNewTerminal()) {
        auto oldIdx = legacy.value->FindBoneIndex(oldName);
        auto newIdx = canonical.value->FindBoneIndex(newName);
        DOMINUS_EXPECT(oldIdx.has_value());
        DOMINUS_EXPECT(newIdx.has_value());
        const auto& ow = legacyWorld[*oldIdx];
        const auto& nw = canonicalWorld[*newIdx];
        DOMINUS_EXPECT(NearlyEqual(ow.x, nw.x));
        DOMINUS_EXPECT(NearlyEqual(ow.y, nw.y));
        DOMINUS_EXPECT(NearlyEqual(ow.rotation_deg, nw.rotation_deg));
    }
}

// --- Canonical skeleton passes DOMINUS RIG v1.0 --------------------------

DOMINUS_TEST(BrooklynMigration_CanonicalSkeletonPassesRigAuthorityValidator) {
    auto dir = FixtureDir();
    auto canonical = SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    DOMINUS_EXPECT(canonical.ok);

    auto report = RigAuthorityValidator::Validate("brooklyn_canonical", *canonical.value);
    DOMINUS_EXPECT(report.is_rigged);
    int errorCount = 0;
    for (const auto& issue : report.issues) {
        if (issue.severity == "error") errorCount++;
    }
    DOMINUS_EXPECT(errorCount == 0);
}

// --- Full animated behavioral equivalence, every real clip ----------------

DOMINUS_TEST(BrooklynMigration_AllThirteenClipsProduceIdenticalWorldPosesAtEveryTerminalBone) {
    auto dir = FixtureDir();
    auto legacySkel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto canonicalSkel = SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    DOMINUS_EXPECT(legacySkel.ok && canonicalSkel.ok);

    int clipsChecked = 0;
    for (const auto& [oldClipName, newClipName] : ClipPairs()) {
        auto oldClip = AnimationClipLoader::LoadFromFile(dir / oldClipName);
        auto newClip = AnimationClipLoader::LoadFromFile(dir / newClipName);
        DOMINUS_EXPECT(oldClip.ok);
        DOMINUS_EXPECT(newClip.ok);
        if (!oldClip.ok || !newClip.ok) continue;
        clipsChecked++;

        float duration = oldClip.value->duration;
        // Same sample points the Python derivation used, including one
        // past the clip's own duration (clamp/loop behavior must also
        // match, not just the interior).
        std::vector<float> samples = {0.0f, duration * 0.25f, duration * 0.5f, duration * 0.75f, duration,
                                       duration * 1.3f};

        for (float t : samples) {
            auto oldPose = AnimationPlayer::Sample(*legacySkel.value, *oldClip.value, t);
            auto newPose = AnimationPlayer::Sample(*canonicalSkel.value, *newClip.value, t);

            for (const auto& [oldName, newName] : OldToNewTerminal()) {
                auto oldIdx = legacySkel.value->FindBoneIndex(oldName);
                auto newIdx = canonicalSkel.value->FindBoneIndex(newName);
                const auto& ow = oldPose[*oldIdx];
                const auto& nw = newPose[*newIdx];
                DOMINUS_EXPECT(NearlyEqual(ow.x, nw.x, 0.01f));
                DOMINUS_EXPECT(NearlyEqual(ow.y, nw.y, 0.01f));
                DOMINUS_EXPECT(NearlyEqual(ow.rotation_deg, nw.rotation_deg, 0.01f));
            }
        }
    }
    DOMINUS_EXPECT(clipsChecked == 13);  // every real clip this migration covers, none silently skipped
}

// --- Combat data migrated consistently ------------------------------------

DOMINUS_TEST(BrooklynMigration_CanonicalHurtboxesReferenceRealBonesOnTheCanonicalSkeleton) {
    auto dir = FixtureDir();
    auto canonicalSkel = SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_canonical_hurtboxes.json");
    DOMINUS_EXPECT(canonicalSkel.ok && hurtboxes.ok);
    DOMINUS_EXPECT(hurtboxes.value->boxes.size() == 6);  // head/chest/hand_L/hand_R/foot_L/foot_R
    for (const auto& box : hurtboxes.value->boxes) {
        DOMINUS_EXPECT(canonicalSkel.value->FindBoneIndex(box.bone).has_value());
    }
}

DOMINUS_TEST(BrooklynMigration_CanonicalJabHitboxReferencesARealCanonicalBone) {
    auto dir = FixtureDir();
    auto canonicalSkel = SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    auto move = MoveLoader::LoadFromFile(dir / "canonical_brooklyn_move_jab.json");
    DOMINUS_EXPECT(canonicalSkel.ok && move.ok);
    DOMINUS_EXPECT(!move.value->hitboxes.empty());
    for (const auto& hb : move.value->hitboxes) {
        DOMINUS_EXPECT(canonicalSkel.value->FindBoneIndex(hb.bone).has_value());
        DOMINUS_EXPECT(hb.bone == "hand_R");  // arm_r's real migration target
    }
}

// --- RigProfile lifecycle, now backed by real, computed evidence ---------

DOMINUS_TEST(BrooklynMigration_CanonicalIdentityProfileReachesValidated) {
    auto dir = FixtureDir();
    auto canonicalSkel = SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    DOMINUS_EXPECT(canonicalSkel.ok);

    RigProfile profile;
    profile.profile_id = "brooklyn_canonical_identity_v1";
    profile.entity_id = "brooklyn";
    for (const auto& bone : canonicalSkel.value->Bones()) {
        profile.mappings.push_back({bone.name, bone.name});  // canonical skeleton maps to itself
    }

    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToMapped(profile));
    auto report = RigProfileValidator::Validate(*canonicalSkel.value, profile);
    DOMINUS_EXPECT(report.valid);
    DOMINUS_EXPECT(RigProfileLifecycle::AdvanceToValidated(profile, report.valid));
    DOMINUS_EXPECT(profile.state == RigProfileState::kValidated);

    // COMPATIBLE is now a REAL, evidence-based declaration -- not a
    // hand-wave. The reason cites the actual permanent regression test
    // that proves it, the same test this file itself contains.
    DOMINUS_EXPECT(RigProfileLifecycle::DeclareCompatible(
        profile,
        "numerically verified: BrooklynMigration_AllThirteenClipsProduceIdenticalWorldPosesAtEveryTerminalBone "
        "proves all 13 real clips produce identical world-space poses at every terminal bone, at multiple time "
        "samples per clip, including out-of-range/looping behavior"));
    DOMINUS_EXPECT(profile.state == RigProfileState::kCompatible);

    // Deliberately NOT declared ACTIVE here -- see the roadmap entry
    // for why: activating means cutting production Brooklyn
    // (brooklyn.dominus) over to these fixtures, which nothing in this
    // phase does. COMPATIBLE (proven, not yet cut over) is the correct,
    // honest stopping point.
    DOMINUS_EXPECT(profile.state != RigProfileState::kActive);
}
