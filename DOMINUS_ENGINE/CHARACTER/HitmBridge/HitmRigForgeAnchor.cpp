// CHARACTER/HitmBridge/HitmRigForgeAnchor.cpp
#include "CHARACTER/HitmBridge/HitmRigForgeAnchor.h"

#include <map>
#include <utility>

namespace dominus::character::hitm {

namespace {

// This bone's own drawn part's pivot POINT, converted from "0..1 within
// that part's own rect" (the space `pivot` is always authored in) into
// whole-sprite-normalized space -- the one real, disclosed conversion
// this whole hypothesis rests on. See this file's header comment.
std::pair<double, double> WholeSpritePivotPoint(const HitmPartPlacement& part) {
    const double x = part.rect_x0 + part.pivot_x * (part.rect_x1 - part.rect_x0);
    const double y = part.rect_y0 + part.pivot_y * (part.rect_y1 - part.rect_y0);
    return {x, y};
}

}  // namespace

core::Result<std::vector<HitmDerivedBoneAnchor>> DeriveBoneAnchors(const HitmPartsRig& rig,
                                                                      const HitmRigPlacement& placement) {
    // Real, order-preserving, last-occurrence-wins dedup -- see this
    // file's header comment for why (matches HitmSpriteDrawData.h's own
    // established precedent for the same real handFar/handNear quirk).
    std::vector<std::string> order;
    std::map<std::string, const HitmBoneEntry*> byName;
    for (const auto& b : rig.Bones()) {
        if (byName.find(b.name) == byName.end()) order.push_back(b.name);
        byName[b.name] = &b;
    }

    std::vector<HitmDerivedBoneAnchor> out;
    out.reserve(order.size());

    for (const auto& name : order) {
        const HitmBoneEntry& b = *byName[name];
        HitmDerivedBoneAnchor derived;
        derived.bone_name = name;
        derived.parent = b.parent;

        const bool ownsPart = placement.HasPart(name);
        if (ownsPart) derived.part = name;

        if (b.parent.has_value()) {
            derived.parent_is_control_bone = !placement.HasPart(*b.parent);
        }

        if (!ownsPart) {
            // A real control bone -- no rect/pivot of its own to derive
            // `.at` from. Left absent, not fabricated. Phase R1b's job.
            out.push_back(std::move(derived));
            continue;
        }

        const HitmPartPlacement* self = placement.Part(name);
        if (self == nullptr) {
            return core::Result<std::vector<HitmDerivedBoneAnchor>>::Fail(
                "HitmRigForgeAnchor: '" + name + "' owns a part per placement.HasPart() but placement.Part() returned null "
                "-- inconsistent HitmRigPlacement, not a real data gap");
        }
        if (self->rect_x1 == self->rect_x0 || self->rect_y1 == self->rect_y0) {
            return core::Result<std::vector<HitmDerivedBoneAnchor>>::Fail(
                "HitmRigForgeAnchor: '" + name + "' has a zero-width or zero-height real rect -- cannot derive a pivot point");
        }

        const auto [wpx, wpy] = WholeSpritePivotPoint(*self);

        if (!b.parent.has_value() || derived.parent_is_control_bone) {
            // Parent is a control bone (or this bone is root, which never
            // owns a part in any real fighter -- handled generically
            // anyway): `.at` is stored in whole-sprite space directly.
            derived.at_x = wpx;
            derived.at_y = wpy;
        } else {
            // Parent owns a real part P: `.at` is stored as a fraction of
            // P's own real rect -- the same 0..1 space P's own `pivot`
            // already uses.
            const HitmPartPlacement* parentPart = placement.Part(*b.parent);
            if (parentPart == nullptr) {
                return core::Result<std::vector<HitmDerivedBoneAnchor>>::Fail(
                    "HitmRigForgeAnchor: '" + name + "'s real parent '" + *b.parent +
                    "' is not a control bone per placement.HasPart() but placement.Part() returned null for it");
            }
            if (parentPart->rect_x1 == parentPart->rect_x0 || parentPart->rect_y1 == parentPart->rect_y0) {
                return core::Result<std::vector<HitmDerivedBoneAnchor>>::Fail(
                    "HitmRigForgeAnchor: '" + name + "'s real parent '" + *b.parent +
                    "' has a zero-width or zero-height real rect -- cannot re-express '" + name + "'s anchor within it");
            }
            derived.at_x = (wpx - parentPart->rect_x0) / (parentPart->rect_x1 - parentPart->rect_x0);
            derived.at_y = (wpy - parentPart->rect_y0) / (parentPart->rect_y1 - parentPart->rect_y0);
        }

        out.push_back(std::move(derived));
    }

    return core::Result<std::vector<HitmDerivedBoneAnchor>>::Ok(std::move(out));
}

}  // namespace dominus::character::hitm
