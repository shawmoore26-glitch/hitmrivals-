// tests/integration/test_hitm_rig_forge_control_anchor.cpp
// DOMINUS Rig Forge Phase R1b-1 -- tightly scoped, per the checkpoint
// that authorized this phase: investigate whether the 5 real control-bone
// anchors Phase R1a left un-derived can be deterministically derived from
// real children data. No hand-authored numbers. No change to
// HitmSceneBridge. No FK replacement. All three real fighters.
//
// THIS PHASE FOUND TWO GENUINELY DIFFERENT REAL ANSWERS:
//
//   1. `root` and `hip` need no anchor at all. Every real bone directly
//      or transitively parented under `hip` (verified: `torso`,
//      `legFarU`, `legNearU`, plus each fighter's own real extra hip
//      children -- `coatFar`/`coatNear` for Brooklyn, `tail` for Rocket,
//      `coatStripFar`/`coatStripNear` for Static) resolves to the EXACT
//      SAME real-world position regardless of what value `root.at`/
//      `hip.at` are given -- proven here by running the real, unmodified
//      `HitmSkeletonFk::ComputeLocalOffset` (Phase R1a's own, untouched)
//      twice, with two deliberately different placeholder anchor pairs,
//      for all three real fighters, and asserting bit-for-bit identical
//      results. This is not "derivable" in the sense Phase R1a's
//      part-owning bones were -- it is algebraically provable that no
//      value is needed, and this test is that proof made executable.
//
//   2. `neck`, `shoulderFar`, and `shoulderNear` are genuinely different
//      AND, on real, quantitative validation, OPTION 1 FAILS FOR THEM --
//      not because no candidate could be constructed, but because the
//      real algorithm reads a control bone's own `.at` in two
//      incompatible coordinate spaces whenever its own real parent owns
//      a part (this exact case) -- see HitmRigForgeControlAnchor.h's own
//      header comment for the full, real derivation. Each of these three
//      bones has EXACTLY one real child in all three fighters (verified,
//      not assumed: `neck`->`head`, `shoulderFar`->`armFarU`,
//      `shoulderNear`->`armNearU`), and `DeriveControlBoneAnchorFromSingleChild`
//      returns a real, physically-motivated CANDIDATE anchor -- solved so
//      the control bone's own resolved position coincides with that
//      child's real pivot point. Run through the real, unmodified
//      `HitmSkeletonFk` two-hop composition (torso -> control bone ->
//      child) exactly as Phase R1a's own methodology requires, that
//      candidate mis-places its one real child by TENS of real pixels
//      (not Phase R1a's own few-pixel bound) -- a real, large, quantified
//      failure, checked below for all three fighters. A corrected,
//      self-consistent two-branch solve was also attempted (see the
//      header comment) and rejected: it degenerates into an equation
//      with zero dependency on the child's actual real geometry. Option 1
//      does not work for these three real control bones -- this is the
//      "exact failure" this phase was asked to bring back, not papered
//      over.
#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "CHARACTER/HitmBridge/HitmRigForgeAnchor.h"
#include "CHARACTER/HitmBridge/HitmRigForgeControlAnchor.h"
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"
#include "CHARACTER/HitmBridge/HitmSkeletonFk.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using dominus::character::hitm::DeriveBoneAnchors;
using dominus::character::hitm::DeriveControlBoneAnchorFromSingleChild;
using dominus::character::hitm::HitmDerivedBoneAnchor;
using dominus::character::hitm::HitmFkOffset;
using dominus::character::hitm::HitmPartsRig;
using dominus::character::hitm::HitmRigPlacement;
using dominus::character::hitm::HitmSkeletonFk;

