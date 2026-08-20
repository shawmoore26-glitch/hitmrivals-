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
            // idle/walk/jump/blockingStance -- real `f.animT`, a counter
            // that resets to 0 on every state transition (Module 5A's
            // `HitmFighterSnapshot::state_frame`, the deliberately
            // scoped extension this file's header comment describes;
            // see its own header comment for exactly what it is and is
            // not). This was `snap.frame` (the match-wide monotonic
            // counter) before that extension existed -- a walk that
            // started mid-cycle rather than at its own frame 0. Fixed,
            // not papered over.
            return static_cast<double>(snap.state_frame);
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

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

// Real bone lookup semantics: the real engine keys bones by a plain
// object (`bones[b.name] = b`), so a repeated name's LAST source-order
// entry silently wins. Module 3's `HitmPartsRig::Bones()` is
// deliberately an order-preserving sequence with duplicates kept (see
// its own header comment on why a map would be lossy there); this
// function builds the map ONLY where this file needs real-engine-
// equivalent last-wins semantics, without touching that class or its
// losslessness guarantee.
std::map<std::string, const HitmBoneEntry*> BoneByNameLastWins(const HitmPartsRig& parts) {
    std::map<std::string, const HitmBoneEntry*> byName;
    for (const auto& b : parts.Bones()) byName[b.name] = &b;  // later entries overwrite, matching real semantics
    return byName;
}

// Every real bone's local pose at the sampled frame -- not just drawn
// parts. Needed because secondary motion's parent-rotation lookups must
// reach control bones (hip/torso/neck/head/shoulderFar/shoulderNear)
// that carry no visual part of their own but do carry real anim.json
// tracks, exactly matching hitm-engine's own `pose()`, which samples
// `for (const name of r.order)` -- every bone, not a drawn-parts subset.
std::map<std::string, HitmLocalPose> SampleAllBones(const HitmAnimationClip& clip,
                                                      const std::map<std::string, const HitmBoneEntry*>& boneByName,
                                                      double sampledFrame) {
    std::map<std::string, HitmLocalPose> localPose;
    for (const auto& [name, bonePtr] : boneByName) {
        (void)bonePtr;
        localPose[name] = clip.Sample(name, sampledFrame);
    }
    return localPose;
}

// Direct, line-by-line port of hitm-engine's real
// `SkeletonSystem._secondary()` -- see this file's header comment for
// the exact real quirks (the `||`-as-fallback fields, last-name-wins
// bone lookup, persistent per-bone spring state) this replicates
// deliberately rather than diverging from.
void ApplySecondaryMotion(const std::map<std::string, const HitmBoneEntry*>& boneByName,
                           std::map<std::string, HitmLocalPose>& localPose, HitmSecondaryMotionState& state) {
    for (const auto& [name, bonePtr] : boneByName) {
        const HitmBoneEntry& b = *bonePtr;
        if (!b.follow.has_value()) continue;
        const HitmBoneFollow& f = *b.follow;

        double parentRot = 0.0;
        if (b.parent.has_value()) {
            auto it = localPose.find(*b.parent);
            if (it != localPose.end()) parentRot = it->second.rotation_deg;
        }

        double lagBeats = f.lag_beats != 0.0 ? f.lag_beats : 1.0;  // real `f.lagBeats || 1`
        double target = parentRot * lagBeats;

        HitmSpringState& s0 = state.BoneState(name);
        if (!s0.initialized) {
            s0.angle_deg = target;
            s0.velocity = 0.0;
            s0.initialized = true;
        }

        double stiffness = f.stiffness != 0.0 ? f.stiffness : 0.2;  // real `f.stiffness || 0.2`
        s0.velocity += (target - s0.angle_deg) * stiffness;
        if (f.gravity != 0.0) {
            s0.velocity += f.gravity * 0.6 * std::sin(s0.angle_deg * kDegToRad);
        }
        double damping = f.damping != 0.0 ? f.damping : 0.7;  // real `f.damping || 0.7`
        s0.velocity *= damping;
        s0.angle_deg += s0.velocity * 1.0;  // real `dt || 1` -- one real frame per BuildSpriteDrawData call, see header

        double maxAngle = f.max_angle != 0.0 ? f.max_angle : 30.0;  // real `f.maxAngle || 30`
        if (s0.angle_deg > maxAngle) {
            s0.angle_deg = maxAngle;
            s0.velocity *= -0.35;
        }
        if (s0.angle_deg < -maxAngle) {
            s0.angle_deg = -maxAngle;
            s0.velocity *= -0.35;
        }

        localPose[name] = HitmLocalPose{s0.angle_deg - parentRot, 0.0, 0.0};
    }
}

}  // namespace

Result<HitmSpriteDrawData> BuildSpriteDrawData(const HitmFighterSnapshot& snapshot, const HitmMoveInstance* currentMove,
                                                 const HitmAssetBundle& bundle, HitmSecondaryMotionState* secondaryMotion) {
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

    // Full-bone local pose, not just drawn parts -- secondary motion's
    // parent-rotation lookups need control bones too (hip/torso/neck/
    // head/shoulderFar/shoulderNear), exactly matching real `pose()`.
    auto boneByName = BoneByNameLastWins(bundle.parts);
    auto localPose = SampleAllBones(*clip, boneByName, draw.sampled_frame);
    if (secondaryMotion) {
        ApplySecondaryMotion(boneByName, localPose, *secondaryMotion);
    }

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

        // localPose always has an entry for partName: boneByName (which
        // seeded it) is built from bundle.parts.Bones(), and drawOrder
        // (HitmPartsRig::Import's own invariant) only ever names parts
        // that also have a real bone entry -- but a bundle assembled by
        // hand without going through HitmAssetImporter could violate
        // that, so this is still checked rather than assumed.
        auto poseIt = localPose.find(partName);
        HitmLocalPose pose = poseIt != localPose.end() ? poseIt->second : clip->Sample(partName, draw.sampled_frame);

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
