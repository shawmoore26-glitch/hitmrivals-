// RIG/CanonicalSkeleton.h
// DOMINUS RIG v1.0: "This is the skeleton contract. Make the asset
// conform to it" -- the inverse of "generate a skeleton that fits this
// asset." The mandatory bone set below (24 bones) plus a documented,
// pattern-matched extension allow-list is the one canonical hierarchy
// every character SHOULD eventually conform to.
//
// Scope, stated plainly: this file defines the CONTRACT. It does not
// retroactively enforce it. Brooklyn's real skeleton (extended in the
// prior "bone rig fix" phase to root/torso/head/arm_r/arm_l/leg_r/
// leg_l) does not conform -- different names, no spine subdivision, no
// clavicle/upperarm/forearm/hand breakdown, no thigh/calf/foot/toe
// breakdown. That's not hidden or worked around here: RigAuthorityValidator
// (RIG/RigAuthorityValidator.h) is a real, standalone, opt-in check --
// running it against Brooklyn's current skeleton is EXPECTED to fail,
// and does (see ROADMAP.md for the actual, verified output). Migrating
// Brooklyn/generic_biped onto this canonical hierarchy would touch
// every move/hurtbox/hitbox/animation-clip fixture that currently
// references the old bone names directly -- a real, separate migration
// effort, not attempted here. This phase ships the CONTRACT and the
// VALIDATOR; it does not silently rewrite 35+ dependent test fixtures
// to match it.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace dominus::rig {

// One entry per mandatory bone: its own name and its REQUIRED parent's
// name (empty string means "must be the skeleton's root, parent
// null"). 24 bones -- root/pelvis, an explicit 3-segment lower spine
// (spine_01-03) plus chest (distinct from Brooklyn's single "torso"
// segment on purpose: a real production rig separates lower-spine
// bend from chest/ribcage rotation), neck+head, both arms fully
// articulated (clavicle->upperarm->forearm->hand), both legs fully
// articulated (thigh->calf->foot->toe).
struct CanonicalBone {
    std::string name;
    std::string required_parent;  // "" means "must be the root (parent == null)"
};

inline const std::vector<CanonicalBone>& CanonicalSkeletonBones() {
    static const std::vector<CanonicalBone> kBones = {
        {"root", ""},
        {"pelvis", "root"},
        {"spine_01", "pelvis"},
        {"spine_02", "spine_01"},
        {"spine_03", "spine_02"},
        {"chest", "spine_03"},
        {"neck", "chest"},
        {"head", "neck"},
        {"clavicle_L", "chest"},
        {"upperarm_L", "clavicle_L"},
        {"forearm_L", "upperarm_L"},
        {"hand_L", "forearm_L"},
        {"clavicle_R", "chest"},
        {"upperarm_R", "clavicle_R"},
        {"forearm_R", "upperarm_R"},
        {"hand_R", "forearm_R"},
        {"thigh_L", "pelvis"},
        {"calf_L", "thigh_L"},
        {"foot_L", "calf_L"},
        {"toe_L", "foot_L"},
        {"thigh_R", "pelvis"},
        {"calf_R", "thigh_R"},
        {"foot_R", "calf_R"},
        {"toe_R", "foot_R"},
    };
    return kBones;
}

inline const std::unordered_map<std::string, std::string>& CanonicalParentByName() {
    static const std::unordered_map<std::string, std::string> kMap = [] {
        std::unordered_map<std::string, std::string> m;
        for (const auto& b : CanonicalSkeletonBones()) m[b.name] = b.required_parent;
        return m;
    }();
    return kMap;
}

// Extension bones: optional, never counted against "required bones
// present," but still checked for real structural validity (a real
// parent, a unique name) when present. Fixed exact names for
// singular features; "_L"/"_R" suffix families and numbered families
// (finger_*, thumb_*, cloth_*, tail_*, wing_*) are prefix-matched
// rather than individually enumerated, since a real character can
// legitimately have finger_01_L through finger_04_L, or three tail
// segments, or none at all -- enumerating every possible count would
// be inventing a limit this engine has no authority to set.
inline const std::vector<std::string>& ExtensionBoneExactNames() {
    static const std::vector<std::string> kNames = {
        "jaw", "eye_L", "eye_R", "weapon_socket", "hand_socket", "foot_socket",
        "shoulder_ctrl", "elbow_ctrl", "knee_ctrl",
    };
    return kNames;
}

inline const std::vector<std::string>& ExtensionBonePrefixFamilies() {
    static const std::vector<std::string> kPrefixes = {
        "finger_", "thumb_", "cloth_", "tail_", "wing_",
    };
    return kPrefixes;
}

inline bool IsCanonicalBoneName(const std::string& name) { return CanonicalParentByName().count(name) > 0; }

inline bool IsKnownExtensionBoneName(const std::string& name) {
    for (const auto& exact : ExtensionBoneExactNames()) {
        if (name == exact) return true;
    }
    for (const auto& prefix : ExtensionBonePrefixFamilies()) {
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return false;
}

}  // namespace dominus::rig
