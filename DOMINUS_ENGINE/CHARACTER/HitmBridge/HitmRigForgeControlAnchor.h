// CHARACTER/HitmBridge/HitmRigForgeControlAnchor.h
// DOMINUS Rig Forge Phase R1b-1 -- tightly scoped, per the checkpoint
// that authorized this phase: investigate whether the 5 real control-bone
// anchors Phase R1a deliberately left un-derived (root/hip/neck/
// shoulderFar/shoulderNear) can be deterministically derived from real
// children data, treated with the same discipline as Phase R1a's own
// `.at` hypothesis -- hypothesis, derivation, all three fighters, run
// through the real FK, quantitative validation. No hand-authored numbers.
// No change to HitmSceneBridge. No FK replacement.
//
// THIS PHASE FOUND TWO GENUINELY DIFFERENT ANSWERS FOR THE 5 CONTROL
// BONES, NOT ONE -- see HITM_RIG_FORGE_R1B1_REPORT.md for the full
// derivation and real numbers; summarized here because it drives this
// file's own two-part shape:
//
//   root and hip need NO anchor at all -- not "derivable," PROVABLY
//   IRRELEVANT. Both bones' own real parent (root has none; hip's is
//   root) is itself a control bone, so `HitmSkeletonFk`'s own real
//   offset formula (`ox = (b.at-0.5)*spriteW - (pb.at-0.5)*spriteW`)
//   telescopes: every real descendant's absolute position reduces
//   algebraically to a function of ITS OWN ancestors' `.at` values with
//   root's and hip's own contribution cancelling out EXACTLY, for any
//   value either could take. This file adds no code for that half of
//   the finding -- it is proven directly against the real, unmodified
//   `HitmSkeletonFk::ComputeLocalOffset` in
//   tests/integration/test_hitm_rig_forge_control_anchor.cpp, by
//   plugging in two deliberately different placeholder values and
//   showing zero effect on any real descendant, for all three fighters.
//
//   neck, shoulderFar, and shoulderNear are different: their own real
//   parent (torso, for all three fighters) owns a real drawn part, so
//   the telescoping cancellation above does NOT apply to them (a
//   different, real, algebraically-provable fact -- also detailed in
//   the report). Each has EXACTLY one real child in all three fighters
//   (`neck`->`head`, `shoulderFar`->`armFarU`, `shoulderNear`->`armNearU`
//   -- verified, not assumed), which `DeriveControlBoneAnchorFromSingleChild`
//   below uses for a real, physically-motivated candidate: solve for the
//   `.at` value that makes the control bone's own resolved position
//   (via the real `else`-branch offset from its real parent part, torso)
//   coincide exactly with its one child's own real, independently-known
//   (rect+pivot-derived, never `.at`-derived) absolute pivot point.
//
//   THIS CANDIDATE FAILS Phase R1a's own validation discipline, for a
//   real, deep, structural reason -- not a bug, and not glossed over:
//   the real algorithm reads a control bone's OWN `.at` value in TWO
//   INCOMPATIBLE COORDINATE SPACES whenever that control bone's own
//   parent owns a part (exactly this case). Resolving the control
//   bone's OWN position (as `b`, parent = torso, which owns a part)
//   reads `.at` as normalized WITHIN TORSO'S OWN RECT (the `else`
//   branch, what this function solves for). Resolving any of ITS
//   CHILDREN's position (the control bone now playing `pb`, and
//   `!pb.part` is true because it owns no part) reads the SAME raw
//   stored `.at` value as normalized in WHOLE-SPRITE space (the `if`
//   branch) -- a real, evidenced, non-obvious consequence of the real
//   algorithm's own branch-selection rule, which depends only on
//   whether the PARENT owns a part, never on what THAT parent's own
//   ancestor looks like. One raw number, two genuinely different real
//   readings, both simultaneously active whenever this function's
//   candidate is actually run through the real FK composition
//   (`HitmSkeletonFk`, unmodified) -- proven, with real numbers, to
//   mis-place the one real child by tens of real pixels, not a few, in
//   `tests/integration/test_hitm_rig_forge_control_anchor.cpp` and
//   `HITM_RIG_FORGE_R1B1_REPORT.md`. A corrected, self-consistent
//   two-branch solve was also attempted and rejected: because Phase
//   R1a's own convention already defines the child's `.at` to equal its
//   own real pivot point EXACTLY (zero error, by construction, whenever
//   the child's parent is a control bone), the "self-consistent" solve
//   degenerates into an equation with no dependency on the child's own
//   real geometry at all -- it produces the IDENTICAL number for `neck`,
//   `shoulderFar`, and `shoulderNear` regardless of their three
//   genuinely different real children, a red flag caught and reported,
//   not shipped. There is no single real number that is both physically
//   motivated by the child's real position and self-consistent with the
//   real algorithm's own two-space reading. This function returns the
//   physically-motivated candidate specifically so its real, quantified
//   failure remains inspectable -- callers must not treat its return
//   value as validated data.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "CHARACTER/HitmBridge/HitmRigForgeAnchor.h"
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>

namespace dominus::character::hitm {

// An UNVALIDATED candidate -- see this file's header comment for why its
// physically-motivated construction does not survive being run through
// the real, full FK composition. Never treat `at_x`/`at_y` here as
// proven derived data the way Phase R1a's part-owning-bone `.at` values
// are; this struct exists so the candidate's real, quantified failure
// stays inspectable, not to hand callers a usable anchor.
struct HitmControlBoneAnchorDerivation {
    std::string control_bone_name;
    std::string parent_part_name;        // the real part-owning bone this control bone's real parent is (e.g. "torso")
    std::string single_child_part_name;   // the one real child this candidate was solved from
    double at_x = 0.0;                    // UNVALIDATED candidate, in parent_part_name's own real rect-local space
    double at_y = 0.0;                    // (matches HitmRigForgeAnchor's own convention for a part-parented bone)
};

// Solves for the UNVALIDATED candidate described in this file's header
// comment -- makes `controlBoneName`'s own `else`-branch position (via
// `parentPartName`) coincide exactly with its one real child's own real
// pivot point. Does NOT validate the result against the real algorithm's
// separate `if`-branch reading of the same value -- callers (this
// phase's own tests) must do that themselves via `HitmSkeletonFk`.
//
// Fails (Result::Fail, never a fabricated fallback) unless:
//   - `controlBoneName` has EXACTLY one real child in `derivedBones`
//     (verified per-fighter, not assumed);
//   - that one child owns a real drawn part (this file never derives a
//     control bone's candidate from another UN-derived control bone);
//   - `parentPartName` names a real part present in both `partsRig` and
//     `placement`, and its own `HitmDerivedBoneAnchor` (in
//     `derivedBones`) already carries a derived `.at` (true for any real
//     part-owning bone whose own parent is a control bone -- torso, for
//     all three real fighters).
core::Result<HitmControlBoneAnchorDerivation> DeriveControlBoneAnchorFromSingleChild(
    const std::string& controlBoneName, const std::string& parentPartName,
    const std::vector<HitmDerivedBoneAnchor>& derivedBones, const HitmPartsRig& partsRig,
    const HitmRigPlacement& placement, double displayHeight);

}  // namespace dominus::character::hitm