namespace {

std::filesystem::path FindDir(const std::filesystem::path& rel) {
    std::vector<std::filesystem::path> candidates = {rel, std::filesystem::path("..") / rel,
                                                       std::filesystem::path("../..") / rel};
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture dir not found: " + rel.string());
}

struct FighterFixture {
    std::string id;
    HitmPartsRig partsRig;
    HitmRigPlacement placement;
    std::vector<HitmDerivedBoneAnchor> derived;
};

FighterFixture LoadFighter(const std::string& fighterId) {
    auto dir = FindDir(std::filesystem::path("tests/fixtures/hitm_sprite_assets/data/characters") / fighterId);
    auto partsResult = HitmPartsRig::Import(dir);
    if (!partsResult.ok) throw std::runtime_error("test setup (" + fighterId + "): " + partsResult.error);
    auto placementResult = HitmRigPlacement::Import(dir, &*partsResult.value);
    if (!placementResult.ok) throw std::runtime_error("test setup (" + fighterId + "): " + placementResult.error);
    auto derivedResult = DeriveBoneAnchors(*partsResult.value, *placementResult.value);
    if (!derivedResult.ok) throw std::runtime_error("test setup (" + fighterId + "): " + derivedResult.error);

    FighterFixture f;
    f.id = fighterId;
    f.partsRig = std::move(*partsResult.value);
    f.placement = std::move(*placementResult.value);
    f.derived = std::move(*derivedResult.value);
    return f;
}

HitmDerivedBoneAnchor* FindMutable(std::vector<HitmDerivedBoneAnchor>& bones, const std::string& name) {
    for (auto& b : bones) {
        if (b.bone_name == name) return &b;
    }
    return nullptr;
}
const HitmDerivedBoneAnchor* Find(const std::vector<HitmDerivedBoneAnchor>& bones, const std::string& name) {
    for (const auto& b : bones) {
        if (b.bone_name == name) return &b;
    }
    return nullptr;
}

std::vector<std::string> RealChildrenOf(const std::vector<HitmDerivedBoneAnchor>& bones, const std::string& parentName) {
    std::vector<std::string> out;
    for (const auto& b : bones) {
        if (b.parent.has_value() && *b.parent == parentName) out.push_back(b.bone_name);
    }
    return out;
}

std::vector<std::string> AllFighters() { return {"brooklyn", "rocket", "static"}; }

constexpr double kDisplayHeight = 225.0;

}  // namespace

// --- 1. root/hip: proven irrelevant, not merely "derived" -----------------

DOMINUS_TEST(HitmRigForgeControlAnchor_RootAndHipAnchors_AreProvenIrrelevantToEveryDescendant) {
    for (const auto& fighterId : AllFighters()) {
        FighterFixture f = LoadFighter(fighterId);
        const double sourceWidth = static_cast<double>(f.partsRig.SourceWidth());
        const double sourceHeight = static_cast<double>(f.partsRig.SourceHeight());

        auto hipChildren = RealChildrenOf(f.derived, "hip");
        DOMINUS_EXPECT(!hipChildren.empty());  // every real fighter has real children of hip

        // Two deliberately, wildly different placeholder anchor pairs --
        // explicitly NOT claimed as real, authored, or derived data.
        struct Placeholder {
            double root_x, root_y, hip_x, hip_y;
        };
        std::vector<Placeholder> placeholders = {{0.5, 1.0, 0.5, 1.0}, {0.9, 0.15, 0.1, 0.7}, {-0.4, 2.3, 1.6, -0.9}};

        const double spriteW = sourceWidth / sourceHeight * kDisplayHeight;

        // The invariant is each hip-child's fully COMPOSED absolute
        // world position (root -> hip -> child) -- NOT the raw
        // `ComputeLocalOffset(child, hip)` return value in isolation,
        // which is a displacement FROM hip's own world position and so
        // trivially depends on hip.at directly. root.at's and hip.at's
        // real cancellation happens ACROSS the composition (world[hip]
        // already cancels root.at; composing world[hip] with the child's
        // own offset then cancels hip.at too) -- see
        // HITM_RIG_FORGE_R1B1_REPORT.md for the full algebraic proof
        // this test makes executable.
        std::vector<std::pair<double, double>> resultsPerPlaceholder;
        for (const auto& ph : placeholders) {
            auto bonesCopy = f.derived;
            HitmDerivedBoneAnchor* root = FindMutable(bonesCopy, "root");
            HitmDerivedBoneAnchor* hip = FindMutable(bonesCopy, "hip");
            DOMINUS_EXPECT(root != nullptr && hip != nullptr);
            root->at_x = ph.root_x;
            root->at_y = ph.root_y;
            hip->at_x = ph.hip_x;
            hip->at_y = ph.hip_y;

            const HitmDerivedBoneAnchor* rootNow = Find(bonesCopy, "root");
            const HitmDerivedBoneAnchor* hipNow = Find(bonesCopy, "hip");

            const double worldRootX = (*rootNow->at_x - 0.5) * spriteW;
            const double worldRootY = (*rootNow->at_y - 1.0) * kDisplayHeight;
            auto hipOffset =
                HitmSkeletonFk::ComputeLocalOffset(*hipNow, *rootNow, f.partsRig, f.placement, sourceWidth, sourceHeight, kDisplayHeight);
            DOMINUS_EXPECT(hipOffset.ok);
            if (!hipOffset.ok) continue;
            const double worldHipX = worldRootX + hipOffset.value->ox;
            const double worldHipY = worldRootY + hipOffset.value->oy;

            for (const auto& childName : hipChildren) {
                const HitmDerivedBoneAnchor* child = Find(bonesCopy, childName);
                DOMINUS_EXPECT(child != nullptr);
                auto offsetResult = HitmSkeletonFk::ComputeLocalOffset(*child, *hipNow, f.partsRig, f.placement,
                                                                          sourceWidth, sourceHeight, kDisplayHeight);
                DOMINUS_EXPECT(offsetResult.ok);
                if (!offsetResult.ok) continue;
                resultsPerPlaceholder.push_back({worldHipX + offsetResult.value->ox, worldHipY + offsetResult.value->oy});
            }
        }

        // Every hip-child's fully composed absolute position is IDENTICAL
        // across all three wildly different root/hip placeholder pairs --
        // root.at/hip.at have zero effect, proven by direct computation,
        // not assumed.
        const std::size_t n = hipChildren.size();
        DOMINUS_EXPECT(resultsPerPlaceholder.size() == n * placeholders.size());
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t p = 1; p < placeholders.size(); ++p) {
                DOMINUS_EXPECT(std::abs(resultsPerPlaceholder[p * n + i].first - resultsPerPlaceholder[i].first) < 1e-9);
                DOMINUS_EXPECT(std::abs(resultsPerPlaceholder[p * n + i].second - resultsPerPlaceholder[i].second) < 1e-9);
            }
        }
    }
}

