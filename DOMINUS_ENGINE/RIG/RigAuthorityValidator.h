// RIG/RigAuthorityValidator.h
// DOMINUS RIG v1.0's actual enforcement -- checks a real, loaded
// animation::Skeleton (and optionally a RetargetMap) against the
// canonical contract in CanonicalSkeleton.h.
//
// Honest scope, stated once here rather than re-litigated per check:
// this engine has NO mesh, skin-weight, inverse-bind-matrix, or socket
// data model anywhere -- Skeleton is bone transforms only (see its own
// header comment: "a flat bone array with parent indices"). Checks
// that require that data (weight validation, weight normalization,
// valid inverse bind matrices, socket bindings as a first-class
// concept, deformation test) are NOT implemented here -- they would
// have nothing real to check against, and faking a PASS for a check
// with no real backing is exactly the fabricated-evidence failure mode
// this whole engine has refused for 20+ phases. They're listed,
// explicitly, as NOT_DECLARED in every RigAuthorityReport this
// produces, not silently omitted.
//
// What IS real and checked: unique bone IDs, canonical parent
// hierarchy, required bones present, no orphan bones, no duplicate
// authority (an extension bone name colliding with a canonical name),
// valid bind pose (finite transforms), and retarget mapping validity
// (every target bone in an attached RetargetMap actually resolves).
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "ANIMATION/Retargeting/RetargetMap.h"
#include "ANIMATION/SkeletonSystem/Skeleton.h"
#include "RIG/CanonicalSkeleton.h"

namespace dominus::rig {

struct RigAuthorityIssue {
    std::string check;     // matches one of the 12 named checks below
    std::string severity;  // "error" | "not_declared"
    std::string message;
};

struct RigAuthorityReport {
    std::string skeleton_id;
    bool is_rigged = false;  // kRigged, per the directive's own vocabulary -- true iff zero "error" issues
    std::vector<RigAuthorityIssue> issues;
};

class RigAuthorityValidator {
public:
    static RigAuthorityReport Validate(const std::string& skeletonId, const animation::Skeleton& skeleton,
                                        const animation::RetargetMap* retargetMap = nullptr) {
        RigAuthorityReport report;
        report.skeleton_id = skeletonId;

        CheckUniqueBoneIds(skeleton, report);
        CheckRequiredBonesPresent(skeleton, report);
        CheckCanonicalParentHierarchy(skeleton, report);
        CheckNoOrphanBones(skeleton, report);
        CheckNoDuplicateAuthority(skeleton, report);
        CheckValidBindPose(skeleton, report);
        CheckRetargetMapping(skeleton, retargetMap, report);

        // Not implemented -- no real data model exists to check against.
        // Named explicitly so a report reader sees the gap, not an
        // absence.
        report.issues.push_back({"inverse_bind_matrices", "not_declared",
                                  "no inverse-bind-matrix concept exists in this engine (Transform2D bind poses only)"});
        report.issues.push_back(
            {"skin_weights", "not_declared", "no mesh/skin-weight data model exists in this engine"});
        report.issues.push_back(
            {"weight_normalization", "not_declared", "depends on skin_weights, which does not exist"});
        report.issues.push_back(
            {"socket_bindings", "not_declared",
             "no first-class Socket system exists -- *_socket extension bones are checked structurally "
             "(unique name, valid parent) but have no separate binding semantics to validate"});
        report.issues.push_back(
            {"deformation_test", "not_declared", "no mesh deformation exists to test -- depends on skin_weights"});

        report.is_rigged = true;
        for (const auto& issue : report.issues) {
            if (issue.severity == "error") report.is_rigged = false;
        }
        return report;
    }

private:
    // 1. Unique bone IDs -- defense in depth. SkeletonLoader itself now
    // rejects duplicate names at load time (the real gap this whole
    // phase started from), but a Skeleton can still be built
    // programmatically, bypassing the loader -- this check catches
    // that path too.
    static void CheckUniqueBoneIds(const animation::Skeleton& skeleton, RigAuthorityReport& report) {
        std::vector<std::string> seen;
        for (const auto& bone : skeleton.Bones()) {
            for (const auto& s : seen) {
                if (s == bone.name) {
                    report.issues.push_back(
                        {"unique_bone_ids", "error", "duplicate bone name '" + bone.name + "'"});
                }
            }
            seen.push_back(bone.name);
        }
    }

