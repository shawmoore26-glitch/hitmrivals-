// CHARACTER/HitmBridge/HitmGameRules.h
// ROADMAP.md Track H Module 4 -- global game-rules table.
//
// Gives HITM Rivals' real, authored `data/system/game.json` a typed home.
// Unlike Modules 1-3 (per-fighter data), this is a single, global,
// canonical file: gravity, walk/dash speed, meter economy, combat scaling
// and hitstop, round rules, and the authoritative fighter roster.
// HITM_INTEGRATION_AUDIT.md section 3 named this exact gap: no DOMINUS
// equivalent existed anywhere -- `GameDesignGenome` is a meta-design
// descriptor (arcade vs. soulslike), not a physics/meter constants table,
// and `PHYSICS/PhysicsSystem` is generic rigid-body, untuned to fighting-
// game feel.
//
// `data/system/cameras.json` and `data/system/vfx.json` (siblings of
// game.json in the real hitm-engine tree) are deliberately OUT OF SCOPE --
// the audit and roadmap named `game.json` specifically for this module;
// camera and VFX data are real, separate gaps for later, separately-gated
// work, not silently folded in here.
//
// Same architecture as Modules 1-3: `raw_` holds the exact parsed JSON
// tree, `ToJson()` returns it verbatim, every typed accessor below is a
// read-only extractive view over it. Unlike the per-fighter genome data
// (Module 2), every field modeled here is REQUIRED, not optional -- this
// is one canonical file with a fixed, fully-specified real schema, not
// data that legitimately varies field-by-field per fighter. A field this
// class does not model explicitly (should the real file ever gain one)
// still survives in `raw_`/`ToJson()`, same guarantee as every prior
// module.
//
// All numeric fields are stored as `double`, matching how MiniJson itself
// stores every JSON number -- the real source data does not distinguish
// integer frame counts from fractional speeds/multipliers at the JSON
// level, and inventing that distinction here would not be extracting
// something the source has, it would be adding something it doesn't.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "CORE/Serialization/MiniJson.h"

namespace dominus::character::hitm {

struct HitmViewConfig {
    double w = 0;
    double h = 0;
};

struct HitmPhysicsRules {
    double ground = 0;
    double gravity = 0;
    double wall_l = 0;
    double wall_r = 0;
    double walk_speed = 0;
    double jump_vel = 0;
    double dash_speed = 0;
    double dash_frames = 0;
};

struct HitmMeterRules {
    double max = 0;
    double on_hit_give = 0;
    double on_hit_take = 0;
    double on_block_give = 0;
    double on_block_take = 0;
    double break_cost = 0;
    double burst_cost = 0;
    double bb_cost = 0;
    double perfect_guard_gain = 0;
};

struct HitmCombatRules {
    double scale_min = 0;
    double scale_step = 0;
    double chip_mult = 0;
    double counter_dmg_mult = 0;
    double counter_stun_bonus = 0;
    double perfect_guard_window = 0;
    double otg_dmg_mult = 0;
    double wall_bounce_min_vx = 0;
    double down_frames = 0;
    double wakeup_invuln = 0;
    double burst_invuln = 0;
    double input_buffer_frames = 0;
    double hitstop_light = 0;
    double hitstop_heavy = 0;
    double hitstop_counter = 0;
};

struct HitmRoundRules {
    double to_win = 0;
    double timer_seconds = 0;
};

struct HitmSpriteRules {
    double display_height = 0;
};

class HitmGameRules {
public:
    // Reads a real hitm-engine data/system/game.json file. Fails
    // (Result::Fail) on a missing file, malformed JSON, a missing
    // required top-level or nested field, or a field present with the
    // wrong JSON type -- never a silent default fill.
    static core::Result<HitmGameRules> Import(const std::filesystem::path& gameJsonPath);

    // The authoritative fighter roster, in the real file's order.
    const std::vector<std::string>& Roster() const { return roster_; }

    const HitmViewConfig& View() const { return view_; }
    const HitmPhysicsRules& Physics() const { return physics_; }
    const HitmMeterRules& Meter() const { return meter_; }
    const HitmCombatRules& Combat() const { return combat_; }
    const HitmRoundRules& Rounds() const { return rounds_; }
    const HitmSpriteRules& Sprite() const { return sprite_; }

    // The exact source tree -- see this file's top comment for why this,
    // not the typed fields above, is the actual losslessness guarantee.
    const core::json::Value& ToJson() const { return raw_; }

private:
    core::json::Value raw_;
    std::vector<std::string> roster_;
    HitmViewConfig view_;
    HitmPhysicsRules physics_;
    HitmMeterRules meter_;
    HitmCombatRules combat_;
    HitmRoundRules rounds_;
    HitmSpriteRules sprite_;
};

}  // namespace dominus::character::hitm