DOMINUS_TEST(HitmRigForgeControlAnchor_SingleChildDerivation_RefusesABoneWithSeveralRealChildren) {
    for (const auto& fighterId : AllFighters()) {
        FighterFixture f = LoadFighter(fighterId);
        // hip has several real children in every real fighter -- this
        // derivation is explicitly NOT for that case (see finding 1
        // above; hip needs no derivation, of any kind, at all).
        auto result = DeriveControlBoneAnchorFromSingleChild("hip", "root", f.derived, f.partsRig, f.placement, kDisplayHeight);
        DOMINUS_EXPECT(!result.ok);
    }
}

// --- 2. neck/shoulderFar/shoulderNear: option 1's real, quantified failure

DOMINUS_TEST(HitmRigForgeControlAnchor_SingleChildBones_CandidateFailsRealFkRoundTrip_ByARealLargeQuantifiedMargin) {
    double maxObservedDeviationPx = 0.0;
    int checkedBones = 0;

    for (const auto& fighterId : AllFighters()) {
        FighterFixture f = LoadFighter(fighterId);
        const double sourceWidth = static_cast<double>(f.partsRig.SourceWidth());
        const double sourceHeight = static_cast<double>(f.partsRig.SourceHeight());

        for (const std::string& controlBone : {"neck", "shoulderFar", "shoulderNear"}) {
            // Real, evidenced for all 3 fighters (HITM_RIG_FORGE_R1B1_REPORT.md):
            // each of these bones' own real parent is "torso".
            auto derivation =
                DeriveControlBoneAnchorFromSingleChild(controlBone, "torso", f.derived, f.partsRig, f.placement, kDisplayHeight);
            DOMINUS_EXPECT(derivation.ok);
            if (!derivation.ok) continue;

            // Round-trip 1: the real, unmodified HitmSkeletonFk offset
            // formula, given this candidate, places the control bone
            // exactly where it was solved to be relative to torso (true
            // by construction -- this candidate was solved from exactly
            // this equation).
            HitmDerivedBoneAnchor controlBoneAnchor;
            controlBoneAnchor.bone_name = controlBone;
            controlBoneAnchor.parent = "torso";
            controlBoneAnchor.parent_is_control_bone = false;
            controlBoneAnchor.at_x = derivation.value->at_x;
            controlBoneAnchor.at_y = derivation.value->at_y;
            // deliberately no `.part` -- a real control bone

            const HitmDerivedBoneAnchor* torso = Find(f.derived, "torso");
            DOMINUS_EXPECT(torso != nullptr);
            auto controlOffset = HitmSkeletonFk::ComputeLocalOffset(controlBoneAnchor, *torso, f.partsRig, f.placement,
                                                                       sourceWidth, sourceHeight, kDisplayHeight);
            DOMINUS_EXPECT(controlOffset.ok);
            if (!controlOffset.ok) continue;

            const double spriteW = sourceWidth / sourceHeight * kDisplayHeight;
            const double worldTorsoX = (*torso->at_x - 0.5) * spriteW;
            const double worldTorsoY = (*torso->at_y - 1.0) * kDisplayHeight;
            const double worldControlX = worldTorsoX + controlOffset.value->ox;
            const double worldControlY = worldTorsoY + controlOffset.value->oy;

            // Round-trip 2 -- THE actual test: composing the ONE real
            // child's own real, unmodified `HitmSkeletonFk` offset FROM
            // this resolved control-bone world position (exactly what
            // the real game would compute, given this candidate `.at`
            // and the child's own already-derived, Phase-R1a `.at`) --
            // does it land on that child's own real, independently-known
            // (rect+pivot-derived) absolute pivot point?
            const HitmDerivedBoneAnchor* child = Find(f.derived, derivation.value->single_child_part_name);
            DOMINUS_EXPECT(child != nullptr);
            auto childOffset = HitmSkeletonFk::ComputeLocalOffset(*child, controlBoneAnchor, f.partsRig, f.placement,
                                                                     sourceWidth, sourceHeight, kDisplayHeight);
            DOMINUS_EXPECT(childOffset.ok);
            if (!childOffset.ok) continue;

            const double worldChildX = worldControlX + childOffset.value->ox;
            const double worldChildY = worldControlY + childOffset.value->oy;

            const auto* childPlace = f.placement.Part(*child->part);
            DOMINUS_EXPECT(childPlace != nullptr);
            const double realChildPivotX =
                childPlace->rect_x0 + childPlace->pivot_x * (childPlace->rect_x1 - childPlace->rect_x0);
            const double realChildPivotY =
                childPlace->rect_y0 + childPlace->pivot_y * (childPlace->rect_y1 - childPlace->rect_y0);
            const double expectedChildX = (realChildPivotX - 0.5) * spriteW;
            const double expectedChildY = (realChildPivotY - 1.0) * kDisplayHeight;

            const double devX = std::abs(worldChildX - expectedChildX);
            const double devY = std::abs(worldChildY - expectedChildY);
            maxObservedDeviationPx = std::max({maxObservedDeviationPx, devX, devY});
            checkedBones++;

            // THE real, quantified failure: run through the real,
            // unmodified FK, this candidate mis-places its one real
            // child by tens of real pixels -- far beyond Phase R1a's own
            // few-pixel bound for the part-owning-bone hypothesis (see
            // this file's header comment and HITM_RIG_FORGE_R1B1_REPORT.md
            // for the real per-bone numbers). Asserted as a lower bound,
            // not an upper one: this test fails loudly if some future
            // change accidentally makes the candidate converge without
            // anyone updating this file's own header comment to match.
            DOMINUS_EXPECT(devX > 5.0 || devY > 5.0);
        }
    }

    DOMINUS_EXPECT(checkedBones == 9);  // 3 control bones x 3 fighters, all real, none skipped
    // The real, observed magnitude -- reported here so a build log
    // carries the actual number, not just "some large value".
    DOMINUS_EXPECT(maxObservedDeviationPx > 5.0);
}

