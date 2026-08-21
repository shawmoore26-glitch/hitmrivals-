// CHARACTER/HitmBridge/HitmRigForgeAnchor.h
// DOMINUS Rig Forge Phase R1a -- tightly scoped: implement and test the
// `.at` derivation hypothesis HITM_RIG_FORGE_AUDIT.md section 5 names,
// for PART-OWNING bones only. Does not touch control bones (root/hip/
// neck/shoulderFar/shoulderNear) -- that decision is explicitly Phase
// R1b's, per the audit's own section 6 and the checkpoint that
// authorized this phase. Never invents a value here: a bone this file
// cannot derive `.at` for is left std::nullopt, not defaulted, not
// estimated from its children.
//
// WHAT `.at` IS AND WHY IT HAS TWO SPACES (see HITM_RIG_FORGE_AUDIT.md
// section 3/4 for the full derivation from the real, authoritative
// `engine/render/SkeletonSystem.js`): every real bone in `rig.json`'s
// `bones[]` array needs a per-bone anchor coordinate, `.at`, that the
// real `build()` function reads but that appears in NO real checked-in
// JSON for any of the three fighters. Its meaning depends on the bone's
// own real parent:
//   - parent is a control bone (owns no drawn part): `.at` is normalized
//     in WHOLE-SPRITE space -- the same (0..1, 0..1 across the full
//     `assets/sprites/<fighter>.png`) space `design.json`'s own
//     `core_parts`/`features` rects already use.
//   - parent owns a drawn part P: `.at` is normalized WITHIN P's own
//     rect -- the same 0..1 space P's own `pivot` already uses.
//
// THE DERIVATION HYPOTHESIS (real, disclosed, testable -- proven or
// disproven by Phase R1a's own test, tests/integration/
// test_hitm_rig_forge_at.cpp, not asserted here): a bone that owns a
// real drawn part already has a real, pixel-verified `rect` + `pivot`
// (`design.json`/`rig.json`/`rig_validation.json` all independently
// agree on both -- see the audit). That part's own pivot POINT --
// `rect0 + pivot*(rect1-rect0)`, converted into whole-sprite space -- is
// a real, evidenced candidate for that bone's `.at`, re-expressed in
// whichever of the two spaces above its own parent's type requires.
//
// This file does NOT run the real FK algorithm or compare against
// HitmSceneBridge -- that is HitmSkeletonFk.h/.cpp and
// test_hitm_rig_forge_at.cpp's job. This file only computes the
// candidate anchor values themselves, as a pure, real-data-only
// function -- the smallest independently-testable unit of the
// hypothesis.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>

namespace dominus::character::hitm {

// One real bone, `.part`/`.at` resolved as far as real data (Phase R0) or
// this file's own disclosed hypothesis (Phase R1a) permits.
struct HitmDerivedBoneAnchor {
    std::string bone_name;
    std::optional<std::string> parent;  // rig.json's own real parent, verbatim (nullopt only for "root")

    // `.part` -- mechanically, losslessly derived (Phase R0, proven):
    // `bone_name` if it names a real key of rig.json's `parts` map, else
    // nullopt (a real control bone).
    std::optional<std::string> part;

    // Real, derived topology fact -- true iff `parent` names a bone that
    // itself has no `part` (i.e. a control bone). False for a part-owning
    // parent. Meaningless (left false) when `parent` is nullopt (root).
    bool parent_is_control_bone = false;

    // `.at` -- Phase R1a's disclosed, testable hypothesis (see this
    // file's header comment). Present if and only if `part` is present:
    // a control bone (`part == nullopt`) NEVER gets a fabricated `.at`
    // here, regardless of what its children need -- that is exactly the
    // gap HITM_RIG_FORGE_AUDIT.md section 6 named and this phase does
    // not close.
    std::optional<double> at_x;
    std::optional<double> at_y;
};

// Derives `.part` + the candidate `.at` for every real bone in `rig`'s
// own `bones()` (parts.json's/rig.json's identical real array -- see
// HITM_RIG_FORGE_AUDIT.md section 3), using `placement`'s real rect/pivot
// data (rig.json's `parts` map, already cross-validated against `rig`'s
// own pivot at import time -- see HitmRigPlacement.h).
//
// Duplicate bone names (the real, evidenced `handFar`/`handNear`
// rigid+follow-overlay quirk -- see HitmPartsRig.h's top comment)
// resolve last-occurrence-wins, matching the real engine's own
// `bones[b.name]=b` plain-object semantics and the same precedent
// `HitmSpriteDrawData.h`'s `ApplySecondaryMotion()` already established
// for this exact quirk. The output vector is in first-appearance order
// (matching the real engine's own `order.push(b.name)` -- see
// `engine/render/SkeletonSystem.js`'s `register()`), one entry per
// unique name.
//
// Fails (Result::Fail) only on malformed real data this file cannot
// safely divide by (a real part whose own `rect` or its parent's `rect`
// has zero width/height) -- never on "control bone with no `.at`", which
// is not a failure, it is this phase's own disclosed, expected boundary.
core::Result<std::vector<HitmDerivedBoneAnchor>> DeriveBoneAnchors(const HitmPartsRig& rig,
                                                                      const HitmRigPlacement& placement);

}  // namespace dominus::character::hitm
