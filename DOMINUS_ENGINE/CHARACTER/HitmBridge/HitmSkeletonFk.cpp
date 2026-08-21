// CHARACTER/HitmBridge/HitmSkeletonFk.cpp
#include "CHARACTER/HitmBridge/HitmSkeletonFk.h"

#include <cmath>
#include <optional>

namespace dominus::character::hitm {

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180.0;  // real engine's own `DEG = Math.PI/180`

const HitmAtlasPart* FindAtlasPart(const HitmPartsRig& partsRig, const std::string& name) {
    for (const auto& p : partsRig.Parts()) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

const HitmDerivedBoneAnchor* FindBone(const std::vector<HitmDerivedBoneAnchor>& bones, const std::string& name) {
    for (const auto& b : bones) {
        if (b.bone_name == name) return &b;
    }
    return nullptr;
}

}  // namespace

core::Result<HitmFkOffset> HitmSkeletonFk::ComputeLocalOffset(const HitmDerivedBoneAnchor& bone,
                                                                 const HitmDerivedBoneAnchor& parentBone,
                                                                 const HitmPartsRig& partsRig,
                                                                 const HitmRigPlacement& placement, double sourceWidth,
                                                                 double sourceHeight, double displayHeight) {
    if (!bone.at_x.has_value() || !bone.at_y.has_value()) {
        return core::Result<HitmFkOffset>::Fail("HitmSkeletonFk::ComputeLocalOffset: '" + bone.bone_name +
                                                  "' has no derived .at -- it is a real control bone (Phase R1a does not "
                                                  "derive control-bone anchors, see HITM_RIG_FORGE_AUDIT.md section 6)");
    }
    if (sourceHeight == 0.0) {
        return core::Result<HitmFkOffset>::Fail("HitmSkeletonFk::ComputeLocalOffset: sourceHeight is zero");
    }
    const double spriteW = sourceWidth / sourceHeight * displayHeight;

    if (!parentBone.part.has_value()) {
        // Real algorithm's first branch: parent is a control bone. Needs
        // the PARENT's own `.at`, in whole-sprite space -- exactly Phase
        // R1b's gap, surfaced as a named failure here, never a fabricated
        // zero.
        if (!parentBone.at_x.has_value() || !parentBone.at_y.has_value()) {
            return core::Result<HitmFkOffset>::Fail(
                "HitmSkeletonFk::ComputeLocalOffset: '" + bone.bone_name + "'s real parent '" + parentBone.bone_name +
                "' is a control bone with no authored/derived .at -- Phase R1a does not fabricate one (see "
                "HITM_RIG_FORGE_AUDIT.md section 6; this is Phase R1b's decision)");
        }
        HitmFkOffset offset;
        offset.ox = (*bone.at_x - 0.5) * spriteW - (*parentBone.at_x - 0.5) * spriteW;
        offset.oy = (*bone.at_y - 1.0) * displayHeight - (*parentBone.at_y - 1.0) * displayHeight;
        return core::Result<HitmFkOffset>::Ok(offset);
    }

    // Real algorithm's second branch: parent owns a real drawn part.
    const HitmPartPlacement* parentRect = placement.Part(*parentBone.part);
    const HitmAtlasPart* parentAtlas = FindAtlasPart(partsRig, *parentBone.part);
    if (parentRect == nullptr || parentAtlas == nullptr) {
        return core::Result<HitmFkOffset>::Fail("HitmSkeletonFk::ComputeLocalOffset: real part '" + *parentBone.part +
                                                  "' (parent of '" + bone.bone_name +
                                                  "') missing from placement/partsRig -- inconsistent real data");
    }
    const double parentWidthPx = parentAtlas->norm_w * spriteW;
    const double parentHeightPx = parentAtlas->norm_h * displayHeight;

    HitmFkOffset offset;
    offset.ox = (*bone.at_x - parentRect->pivot_x) * parentWidthPx;
    offset.oy = (*bone.at_y - parentRect->pivot_y) * parentHeightPx;
    return core::Result<HitmFkOffset>::Ok(offset);
}

core::Result<std::map<std::string, HitmFkWorldTransform>> HitmSkeletonFk::BuildWorldTransforms(
    const std::vector<HitmDerivedBoneAnchor>& bones, const HitmPartsRig& partsRig, const HitmRigPlacement& placement,
    const HitmFkLocalPoseMap& localPose, double sourceWidth, double sourceHeight, double displayHeight) {
    std::map<std::string, HitmFkWorldTransform> world;

    for (const auto& bone : bones) {
        HitmFkLocalPose lp;  // real zero-pose fallback, matching `localPose[name] || {rot:0,dx:0,dy:0}`
        auto lpIt = localPose.find(bone.bone_name);
        if (lpIt != localPose.end()) lp = lpIt->second;

        if (!bone.parent.has_value()) {
            // Real algorithm's root case: `world[name] = {x:(b.at[0]-0.5)*spriteW+..., ...}`.
            if (!bone.at_x.has_value() || !bone.at_y.has_value()) {
                return core::Result<std::map<std::string, HitmFkWorldTransform>>::Fail(
                    "HitmSkeletonFk::BuildWorldTransforms: '" + bone.bone_name +
                    "' is the real root bone and has no authored/derived .at -- absolute FK posing cannot start "
                    "without it (see HITM_RIG_FORGE_AUDIT.md section 6; this is Phase R1b's decision, not fabricated "
                    "here)");
            }
            const double spriteW = sourceWidth / sourceHeight * displayHeight;
            HitmFkWorldTransform t;
            t.x = (*bone.at_x - 0.5) * spriteW + lp.dx * displayHeight;
            t.y = (*bone.at_y - 1.0) * displayHeight + lp.dy * displayHeight;
            t.rot_rad = lp.rot_deg * kDeg;
            world[bone.bone_name] = t;
            continue;
        }

        auto parentWorldIt = world.find(*bone.parent);
        if (parentWorldIt == world.end()) {
            return core::Result<std::map<std::string, HitmFkWorldTransform>>::Fail(
                "HitmSkeletonFk::BuildWorldTransforms: '" + bone.bone_name + "'s real parent '" + *bone.parent +
                "' has no resolved world transform yet -- either it was never posed, or an earlier real ancestor "
                "already failed to resolve (see the earlier error in this Result chain)");
        }
        const HitmDerivedBoneAnchor* parentBone = FindBone(bones, *bone.parent);
        if (parentBone == nullptr) {
            return core::Result<std::map<std::string, HitmFkWorldTransform>>::Fail(
                "HitmSkeletonFk::BuildWorldTransforms: '" + bone.bone_name + "'s real parent '" + *bone.parent +
                "' is not present in the supplied bones list");
        }

        auto offsetResult =
            ComputeLocalOffset(bone, *parentBone, partsRig, placement, sourceWidth, sourceHeight, displayHeight);
        if (!offsetResult.ok) {
            return core::Result<std::map<std::string, HitmFkWorldTransform>>::Fail(offsetResult.error);
        }

        const HitmFkWorldTransform& p = parentWorldIt->second;
        const double c = std::cos(p.rot_rad);
        const double s = std::sin(p.rot_rad);
        const double ox = offsetResult.value->ox;
        const double oy = offsetResult.value->oy;

        HitmFkWorldTransform t;
        t.x = p.x + ox * c - oy * s + lp.dx * displayHeight;
        t.y = p.y + ox * s + oy * c + lp.dy * displayHeight;
        t.rot_rad = p.rot_rad + lp.rot_deg * kDeg;
        world[bone.bone_name] = t;
    }

    return core::Result<std::map<std::string, HitmFkWorldTransform>>::Ok(std::move(world));
}

}  // namespace dominus::character::hitm
