// RIG/RigProfileValidator.h
// Turns "is_rigged=false" into an actual migration diagnostic:
//   mapped              -- legacy bones successfully corresponding to a canonical bone
//   missing             -- canonical bones no mapping ever addresses (NOT invented -- reported)
//   extra               -- legacy skeleton bones the profile doesn't map to anything
//   hierarchy_conflicts -- a mapped bone's real legacy parent doesn't correspond
//                          (via the SAME profile) to its required canonical parent
//   unresolved          -- a mapping entry referencing a legacy bone name that
//                          doesn't actually exist on the given skeleton, or a
//                          canonical_bone name that isn't a real canonical name
//
// Every category is a real, checkable fact -- nothing here fabricates
// a missing bone or silently drops a mapping problem into "valid."
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ANIMATION/SkeletonSystem/Skeleton.h"
#include "RIG/CanonicalSkeleton.h"
#include "RIG/RigProfile.h"

namespace dominus::rig {

struct RigProfileReport {
    std::string profile_id;
    std::vector<std::string> mapped;               // "legacy_bone -> canonical_bone"
    std::vector<std::string> missing;               // canonical bone names
    std::vector<std::string> extra;                 // legacy bone names
    std::vector<std::string> hierarchy_conflicts;    // descriptive strings
    std::vector<std::string> unresolved;             // descriptive strings
    bool valid = false;  // true iff missing/hierarchy_conflicts/unresolved are all empty
};

class RigProfileValidator {
public:
    static RigProfileReport Validate(const animation::Skeleton& legacySkeleton, const RigProfile& profile) {
        RigProfileReport report;
        report.profile_id = profile.profile_id;

        for (const auto& m : profile.mappings) {
            bool legacyExists = static_cast<bool>(legacySkeleton.FindBoneIndex(m.legacy_bone));
            bool canonicalKnown = IsCanonicalBoneName(m.canonical_bone);

            if (!legacyExists) {
                report.unresolved.push_back(m.legacy_bone + " -> " + m.canonical_bone +
                                             " (legacy bone not found on this skeleton)");
                continue;
            }
            if (!canonicalKnown) {
                report.unresolved.push_back(m.legacy_bone + " -> " + m.canonical_bone +
                                             " (canonical_bone is not a recognized canonical name)");
                continue;
            }
            report.mapped.push_back(m.legacy_bone + " -> " + m.canonical_bone);
        }

        // missing: canonical bones no mapping ever addresses.
        for (const auto& canonical : CanonicalSkeletonBones()) {
            if (!profile.LegacyFor(canonical.name)) {
                report.missing.push_back(canonical.name);
            }
        }

        // extra: legacy skeleton bones the profile never mentions.
        for (const auto& bone : legacySkeleton.Bones()) {
            if (!profile.CanonicalFor(bone.name)) {
                report.extra.push_back(bone.name);
            }
        }

        // hierarchy_conflicts: for every successfully-resolved mapping,
        // the legacy bone's REAL parent must itself map (via this same
        // profile) to the canonical bone's REQUIRED parent.
        for (const auto& m : profile.mappings) {
            auto legacyIdx = legacySkeleton.FindBoneIndex(m.legacy_bone);
            if (!legacyIdx || !IsCanonicalBoneName(m.canonical_bone)) continue;  // already reported unresolved

            const std::string& requiredCanonicalParent = CanonicalParentByName().at(m.canonical_bone);
            const auto& legacyBone = legacySkeleton.Bones()[static_cast<size_t>(*legacyIdx)];

            std::string actualLegacyParentName;
            if (legacyBone.parent_index >= 0) {
                actualLegacyParentName = legacySkeleton.Bones()[static_cast<size_t>(legacyBone.parent_index)].name;
            }

            if (requiredCanonicalParent.empty()) {
                // this canonical bone must be the root.
                if (!actualLegacyParentName.empty()) {
                    report.hierarchy_conflicts.push_back(m.canonical_bone + " (via '" + m.legacy_bone +
                                                           "') must be the root, but its legacy parent is '" +
                                                           actualLegacyParentName + "'");
                }
                continue;
            }

            std::optional<std::string> actualCanonicalOfParent;
            if (!actualLegacyParentName.empty()) {
                actualCanonicalOfParent = profile.CanonicalFor(actualLegacyParentName);
            }

            if (!actualCanonicalOfParent || *actualCanonicalOfParent != requiredCanonicalParent) {
                report.hierarchy_conflicts.push_back(
                    m.canonical_bone + " (via '" + m.legacy_bone + "') requires parent '" + requiredCanonicalParent +
                    "', but its legacy parent '" + (actualLegacyParentName.empty() ? "(root)" : actualLegacyParentName) +
                    "' maps to '" + (actualCanonicalOfParent ? *actualCanonicalOfParent : "(unmapped)") + "'");
            }
        }

        report.valid = report.missing.empty() && report.hierarchy_conflicts.empty() && report.unresolved.empty();
        return report;
    }
};

}  // namespace dominus::rig
