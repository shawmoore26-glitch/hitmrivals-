// CHARACTER/HitmBridge/HitmPartsRig.h
// ROADMAP.md Track H Module 3 -- 2D sprite-cutout rig representation.
//
// Gives HITM Rivals' real, generated `parts.json` a home in DOMINUS.
// `ANIMATION/SkeletonSystem/Skeleton.h` has no concept of a sprite part, a
// pivot, an atlas pixel frame, or a draw layer at all (confirmed by grep
// in HITM_INTEGRATION_AUDIT.md section 3) -- this class is that concept,
// built from the real file, not invented.
//
// IMPORTANT PROVENANCE NOTE, read before treating this as authored data:
// `parts.json` lives under hitm-engine's `data/characters/<fighter>/` --
// per that project's own README, this is GENERATED OUTPUT ("never touch
// this"), compiled by `tools/slice_rig.py` from the real sprite sheet plus
// the AUTHORED `design.json` (already imported losslessly by Module 1,
// see HitmIdentityRecord::design). This class reads generated output --
// legitimately, HITM's own rule is "never hand-edit it," not "never read
// it" -- but it is not itself the source of truth for a part's *intended*
// placement; `design.json`'s `core_parts` is. A future module that needs
// to regenerate this data would derive it from `design.json` again, not
// from editing what this class reads.
//
// Same architecture as Module 2 (HitmCombatGenome): `raw_` holds the
// exact parsed JSON tree, `ToJson()` returns it verbatim, every typed
// accessor is a read-only extractive view over `raw_`, never the reverse.
//
// A REAL, NON-OBVIOUS FINDING FROM THE ACTUAL DATA (not asserted --
// verified against all three real fighters): `parts.json`'s `bones` array
// contains DUPLICATE names. `handFar`/`handNear` each appear twice --
// once as the primary rigid kinetic-chain bone (no `follow`), and again
// later as a secondary-motion glove-bounce spring attached to the same
// joint name (`follow` present). This is HITM's real design, not a data
// bug: `Bones()` below is therefore an ORDER-PRESERVING SEQUENCE, not a
// name-keyed map -- collapsing it into a map (the way
// `ANIMATION::Skeleton::AddBone` indexes by name) would silently discard
// one of the two real, distinct entries. This class deliberately does
// NOT build a `dominus::animation::Skeleton` from this data: `len` alone
// is not a full bind-pose `Transform2D` (no position/rotation), and
// synthesizing one would be exactly the kind of invented value this
// track refuses. That binding is real, separately-gated future work.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "CORE/Serialization/MiniJson.h"

namespace dominus::character::hitm {

// Secondary-motion spring parameters -- render-layer only, per the
// Constitution's "secondary motion never affects gameplay" law. Present
// only on bones that are genuinely springs (dreads, coat, hat, jaw, tie,
// chain, and the glove-bounce hand overlays), evidenced identical 5-key
// shape across all three real fighters.
struct HitmBoneFollow {
    double stiffness = 0;
    double damping = 0;
    double lag_beats = 0;
    double max_angle = 0;
    double gravity = 0;
};

// One entry in the real `bones` array, in source order. `name` is
// deliberately NOT unique across the sequence -- see this file's top
// comment.
struct HitmBoneEntry {
    std::string name;
    std::optional<std::string> parent;  // absent/null == root
    std::string why;                    // "_why" -- HITM's own documentation for the bone's purpose
    std::optional<double> len;
    std::optional<HitmBoneFollow> follow;
};

// One entry in the real `parts` map -- the atlas-space placement HITM's
// real `parts.json` schema calls out (pivot + normalized size + pixel
// frame rect within the named atlas texture).
struct HitmAtlasPart {
    std::string name;  // the JSON key this came from, carried along since Parts() below is a vector, not a map (see class comment)
    double pivot_x = 0;
    double pivot_y = 0;
    double norm_w = 0;
    double norm_h = 0;
    int frame_x = 0;
    int frame_y = 0;
    int frame_w = 0;
    int frame_h = 0;
};

class HitmPartsRig {
public:
    // Reads <characterDir>/parts.json -- the real hitm-engine
    // data/characters/<fighter>/ directory shape. Fails (Result::Fail) on
    // a missing directory/file, malformed JSON, a missing/wrong-typed
    // required field, or an atlas name that does not match
    // "<directory_name>_atlas" -- the real, evidenced (3/3 fighters)
    // naming convention; a mismatch here is the same class of real bug
    // Module 1's identity/directory cross-check catches, applied to this
    // file instead.
    static core::Result<HitmPartsRig> Import(const std::filesystem::path& characterDir);

    const std::string& FighterId() const { return fighter_id_; }
    const std::string& Atlas() const { return atlas_; }
    int32_t SourceWidth() const { return source_width_; }
    int32_t SourceHeight() const { return source_height_; }
    const std::string& HandBone() const { return hand_bone_; }
    double HandPointX() const { return hand_point_x_; }
    double HandPointY() const { return hand_point_y_; }

    // Draw order, back-to-front, by part name -- real z-order data with
    // no equivalent anywhere in DOMINUS today.
    const std::vector<std::string>& DrawOrder() const { return draw_order_; }

    // Source order preserved, duplicates included -- see this file's top
    // comment for why a map would be lossy here.
    const std::vector<HitmBoneEntry>& Bones() const { return bones_; }

    // Atlas-space part placements. Real fighters' part names are unique
    // (evidenced 3/3) so this could be a map without loss, but it is kept
    // as an order-preserving vector anyway for the same reason as Bones()
    // -- a map is only safe to use in place of a sequence when uniqueness
    // is a guaranteed invariant of the format, not just true of today's
    // three fighters; nothing in the real schema promises that for a
    // fighter this class hasn't seen yet.
    const std::vector<HitmAtlasPart>& Parts() const { return parts_; }

    // The exact source tree -- see this file's top comment for why this,
    // not the typed fields above, is the actual losslessness guarantee.
    const core::json::Value& ToJson() const { return raw_; }

private:
    std::string fighter_id_;
    core::json::Value raw_;
    std::string atlas_;
    int32_t source_width_ = 0;
    int32_t source_height_ = 0;
    std::string hand_bone_;
    double hand_point_x_ = 0;
    double hand_point_y_ = 0;
    std::vector<std::string> draw_order_;
    std::vector<HitmBoneEntry> bones_;
    std::vector<HitmAtlasPart> parts_;
};

}  // namespace dominus::character::hitm