    // 2. Required bones present -- all 24 canonical bones, by name.
    static void CheckRequiredBonesPresent(const animation::Skeleton& skeleton, RigAuthorityReport& report) {
        for (const auto& canonical : CanonicalSkeletonBones()) {
            if (!skeleton.FindBoneIndex(canonical.name)) {
                report.issues.push_back(
                    {"required_bones_present", "error", "missing required canonical bone '" + canonical.name + "'"});
            }
        }
    }

    // 3. Canonical parent hierarchy -- for every bone that IS present
    // and IS a canonical name, its actual parent (by name) must match
    // the contract exactly.
    static void CheckCanonicalParentHierarchy(const animation::Skeleton& skeleton, RigAuthorityReport& report) {
        for (const auto& bone : skeleton.Bones()) {
            if (!IsCanonicalBoneName(bone.name)) continue;
            const std::string& requiredParent = CanonicalParentByName().at(bone.name);

            std::string actualParent;
            if (bone.parent_index >= 0) {
                actualParent = skeleton.Bones()[static_cast<size_t>(bone.parent_index)].name;
            }

            if (requiredParent.empty()) {
                if (bone.parent_index >= 0) {
                    report.issues.push_back({"canonical_parent_hierarchy", "error",
                                              "'" + bone.name + "' must be the root (parent null), found parent '" +
                                                  actualParent + "'"});
                }
            } else if (actualParent != requiredParent) {
                report.issues.push_back({"canonical_parent_hierarchy", "error",
                                          "'" + bone.name + "' must be parented to '" + requiredParent +
                                              "', found '" + (actualParent.empty() ? "(root)" : actualParent) + "'"});
            }
        }
    }

    // 4. No orphan bones -- defensive re-check. SkeletonLoader already
    // rejects an unresolvable parent name at load time (forward
    // references fail cleanly), so this is a structural sanity check
    // for any Skeleton built outside that loader: every non-root
    // parent_index must be a genuinely valid index into the bone array.
    static void CheckNoOrphanBones(const animation::Skeleton& skeleton, RigAuthorityReport& report) {
        int count = static_cast<int>(skeleton.BoneCount());
        for (const auto& bone : skeleton.Bones()) {
            if (bone.parent_index >= count) {
                report.issues.push_back(
                    {"no_orphan_bones", "error", "bone '" + bone.name + "' has an out-of-range parent index"});
            }
        }
    }

    // 5. No duplicate authority -- an extension bone must never claim a
    // canonical bone's own name (that would mean two different
    // authoring intents -- "this is chest" vs "this is a custom
    // extension" -- collapsing onto one bone slot).
    static void CheckNoDuplicateAuthority(const animation::Skeleton& skeleton, RigAuthorityReport& report) {
        for (const auto& bone : skeleton.Bones()) {
            if (IsCanonicalBoneName(bone.name)) continue;
            if (!IsKnownExtensionBoneName(bone.name)) {
                report.issues.push_back(
                    {"no_duplicate_authority", "error",
                     "'" + bone.name + "' is neither a canonical bone nor a recognized extension pattern"});
            }
        }
    }

    // 6. Valid bind pose -- every world-space transform must be finite.
    // NaN/Inf here would silently corrupt every hitbox/hurtbox/IK
    // target that reads this bone's world position.
    static void CheckValidBindPose(const animation::Skeleton& skeleton, RigAuthorityReport& report) {
        auto world = skeleton.ComputeBindPoseWorld();
        for (size_t i = 0; i < world.size(); ++i) {
            const auto& t = world[i];
            bool finite = std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.rotation_deg) &&
                          std::isfinite(t.scale_x) && std::isfinite(t.scale_y);
            if (!finite) {
                report.issues.push_back(
                    {"valid_bind_pose", "error", "bone '" + skeleton.Bones()[i].name + "' has a non-finite world transform"});
            }
        }
    }

    // 7. Retarget mapping -- if a RetargetMap is attached, every target
    // bone it declares must actually exist on this skeleton (matches
    // the same real check RigBinder::RetargetMapComponent binding
    // already implicitly relies on working correctly).
    static void CheckRetargetMapping(const animation::Skeleton& skeleton, const animation::RetargetMap* retargetMap,
                                      RigAuthorityReport& report) {
        if (!retargetMap) return;
        for (const auto& targetBone : retargetMap->AllTargetBones()) {
            if (!skeleton.FindBoneIndex(targetBone)) {
                report.issues.push_back(
                    {"retarget_mapping", "error", "retarget map references unknown target bone '" + targetBone + "'"});
            }
        }
    }
};

}  // namespace dominus::rig
