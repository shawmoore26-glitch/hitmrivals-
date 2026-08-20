// CHARACTER/HitmBridge/HitmSpriteDrawData.cpp
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"

#include <algorithm>
#include <cmath>

namespace dominus::character::hitm {

using core::Result;

core::Result<std::string> SelectClipName(HitmFighterState state) {
    // Direct port of hitm-engine's AnimationSystem.js `clipFor()` --
    // see this file's header comment for every documented divergence.
    switch (state) {
        case HitmFighterState::kIdle:
            return Result<std::string>::Ok("idle");
        case HitmFighterState::kWalking:
            return Result<std::string>::Ok("walk");  // real code also has 'walkBack' -- no facing model here, see header
        case HitmFighterState::kJumping:
            // clip resolved by caller (needs snap.velocity_y); this
            // overload only covers state-determined clips.
            return Result<std::string>::Fail("SelectClipName: kJumping needs velocity_y -- use the snapshot-aware overload");
        case HitmFighterState::kBlockingStance:
        case HitmFighterState::kBlockstun:
            return Result<std::string>::Ok("block");  // real STATE enum has no separate blockstun clip, see header
        case HitmFighterState::kHitstun:
            return Result<std::string>::Ok("hurt");
        case HitmFighterState::kAttackStartup:
        case HitmFighterState::kAttackActive:
        case HitmFighterState::kAttackRecovery:
            return Result<std::string>::Fail("SelectClipName: attack states need the current move's move_key -- use BuildSpriteDrawData");
    }
    return Result<std::string>::Fail("SelectClipName: unhandled HitmFighterState");
}

namespace {

// Full clip-name resolution, including the two cases the header-exposed
// SelectClipName() above cannot handle alone (kJumping needs vy; attack
// states need the move key).
Result<std::string> ResolveClipName(const HitmFighterSnapshot& snap, const HitmMoveInstance* currentMove) {
    switch (snap.state) {
        case HitmFighterState::kJumping:
            return Result<std::string>::Ok(snap.velocity_y < 0.0f ? "jumpUp" : "jumpDown");
        case HitmFighterState::kAttackStartup:
        case HitmFighterState::kAttackActive:
        case HitmFighterState::kAttackRecovery:
            if (!currentMove) {
                return Result<std::string>::Fail("ResolveClipName: fighter is in an attack state but no currentMove was given");
            }
            return Result<std::string>::Ok(currentMove->move_key);
        default:
            return SelectClipName(snap.state);
    }
}

// Real, evidenced elapsed-frame reconstruction for the three attack sub-
// states -- see this file's header comment for the exact-match evidence
// (Brooklyn's real "special" clip len == startup+active+recovery, 36).
double ElapsedInMove(const HitmFighterSnapshot& snap, const combat::FrameData& frames) {
    int priorTotal = 0;
    int subPhaseTotal = 0;
    switch (snap.state) {
        case HitmFighterState::kAttackStartup:
            priorTotal = 0;
            subPhaseTotal = frames.startup;
            break;
        case HitmFighterState::kAttackActive:
            priorTotal = frames.startup;
            subPhaseTotal = frames.active;
            break;
        case HitmFighterState::kAttackRecovery:
            priorTotal = frames.startup + frames.active;
            subPhaseTotal = frames.recovery;
            break;
        default:
            return 0.0;  // unreachable given this function's only call site
    }
    int elapsedInSubPhase = subPhaseTotal - snap.state_frames_remaining;
    return static_cast<double>(priorTotal + elapsedInSubPhase);
}

// Direct port of hitm-engine's AnimationSystem.js `frameFor()`.
double ComputeRawFrame(const HitmFighterSnapshot& snap, const HitmMoveInstance* currentMove, const HitmAnimationClip& clip) {
    switch (snap.state) {
        case HitmFighterState::kAttackStartup:
        case HitmFighterState::kAttackActive:
        case HitmFighterState::kAttackRecovery:
            // currentMove non-null is already guaranteed by the caller
            // (ResolveClipName would have failed otherwise).
            return ElapsedInMove(snap, currentMove->move_def.frames);
        case HitmFighterState::kHitstun:
        case HitmFighterState::kBlockstun: {
            // Math.max(0, clip.len - stunRemaining) -- real port.
            double raw = static_cast<double>(clip.len) - static_cast<double>(snap.state_frames_remaining);
            return raw < 0.0 ? 0.0 : raw;
        }
        default:
            // idle/walk/jump/blockingStance -- see this file's header
            // comment for the documented animT-vs-match-frame gap.
            return static_cast<double>(snap.frame);
    }
}

double SampledFrame(const HitmAnimationClip& clip, double rawFrame) {
    // Direct port of hitm-engine's SkeletonSystem.js `pose()`:
    // `clip.loop ? (frame % clip.len) : Math.min(frame, clip.len)`.
    if (clip.loop) {
        double m = std::fmod(rawFrame, static_cast<double>(clip.len));
        return m < 0.0 ? m + clip.len : m;  // real JS % keeps sign of dividend; rawFrame is never negative here, kept for safety
    }
    return std::min(rawFrame, static_cast<double>(clip.len));
}

}  // namespace

Result<HitmSpriteDrawData> BuildSpriteDrawData(const HitmFighterSnapshot& snapshot, const HitmMoveInstance* currentMove,
                                                 const HitmAssetBundle& bundle) {
    auto clipNameResult = ResolveClipName(snapshot, currentMove);
    if (!clipNameResult.ok) {
        return Result<HitmSpriteDrawData>::Fail("BuildSpriteDrawData: " + clipNameResult.error);
    }

    const HitmAnimationClip* clip = bundle.animations.Clip(*clipNameResult.value);
    if (!clip) {
        return Result<HitmSpriteDrawData>::Fail("BuildSpriteDrawData: fighter '" + bundle.fighter_id +
                                                  "' has no real anim.json clip named '" + *clipNameResult.value + "'");
    }

    HitmSpriteDrawData draw;
    draw.clip_name = *clipNameResult.value;
    draw.raw_frame = ComputeRawFrame(snapshot, currentMove, *clip);
    draw.sampled_frame = SampledFrame(*clip, draw.raw_frame);

    draw.parts.reserve(bundle.parts.DrawOrder().size());
    for (const auto& partName : bundle.parts.DrawOrder()) {
        const HitmAtlasPart* atlasPart = nullptr;
        for (const auto& p : bundle.parts.Parts()) {
            if (p.name == partName) { atlasPart = &p; break; }
        }
        if (!atlasPart) {
            // HitmPartsRig::Import already guarantees every drawOrder
            // entry names a real part -- unreachable, but never trust a
            // pointer without checking it.
            return Result<HitmSpriteDrawData>::Fail("BuildSpriteDrawData: drawOrder names unknown part '" + partName + "'");
        }
        const HitmPartPlacement* placement = bundle.placement.Part(partName);
        if (!placement) {
            // HitmRigPlacement::Import (called with a cross-check) already
            // guarantees this too -- unreachable via HitmAssetImporter,
            // defensive for any caller that builds a bundle by hand.
            return Result<HitmSpriteDrawData>::Fail("BuildSpriteDrawData: part '" + partName + "' has no rig.json placement");
        }

        HitmLocalPose pose = clip->Sample(partName, draw.sampled_frame);

        HitmPartDraw pd;
        pd.part_name = partName;
        pd.frame_x = atlasPart->frame_x;
        pd.frame_y = atlasPart->frame_y;
        pd.frame_w = atlasPart->frame_w;
        pd.frame_h = atlasPart->frame_h;
        pd.norm_w = atlasPart->norm_w;
        pd.norm_h = atlasPart->norm_h;
        pd.pivot_x = atlasPart->pivot_x;
        pd.pivot_y = atlasPart->pivot_y;
        pd.place_x = placement->rect_x0;
        pd.place_y = placement->rect_y0;
        pd.pose_rotation_deg = pose.rotation_deg;
        pd.pose_offset_x = pose.offset_x;
        pd.pose_offset_y = pose.offset_y;
        draw.parts.push_back(pd);
    }

    return Result<HitmSpriteDrawData>::Ok(std::move(draw));
}

}  // namespace dominus::character::hitm