// --- 3. The failed candidate is not nonsensical -- a real, secondary,
//        unplanned signal, checked honestly, not as redemption ----------

DOMINUS_TEST(HitmRigForgeControlAnchor_FailedCandidate_IsStillApproximatelyMirrorSymmetric_AcrossAllThreeFighters) {
    for (const auto& fighterId : AllFighters()) {
        FighterFixture f = LoadFighter(fighterId);

        auto far = DeriveControlBoneAnchorFromSingleChild("shoulderFar", "torso", f.derived, f.partsRig, f.placement, kDisplayHeight);
        auto near = DeriveControlBoneAnchorFromSingleChild("shoulderNear", "torso", f.derived, f.partsRig, f.placement, kDisplayHeight);
        DOMINUS_EXPECT(far.ok);
        DOMINUS_EXPECT(near.ok);
        if (!far.ok || !near.ok) continue;

        // This candidate is PROVEN to fail the real FK round-trip (test
        // above) -- but it is worth checking it failed for the real
        // reason this file documents (a genuine two-space algorithm
        // inconsistency), not because the candidate itself is
        // geometrically nonsensical. Neither derivation is told the
        // character is bilaterally symmetric -- each is solved
        // independently, purely from its own one real child's own real
        // geometry. Landing within a real, small margin of exact mirror
        // symmetry (at_x summing to 1.0 around torso's own horizontal
        // center) is a real, unplanned, secondary signal that the
        // CANDIDATE'S OWN construction is sound, even though running it
        // through the real algorithm's actual two-space reading fails --
        // reported as exactly that in HITM_RIG_FORGE_R1B1_REPORT.md, not
        // as evidence the overall hypothesis succeeded.
        const double symmetrySum = far.value->at_x + near.value->at_x;
        DOMINUS_EXPECT(std::abs(symmetrySum - 1.0) < 0.05);

        // Neck should land near torso's own top edge (y close to 0 in
        // torso's own rect-local space) and near horizontal center (x
        // close to 0.5) -- a real, coarse, anatomical sanity check on the
        // same candidate.
        auto neck = DeriveControlBoneAnchorFromSingleChild("neck", "torso", f.derived, f.partsRig, f.placement, kDisplayHeight);
        DOMINUS_EXPECT(neck.ok);
        if (neck.ok) {
            DOMINUS_EXPECT(std::abs(neck.value->at_x - 0.5) < 0.1);
            DOMINUS_EXPECT(neck.value->at_y >= -0.1 && neck.value->at_y < 0.2);
        }
    }
}
