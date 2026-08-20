// CHARACTER/HitmBridge/HitmSpriteDrawData.h
// ROADMAP.md Track H Module 5B -- real HITM sprite/texture integration,
// Phase 1 (CPU-observable). This is the "ANIMATION/FRAME SELECTION" and
// "SPRITE DRAW DATA" stages of the pipeline:
//
//   REAL HITM ASSETS -> HitmAssetImporter -> DOMINUS asset representation
//   -> Brooklyn runtime (Module 5A, untouched) -> ANIMATION/FRAME
//   SELECTION (this file) -> SPRITE DRAW DATA (this file) -> renderer
//
// Pure functions only. Nothing here mutates a HitmFighterRuntime, opens a
// file, or touches WORLD/PHYSICS -- it consumes the runtime's already-
// public `HitmFighterSnapshot` (Module 5A, unmodified) plus a
// `HitmAssetBundle` (this module) and derives which atlas region, at what
// pose, each real drawn part should show this frame. Deterministic by
// construction: same snapshot + same bundle => byte-identical output,
// forever -- exactly the property Module 5A's own runtime already
// guarantees one level down.
//
// CLIP SELECTION AND FRAME SELECTION ARE DIRECT, VERIFIED PORTS of
// hitm-engine's own real, working `AnimationSystem.js` (`clipFor()` /
// `frameFor()`) -- not independently invented conventions. Every
// divergence from the real algorithm below is because DOMINUS's Module
// 5A runtime does not (yet) track a real equivalent of the field the
// real engine reads, and is called out explicitly rather than silently
// approximated:
//
//   - `clipFor(WALK)` real logic picks 'walk' vs 'walkBack' from facing
//     vs. velocity direction. DOMINUS's Module 5A runtime has no facing
//     concept at all (single-fighter vertical slice, no opponent to face
//     -- see HITM_FIGHTER_RUNTIME_REPORT.md's own "NOT IMPLEMENTED: a
//     second fighter/opponent"). This module always selects 'walk'.
//   - `clipFor(IDLE)` real logic picks 'land' for a few frames right
//     after touching ground (`landT>0`). Module 5A's runtime transitions
//     kJumping -> kIdle on ground contact with no separate landing-
//     recovery timer. This module always selects 'idle'.
//   - `frameFor()`'s default branch (idle/walk/jump) returns the real
//     engine's `f.animT`, a counter that RESETS to 0 every time the
//     fighter's state changes. Module 5A's public `HitmFighterSnapshot`
//     only exposes `frame`, the match-wide monotonic frame counter (state
//     transitions for kIdle/kWalking/kJumping are not currently timed --
//     Module 5A only needed frame-accurate countdowns for combat/reaction
//     states, so that's all it built). This module uses `snap.frame`
//     directly. It is a real, deterministic, real-data-only mapping, but
//     it will not visually match what the real engine would show right
//     after a state transition (a walk that starts mid-cycle rather than
//     at frame 0). THE SMALLEST CORRECT EXTENSION: add a
//     `state_entry_frame` (or equivalent per-state elapsed counter) to
//     `HitmFighterRuntime::FrameState`, mirroring the real engine's
//     `animT`/`stateT` reset-on-transition convention -- a small, real,
//     specific Module 5A change, deliberately NOT made in this module per
//     the explicit instruction not to reopen Module 5A without a genuine
//     defect forcing it. This is a known limitation, not a defect this
//     module's own scope requires fixing.
//   - kAttackStartup/Active/Recovery and kHitstun/kBlockstun face no such
//     gap: Module 5A's real `state_frames_remaining` countdown, combined
//     with the currently-executing move's real
//     `frames.{startup,active,recovery}` (kAttackStartup/Active/
//     Recovery) or the selected clip's own real `len` (kHitstun/
//     kBlockstun, a direct port of the real engine's
//     `Math.max(0, 14 - f.stun)` HITSTUN branch), reconstructs the real
//     engine's own `f.move.t`/stun-elapsed values exactly -- verified,
//     for Brooklyn's real "special", against the real anim.json clip's
//     own authored `len` (36), which is EXACTLY
//     `startup(14)+active(4)+recovery(18)` -- not a coincidence this
//     module manufactured, a real authored correspondence it found.
//   - `kBlockstun` has no distinct clip in the real engine's own `STATE`
//     enum (see hitm-engine's `Fighter.js`) -- 'block' is the only real
//     blocking-related clip, so this module selects it for both
//     kBlockingStance and kBlockstun, with the stun-elapsed frame formula
//     applied only to kBlockstun (kBlockingStance, held indefinitely,
//     uses the same convention as idle/walk above).
//
// SECONDARY MOTION (Track A gap #1, closed): hitm-engine's real
// `SkeletonSystem._secondary()` drags every bone flagged `follow` in
// `parts.json`'s real `bones[]` array toward its parent's rotation
// through a spring/damper, so coats/dreads/chains/the jaw/glove-bounce
// arrive late and keep moving after the parent stops (real, authored
// per-bone `stiffness`/`damping`/`lagBeats`/`maxAngle`/`gravity` --
// already imported losslessly by Module 3's `HitmPartsRig`, unused by
// anything until now). `ApplySecondaryMotion()` below is a direct,
// line-by-line port of the real function, including its real quirks:
//   - `f.stiffness || 0.2` / `f.damping || 0.7` / `f.maxAngle || 30`:
//     JavaScript's `||` treats 0 as falsy, silently substituting the
//     default even for an explicit authored 0. Real `stiffness`/
//     `damping` are never 0 in any real fighter's data, but they are
//     NOT one universal constant either -- corrected from an earlier,
//     inaccurate claim here: each fighter has its own uniform pair,
//     evidenced across every one of that fighter's own real follow
//     bones (Brooklyn 0.118/0.634, Rocket 0.268/0.784, Static
//     0.184/0.652) -- matching `HitmBoneFollow`'s own header comment,
//     "Spring constants are DNA-derived: a heavy, inelastic body
//     produces a stiff, well-damped coat; a light chaotic one produces
//     a coat that never quite settles." `gravity`, by contrast, *is*
//     legitimately 0 for several real bones (e.g. every fighter's real
//     `handFar`/`handNear`/ears) -- exactly the case this fallback rule
//     exists for, faithfully replicated rather than silently diverging
//     from it.
//   - The real engine's bone lookup is a plain object keyed by name
//     (`bones[b.name] = b`), so when a name appears twice in the real
//     source array (Module 3's own documented finding: `handFar`/
//     `handNear` each appear twice, once rigid, once as a later
//     "glove bounce" follow overlay), the LATER entry silently wins.
//     `ApplySecondaryMotion()` replicates this exactly (last-occurrence-
//     wins per name) rather than Module 3's own lossless, order-
//     preserving `Bones()` sequence -- meaning handFar/handNear are
//     ALWAYS spring-driven here, matching real behavior, not a DOMINUS
//     choice.
//   - Per-fighter spring state (`{angle, velocity}` per follow bone)
//     genuinely persists across calls -- unlike everything else in this
//     file, secondary motion is NOT a pure function of one snapshot. A
//     caller owns a `HitmSecondaryMotionState` per fighter (mirroring
//     the real engine's own per-`fighterKey` `Map`) and must call
//     `BuildSpriteDrawData` exactly once per real simulation frame when
//     supplying one -- calling it more than once for the same frame
//     double-integrates the spring, the same discipline
//     `HitmFighterRuntime::AdvanceFrame()` already requires of its own
//     frame counter. Passing `nullptr` (the default) skips secondary
//     motion entirely -- every follow bone with no real track in the
//     selected clip then gets the zero pose exactly as before this
//     capability existed, unchanged, backward-compatible.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>

