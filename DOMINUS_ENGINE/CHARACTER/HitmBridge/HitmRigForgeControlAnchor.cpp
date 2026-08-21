// CHARACTER/HitmBridge/HitmRigForgeControlAnchor.cpp
#include "CHARACTER/HitmBridge/HitmRigForgeControlAnchor.h"

#include <utility>

namespace dominus::character::hitm {

namespace {

const HitmDerivedBoneAnchor* FindBone(const std::vector<HitmDerivedBoneAnchor>& bones, const std::string& name) {
    for (const auto& b : bones) {
        if (b.bone_name == name) return &b;
    }
    return nullptr;
}

const HitmAtlasPart* FindAtlasPart(const HitmPartsRig& partsRig, const std::string& name) {
    for (const auto& p : partsRig.Parts()) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

// The one real child's own absolute pivot point, in whole-sprite space --
// computed directly from ITS OWN real rect+pivot, never from its derived
// `.at` (see this file's header comment for why: avoids compounding
// Phase R1a's own unproven-until-tested hypothesis with a second one).
std::pair<double, double> WholeSpritePivotPoint(const HitmPartPlacement& part) {
    const double x = part.rect_x0 + part.pivot_x * (part.rect_x1 - part.rect_x0);
    const double y = part.rect_y0 + part.pivot_y * (part.rect_y1 - part.rect_y0);
    return {x, y};
}

}  // namespace

core::Result<HitmControlBoneAnchorDerivation> DeriveControlBoneAnchorFromSingleChild(
    const std::string& controlBoneName, const std::string& parentPartName,
    const std::vector<HitmDerivedBoneAnchor>& derivedBones, const HitmPartsRig& partsRig,
    const HitmRigPlacement& placement, double displayHeight) {
    // Exactly one real child, verified against the actual data -- not
    // assumed true because it happened to hold for one fighter.
    std::vector<const HitmDerivedBoneAnchor*> children;
    for (const auto& b : derivedBones) {
        if (b.parent.has_value() && *b.parent == controlBoneName) children.push_back(&b);
    }
    if (children.size() != 1) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail(
            "HitmRigForgeControlAnchor: '" + controlBoneName + "' has " + std::to_string(children.size()) +
            " real children, not exactly 1 -- this derivation is only defined for a control bone with a single real "
            "child (see this file's header comment; a bone like 'hip' with several real children is a different, "
            "already-resolved case -- see HITM_RIG_FORGE_R1B1_REPORT.md)");
    }
    const HitmDerivedBoneAnchor& child = *children.front();
    if (!child.part.has_value()) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail("HitmRigForgeControlAnchor: '" + controlBoneName +
                                                                     "'s one real child '" + child.bone_name +
                                                                     "' owns no drawn part -- cannot derive from it");
    }
    const HitmPartPlacement* childPlace = placement.Part(*child.part);
    if (childPlace == nullptr) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail(
            "HitmRigForgeControlAnchor: real part '" + *child.part + "' (child of '" + controlBoneName +
            "') missing from placement");
    }

    const HitmDerivedBoneAnchor* parentBone = FindBone(derivedBones, parentPartName);
    if (parentBone == nullptr || !parentBone->at_x.has_value() || !parentBone->at_y.has_value()) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail(
            "HitmRigForgeControlAnchor: '" + parentPartName + "' (real parent of '" + controlBoneName +
            "') has no already-derived .at -- this derivation requires Phase R1a's own precondition to already hold "
            "for it");
    }
    const HitmPartPlacement* parentPlace = placement.Part(parentPartName);
    const HitmAtlasPart* parentAtlas = FindAtlasPart(partsRig, parentPartName);
    if (parentPlace == nullptr || parentAtlas == nullptr) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail("HitmRigForgeControlAnchor: real part '" +
                                                                     parentPartName + "' missing from placement/partsRig");
    }
    if (partsRig.SourceHeight() == 0) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail("HitmRigForgeControlAnchor: sourceHeight is zero");
    }

    const double spriteW =
        static_cast<double>(partsRig.SourceWidth()) / static_cast<double>(partsRig.SourceHeight()) * displayHeight;

    // world[parentPartName] -- a direct, un-recursed application of the
    // real algorithm's own root-case formula. Valid because
    // `parentPartName`'s own `.at` is ALREADY known (whole-sprite space,
    // Phase R1a's own convention for a part-owning bone whose real
    // parent is a control bone) and, per this file's header comment (and
    // HITM_RIG_FORGE_R1B1_REPORT.md's own proof), every ancestor between
    // `parentPartName` and the real skeleton's root (root, hip) is
    // provably irrelevant to `parentPartName`'s own absolute position --
    // it telescopes to exactly this formula, not an approximation of it.
    const double worldParentX = (*parentBone->at_x - 0.5) * spriteW;
    const double worldParentY = (*parentBone->at_y - 1.0) * displayHeight;

    // The real, independently-known target this control bone's own
    // `else`-branch position is solved to coincide with: the one child's
    // own absolute pivot point, computed directly from ITS OWN real
    // rect+pivot (never from its derived `.at`).
    const auto [childPivotX, childPivotY] = WholeSpritePivotPoint(*childPlace);
    const double targetX = (childPivotX - 0.5) * spriteW;
    const double targetY = (childPivotY - 1.0) * displayHeight;

    const double parentWidthPx = parentAtlas->norm_w * spriteW;
    const double parentHeightPx = parentAtlas->norm_h * displayHeight;
    if (parentWidthPx == 0.0 || parentHeightPx == 0.0) {
        return core::Result<HitmControlBoneAnchorDerivation>::Fail("HitmRigForgeControlAnchor: '" + parentPartName +
                                                                     "' has zero real rendered width or height");
    }

    // NOTE, load-bearing for how this value must be validated (see
    // tests/integration/test_hitm_rig_forge_control_anchor.cpp and
    // HITM_RIG_FORGE_R1B1_REPORT.md): this solves ONLY the `else`-branch
    // reading of `.at` (the control bone's own real parent, which owns a
    // part) -- i.e. it makes `world[controlBoneName]` (as computed via
    // `parentPartName`) coincide exactly with the child's own real pivot
    // point. It deliberately does NOT also solve the `if`-branch reading
    // (how this SAME raw value gets read when the control bone serves as
    // ITS OWN children's parent) -- those two readings are real,
    // evidenced, and INCOMPATIBLE for a single stored number, per this
    // file's header comment. This function returns the physically-
    // motivated candidate; the caller (this phase's own test) is
    // responsible for validating it against the real, full FK
    // composition and reporting exactly how far it falls short.
    HitmControlBoneAnchorDerivation out;
    out.control_bone_name = controlBoneName;
    out.parent_part_name = parentPartName;
    out.single_child_part_name = *child.part;
    out.at_x = parentPlace->pivot_x + (targetX - worldParentX) / parentWidthPx;
    out.at_y = parentPlace->pivot_y + (targetY - worldParentY) / parentHeightPx;
    return core::Result<HitmControlBoneAnchorDerivation>::Ok(out);
}

}  // namespace dominus::character::hitm
