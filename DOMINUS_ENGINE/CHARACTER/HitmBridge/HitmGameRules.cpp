// CHARACTER/HitmBridge/HitmGameRules.cpp
#include "CHARACTER/HitmBridge/HitmGameRules.h"

#include <fstream>
#include <sstream>

namespace dominus::character::hitm {

using core::Result;
using core::json::Value;

namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

double ReqNumber(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsNumber()) throw std::runtime_error(std::string("field '") + key + "' is not a number");
    return v->AsNumber();
}

const Value& ReqObject(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsObject()) throw std::runtime_error(std::string("field '") + key + "' is not an object");
    return *v;
}

std::vector<std::string> ReqStringArray(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsArray()) throw std::runtime_error(std::string("field '") + key + "' is not an array");
    std::vector<std::string> out;
    out.reserve(v->AsArray().size());
    for (const auto& e : v->AsArray()) {
        if (!e.IsString()) throw std::runtime_error(std::string("field '") + key + "' array contains a non-string element");
        out.push_back(e.AsString());
    }
    return out;
}

HitmViewConfig ExtractView(const Value& v) {
    HitmViewConfig cfg;
    cfg.w = ReqNumber(v, "w");
    cfg.h = ReqNumber(v, "h");
    return cfg;
}

HitmPhysicsRules ExtractPhysics(const Value& v) {
    HitmPhysicsRules p;
    p.ground = ReqNumber(v, "ground");
    p.gravity = ReqNumber(v, "gravity");
    p.wall_l = ReqNumber(v, "wallL");
    p.wall_r = ReqNumber(v, "wallR");
    p.walk_speed = ReqNumber(v, "walkSpeed");
    p.jump_vel = ReqNumber(v, "jumpVel");
    p.dash_speed = ReqNumber(v, "dashSpeed");
    p.dash_frames = ReqNumber(v, "dashFrames");
    return p;
}

HitmMeterRules ExtractMeter(const Value& v) {
    HitmMeterRules m;
    m.max = ReqNumber(v, "max");
    m.on_hit_give = ReqNumber(v, "onHitGive");
    m.on_hit_take = ReqNumber(v, "onHitTake");
    m.on_block_give = ReqNumber(v, "onBlockGive");
    m.on_block_take = ReqNumber(v, "onBlockTake");
    m.break_cost = ReqNumber(v, "breakCost");
    m.burst_cost = ReqNumber(v, "burstCost");
    m.bb_cost = ReqNumber(v, "bbCost");
    m.perfect_guard_gain = ReqNumber(v, "perfectGuardGain");
    return m;
}

HitmCombatRules ExtractCombat(const Value& v) {
    HitmCombatRules c;
    c.scale_min = ReqNumber(v, "scaleMin");
    c.scale_step = ReqNumber(v, "scaleStep");
    c.chip_mult = ReqNumber(v, "chipMult");
    c.counter_dmg_mult = ReqNumber(v, "counterDmgMult");
    c.counter_stun_bonus = ReqNumber(v, "counterStunBonus");
    c.perfect_guard_window = ReqNumber(v, "perfectGuardWindow");
    c.otg_dmg_mult = ReqNumber(v, "otgDmgMult");
    c.wall_bounce_min_vx = ReqNumber(v, "wallBounceMinVX");
    c.down_frames = ReqNumber(v, "downFrames");
    c.wakeup_invuln = ReqNumber(v, "wakeupInvuln");
    c.burst_invuln = ReqNumber(v, "burstInvuln");
    c.input_buffer_frames = ReqNumber(v, "inputBufferFrames");
    c.hitstop_light = ReqNumber(v, "hitstopLight");
    c.hitstop_heavy = ReqNumber(v, "hitstopHeavy");
    c.hitstop_counter = ReqNumber(v, "hitstopCounter");
    return c;
}

HitmRoundRules ExtractRounds(const Value& v) {
    HitmRoundRules r;
    r.to_win = ReqNumber(v, "toWin");
    r.timer_seconds = ReqNumber(v, "timerSeconds");
    return r;
}

HitmSpriteRules ExtractSprite(const Value& v) {
    HitmSpriteRules s;
    s.display_height = ReqNumber(v, "displayHeight");
    return s;
}

}  // namespace

Result<HitmGameRules> HitmGameRules::Import(const std::filesystem::path& gameJsonPath) {
    HitmGameRules rules;

    try {
        std::string text = ReadFile(gameJsonPath);
        Value root;
        try {
            root = Value::Parse(text);
        } catch (const std::exception& e) {
            throw std::runtime_error(gameJsonPath.string() + ": parse error: " + e.what());
        }
        if (!root.IsObject()) throw std::runtime_error(gameJsonPath.string() + ": expected a JSON object at the top level");
        rules.raw_ = root;

        rules.roster_ = ReqStringArray(root, "roster");
        if (rules.roster_.empty()) throw std::runtime_error("'roster' is present but empty -- a game with no fighters is not a real roster");

        rules.view_ = ExtractView(ReqObject(root, "view"));
        rules.physics_ = ExtractPhysics(ReqObject(root, "physics"));
        rules.meter_ = ExtractMeter(ReqObject(root, "meter"));
        rules.combat_ = ExtractCombat(ReqObject(root, "combat"));
        rules.rounds_ = ExtractRounds(ReqObject(root, "rounds"));
        rules.sprite_ = ExtractSprite(ReqObject(root, "sprite"));
    } catch (const std::exception& e) {
        return Result<HitmGameRules>::Fail("HitmGameRules::Import(" + gameJsonPath.string() + "): " + e.what());
    }

    return Result<HitmGameRules>::Ok(std::move(rules));
}

}  // namespace dominus::character::hitm