namespace dominus::character::hitm {

// Per-bone spring state -- `{angle, velocity}` in the real engine.
struct HitmSpringState {
    double angle_deg = 0.0;
    double velocity = 0.0;
    bool initialized = false;
};

// Owns one fighter's real secondary-motion spring state across frames.
// Mirrors the real engine's own per-`fighterKey` `Map` -- explicit,
// caller-owned state, the same discipline `HitmFighterRuntime` itself
// already uses, rather than a hidden global/static map.
class HitmSecondaryMotionState {
public:
    // Get-or-default-construct this bone's spring state -- matches the
    // real `st[name]` access pattern.
    HitmSpringState& BoneState(const std::string& boneName) { return bones_[boneName]; }

    // Matches the real engine's own `resetFollow()` -- call on round
    // reset so replays/re-runs stay deterministic from a known zero
    // state, exactly as the real engine's own comment specifies.
    void Reset() { bones_.clear(); }

private:
    std::map<std::string, HitmSpringState> bones_;
};

// One real part's fully-resolved draw instruction for this frame, in
// real drawOrder (back-to-front).
struct HitmPartDraw {
    std::string part_name;
    // Real atlas-pixel source rect (parts.json's real `frame`).
    int frame_x = 0;
    int frame_y = 0;
    int frame_w = 0;
    int frame_h = 0;
    // Real normalized size + pivot (parts.json).
    double norm_w = 0;
    double norm_h = 0;
    double pivot_x = 0;
    double pivot_y = 0;
    // Real normalized placement rect corner (rig.json's real `rect`,
    // matching hitm-engine's own real `tools/rig_render.py` convention --
    // see HitmAssetImporter.h / HitmRigPlacement.h for why this, not
    // SkeletonSystem.js's incomplete bone-hierarchy walk).
    double place_x = 0;
    double place_y = 0;
    // Real per-clip local pose sampled from anim.json at this frame
    // (zero if this part has no track in the selected clip -- matches
    // hitm-engine's own `_sample(null, f)` fallback exactly).
    double pose_rotation_deg = 0;
    double pose_offset_x = 0;
    double pose_offset_y = 0;
};

struct HitmSpriteDrawData {
    std::string clip_name;
    // The pre-loop/clamp candidate frame this module computed (see this
    // file's header comment) -- exposed for tests/debugging, not just the
    // post-adjustment sampled value.
    double raw_frame = 0;
    // What was actually fed to every `HitmAnimationClip::Sample()` call
    // this frame -- `clip.loop ? fmod(raw_frame, clip.len) : min(raw_frame, clip.len)`,
    // a direct port of hitm-engine's own `SkeletonSystem.pose()`.
    double sampled_frame = 0;
    // Real drawOrder, back-to-front.
    std::vector<HitmPartDraw> parts;
};

// `currentMove` is required (non-null) whenever `snapshot.state` is one
// of kAttackStartup/kAttackActive/kAttackRecovery, and must be the same
// real `HitmMoveInstance` whose frame data is currently driving that
// fighter's attack sub-state (Module 5A only ever runs one move -- the
// real "special" -- per HitmFighterRuntime::Create; a caller building
// this from the same identity record Module 5A used can simply
// re-extract it, a pure, deterministic, already-tested operation). Fails
// (Result::Fail) if `snapshot.state` requires a clip
// (`bundle.animations`) does not actually have -- e.g. a fighter missing
// the real 'hurt' clip -- rather than silently falling back to 'idle'
// the way the real engine's own `AnimationSystem.update()` does (that
// fallback is a rendering-robustness choice appropriate to a live game;
// this module's job is to prove what the real data supports, so a
// missing clip is a real, reportable gap, not something to paper over).
//
// `secondaryMotion`, if non-null, is both read and mutated -- see this
// file's header comment above for the exactly-once-per-real-frame
// calling discipline this requires. `nullptr` (the default) means no
// secondary motion: every follow bone falls back to whatever its own
// clip track provides (usually the zero pose -- real clips generally do
// not author follow-bone tracks directly, see the header comment),
// identical to this function's behavior before secondary motion existed.
core::Result<HitmSpriteDrawData> BuildSpriteDrawData(const HitmFighterSnapshot& snapshot, const HitmMoveInstance* currentMove,
                                                       const HitmAssetBundle& bundle,
                                                       HitmSecondaryMotionState* secondaryMotion = nullptr);

// Exposed separately for direct unit coverage of the clip-selection rule
// table described in this file's header comment.
core::Result<std::string> SelectClipName(HitmFighterState state);

}  // namespace dominus::character::hitm
