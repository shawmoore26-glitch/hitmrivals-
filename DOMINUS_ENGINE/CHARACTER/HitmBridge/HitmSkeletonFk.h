// CHARACTER/HitmBridge/HitmSkeletonFk.h
// DOMINUS Rig Forge Phase R1a -- a faithful, generic port of hitm-engine's
// real, authoritative `engine/render/SkeletonSystem.js` `build()` function
// (quoted in full in HITM_RIG_FORGE_AUDIT.md section 3). This is the "FK"
// stage of the Rig Forge target pipeline the checkpoint that authorized
// this phase specified:
//
//   design + parts + derived .at -> DOMINUS Rig Forge -> FK -> part placement
//
// FAITHFUL, NOT CONVENIENT: every formula below is copied from the real
// algorithm's own real per-axis scale choices, including the one this
// phase's own investigation found HitmSceneBridge does NOT replicate --
// `spriteW = sourceWidth/sourceHeight*displayHeight`, a real, per-fighter,
// non-uniform X-axis scale (HitmSceneBridge instead uses one uniform
// `displayHeight` for both axes -- a real, disclosed, ALREADY-PROVEN,
// deliberate choice for THAT module's own placement convention, not a bug
// -- see HitmSceneBridge.h's own header comment). This file does not
// silently reconcile the two; see test_hitm_rig_forge_at.cpp for where
// that real divergence is measured and reported, not corrected.
//
// TWO ENTRY POINTS, deliberately kept separate:
//
//   ComputeLocalOffset() -- the real algorithm's per-bone `ox,oy`
//   computation in isolation. Requires only the bone's own derived `.at`
//   and its PARENT's real pivot/size (if the parent owns a part) -- NEVER
//   requires a control bone's `.at`. This is the only entry point Phase
//   R1a's own comparison test can actually exercise against real fighter
//   data, because (a real, evidenced, topology-driven finding -- see
//   HITM_RIG_FORGE_AUDIT.md's Phase R1a report) every real part-owning
//   bone's own real parent is either another part-owning bone (this case)
//   or a control bone (the next case, which needs an anchor this phase
//   does not author).
//
//   BuildWorldTransforms() -- the real algorithm's full recursive
//   world-position walk, exactly as `SkeletonSystem.build()` performs it.
//   Genuine, reusable infrastructure for a future phase once control-bone
//   anchors exist (Phase R1b) -- but for THIS phase, called against a real
//   fighter's Phase R1a-derived bones (no control-bone `.at`), it is
//   EXPECTED, PROVEN-BY-TEST behavior for this to fail immediately at
//   `root` (the very first bone in real topological order, and every real
//   fighter's real root never owns a drawn part) -- a real, executable
//   demonstration that the real FK algorithm's absolute posing is
//   completely blocked without control-bone anchors, not a defect in this
//   file. It never defaults a missing anchor to zero; it fails, naming the
//   exact bone.
#pragma once

#include <map>
#include <string>

#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "CHARACTER/HitmBridge/HitmRigForgeAnchor.h"
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>

namespace dominus::character::hitm {

// Real per-bone local pose sample -- matches the real engine's own
// `localPose[name] = {rot, dx, dy}` shape (`anim.json`'s real sampled
// rotation/offset, already imported by `HitmAnimationSet`; this file does
// not sample anim.json itself, a caller supplies whatever pose it wants
// posed). A bone with no entry is treated as the real zero pose, matching
// `lp = localPose[name] || {rot:0,dx:0,dy:0}`.
struct HitmFkLocalPose {
    double rot_deg = 0.0;
    double dx = 0.0;
    double dy = 0.0;
};
using HitmFkLocalPoseMap = std::map<std::string, HitmFkLocalPose>;

// Real fighter-local-space offset from the parent's own world position,
// BEFORE the parent's accumulated rotation is applied -- exactly the
// algorithm's own `ox, oy` (see this file's header comment).
struct HitmFkOffset {
    double ox = 0.0;
    double oy = 0.0;
};

// Real fighter-local-space world transform -- origin at the fighter's own
// anchor point, +y downward, exactly as `SkeletonSystem.build()`'s own
// doc comment specifies ("Coordinates are in fighter-local space: origin
// at the feet, +y downward").
struct HitmFkWorldTransform {
    double x = 0.0;
    double y = 0.0;
    double rot_rad = 0.0;
};

class HitmSkeletonFk {
public:
    // The real algorithm's per-bone offset computation, in isolation --
    // see this file's header comment for exactly which real bones this
    // can be exercised against without a control-bone `.at`.
    //
    // `bone` must have a real, derived `.at` (Result::Fail otherwise --
    // this function never computes an offset FOR a control bone, only
    // FROM one). `parentBone` is `bone`'s own real parent's derived
    // entry: if `parentBone.part` is present, `partsRig`/`placement` must
    // contain that part (real data, already cross-validated at import);
    // if `parentBone.part` is absent (a control bone), `parentBone` must
    // itself carry a real `.at` (Result::Fail otherwise -- this is
    // exactly Phase R1b's gap, surfaced here as a named failure, not a
    // fabricated zero).
    static core::Result<HitmFkOffset> ComputeLocalOffset(const HitmDerivedBoneAnchor& bone,
                                                            const HitmDerivedBoneAnchor& parentBone,
                                                            const HitmPartsRig& partsRig,
                                                            const HitmRigPlacement& placement, double sourceWidth,
                                                            double sourceHeight, double displayHeight);

    // The real algorithm's full recursive world-position walk -- see this
    // file's header comment for why, against a real fighter's Phase
    // R1a-derived bones, this is expected to fail at `root`.
    static core::Result<std::map<std::string, HitmFkWorldTransform>> BuildWorldTransforms(
        const std::vector<HitmDerivedBoneAnchor>& bones, const HitmPartsRig& partsRig, const HitmRigPlacement& placement,
        const HitmFkLocalPoseMap& localPose, double sourceWidth, double sourceHeight, double displayHeight);
};

}  // namespace dominus::character::hitm
