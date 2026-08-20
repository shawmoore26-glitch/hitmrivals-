// CHARACTER/HitmBridge/HitmAnimationSet.h
// ROADMAP.md Track H Module 5B -- real HITM sprite/texture integration,
// Phase 1 (CPU-observable). Gives HITM Rivals' real, generated `anim.json`
// a home in DOMINUS.
//
// PROVENANCE: `anim.json` lives under hitm-engine's
// `data/characters/<fighter>/`, sibling to Module 3's `parts.json` --
// same directory, same "generated output, legitimate to read, never to
// hand-edit" status (see HitmPartsRig.h's top comment; this file inherits
// that reasoning verbatim, it is not re-argued here).
//
// WHAT THIS ACTUALLY IS, evidenced against the real file (not guessed):
// a per-clip, per-BONE keyframe track set -- `anim.json`'s top level maps
// a clip name ("idle", "walk", "special", ...) to `{loop, len, tracks}`,
// where `tracks` maps a bone/part name to an array of
// `[frame, rotationDegrees, offsetX, offsetY]` keyframes, `offsetX`/
// `offsetY` in the same normalized-to-display-height units as
// `HitmPartsRig`'s `normW`/`normH`. This is a genuine skeletal-animation
// format, not a sprite flipbook -- consistent with Module 3's own finding
// that HITM's rig is cutout/skeletal, not per-frame raster art.
//
// THE SAMPLING ALGORITHM BELOW (`Sample()`) IS A DIRECT, VERIFIED PORT of
// hitm-engine's own real, working `SkeletonSystem.js`'s `_sample()`
// function (clamp below the first key, clamp above the last key,
// otherwise linear-search the bracketing pair and interpolate with a
// smoothstep ease, `t*t*(3-2*t)`) -- not an independently invented
// interpolation scheme. Fidelity to the real algorithm is what makes
// this module's numeric output meaningfully comparable to what the real
// engine would actually show, not just "some plausible curve."
//
// WHAT THIS MODULE DELIBERATELY DOES NOT ATTEMPT: hitm-engine's real
// `SkeletonSystem.build()` (full bone-hierarchy forward-kinematics,
// walking parent transforms to resolve every bone's *world* position) has
// a real, evidenced data gap in this repository -- `parts.json`'s real
// `bones[]` entries carry no bind-pose anchor (`.at`) and no declared
// part ownership (`.part`) field, so `build()`'s own parent-offset math
// and `boneName = order.find(n => bones[n].part === pName)` lookup cannot
// actually run against the real, checked-in data (verified: neither
// field appears anywhere in any of the three real fighters' `parts.json`
// files). This is not a DOMINUS omission; it is a property of the real
// source data as it exists in this repository today. See
// `HitmSpriteDrawData.h`'s top comment for the smallest correct
// extension this module identifies instead of inventing bind-pose data
// to paper over the gap.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "CORE/Serialization/MiniJson.h"

namespace dominus::character::hitm {

// One real `[frame, rotationDegrees, offsetX, offsetY]` entry.
struct HitmKeyframe {
    int frame = 0;
    double rotation_deg = 0.0;
    double offset_x = 0.0;
    double offset_y = 0.0;
};

// The local pose a bone/part carries at a given sampled point in a clip --
// `Sample()`'s return value.
struct HitmLocalPose {
    double rotation_deg = 0.0;
    double offset_x = 0.0;
    double offset_y = 0.0;
};

struct HitmAnimationClip {
    std::string name;
    bool loop = false;
    int len = 0;  // real authored clip length, in frames
    // Keyed by bone/part name (real data: the two namespaces coincide by
    // exact string match for every bone that carries a visual part --
    // see HitmSpriteDrawData.h). Not every drawn part has a track in
    // every clip (real data: e.g. Brooklyn's real "block" clip has 11
    // tracks against 22 real parts) -- `Sample()` on a missing track
    // returns the zero pose, exactly matching hitm-engine's own
    // `_sample(null, f)` fallback, not a DOMINUS-invented default.
    std::map<std::string, std::vector<HitmKeyframe>> tracks;

    // Direct port of hitm-engine's `SkeletonSystem._sample()`. `frame`
    // may be fractional (real engine supports blend/crossfade sampling);
    // Phase 1 only ever calls this with integer frames, but the function
    // itself makes no such assumption, matching the real one.
    HitmLocalPose Sample(const std::string& trackName, double frame) const;
};

class HitmAnimationSet {
public:
    // Reads <characterDir>/anim.json -- sibling to HitmPartsRig::Import's
    // parts.json, same real directory. Fails (Result::Fail) on a missing
    // file, malformed JSON, a clip missing `loop`/`len`, a non-positive
    // `len`, an empty keyframe track, or a keyframe array entry that is
    // not exactly the real 4-element `[frame, rot, dx, dy]` shape.
    //
    // DELIBERATELY NOT VALIDATED: keyframe frame-number ordering. A first
    // pass here required strictly-increasing frames per track and
    // immediately failed to import Brooklyn's own real data --
    // `Sample()`'s bracket search does not actually require monotonicity
    // (see its own comment), and the real, generated `anim.json` files
    // genuinely are not monotonic: real "anticipation snap" authoring
    // repeats a frame number with a different value (e.g. Brooklyn's real
    // `heavy`/`footFar` track has two keyframes at frame 0, `[0,0,0,0]`
    // then `[0,9,0,0]` a few entries later) and, in a few tracks, even
    // places an earlier frame number after a later one (Brooklyn's real
    // `light1`/`legFarU`: `[[0,0,0,0],[1,12,0,0],[0,-18,0,0],[11,0,0,0]]`
    // -- the middle entry is frame 0 again, arriving textually after
    // frame 1). `Sample()`'s pair-scan (a direct, faithful port of
    // hitm-engine's own `_sample()`) simply never satisfies its bracket
    // condition for an out-of-order pair, so such entries are inert --
    // real, present, harmless. Requiring monotonicity would have been a
    // DOMINUS-invented rule the real data does not actually follow;
    // dropped rather than kept as false rigor.
    static core::Result<HitmAnimationSet> Import(const std::filesystem::path& characterDir);

    const std::string& FighterId() const { return fighter_id_; }
    bool HasClip(const std::string& name) const { return clips_.count(name) > 0; }
    const HitmAnimationClip* Clip(const std::string& name) const;

    // Every real clip name this fighter's anim.json actually defines, in
    // source (map) order -- deterministic, not insertion order of some
    // other structure.
    std::vector<std::string> ClipNames() const;

private:
    std::string fighter_id_;
    std::map<std::string, HitmAnimationClip> clips_;
};

}  // namespace dominus::character::hitm
