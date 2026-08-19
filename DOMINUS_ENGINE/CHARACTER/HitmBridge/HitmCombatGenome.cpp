// CHARACTER/HitmBridge/HitmCombatGenome.cpp
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"

namespace dominus::character::hitm {

using core::Result;
using core::json::Value;

namespace {

// --- strict, type-checked extraction helpers -------------------------
// Every helper here either (a) returns the real value if present with
// the right JSON type, (b) returns an empty optional/vector if the key is
// genuinely absent, or (c) throws if the key IS present but the wrong
// type -- there is no fourth case that silently substitutes a default.

std::optional<std::string> OptString(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) return std::nullopt;
    if (!v->IsString()) throw std::runtime_error(std::string("field '") + key + "' present but not a string");
    return v->AsString();
}

std::optional<double> OptNumber(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) return std::nullopt;
    if (!v->IsNumber()) throw std::runtime_error(std::string("field '") + key + "' present but not a number");
    return v->AsNumber();
}

std::optional<std::vector<std::string>> OptStringArray(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) return std::nullopt;
    if (!v->IsArray()) throw std::runtime_error(std::string("field '") + key + "' present but not an array");
    std::vector<std::string> out;
    out.reserve(v->AsArray().size());
    for (const auto& elem : v->AsArray()) {
        if (!elem.IsString()) throw std::runtime_error(std::string("field '") + key + "' array contains a non-string element");
        out.push_back(elem.AsString());
    }
    return out;
}

std::string ReqString(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsString()) throw std::runtime_error(std::string("field '") + key + "' is not a string");
    return v->AsString();
}

std::vector<std::string> ReqStringArray(const Value& obj, const char* key) {
    auto opt = OptStringArray(obj, key);
    if (!opt) throw std::runtime_error(std::string("missing required field '") + key + "'");
    return *opt;
}

const Value& ReqObject(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsObject()) throw std::runtime_error(std::string("field '") + key + "' is not an object");
    return *v;
}

RhythmProfile ExtractRhythm(const Value& p) {
    return RhythmProfile{OptString(p, "categorical"), OptString(p, "beat"), OptString(p, "tempo"), OptString(p, "signature")};
}

WeightProfile ExtractWeight(const Value& p) {
    return WeightProfile{OptString(p, "categorical"), OptNumber(p, "inertia"), OptString(p, "settle")};
}

RiskProfile ExtractRisk(const Value& p) {
    return RiskProfile{OptString(p, "commitment"), OptString(p, "reward"), OptString(p, "recovery_exposure")};
}

DefenseProfile ExtractDefense(const Value& p) {
    DefenseProfile d;
    d.style = OptString(p, "style");
    d.block_preference = OptNumber(p, "blockPreference");
    d.escape = OptString(p, "escape");
    d.law = OptString(p, "_law");
    d.from = OptStringArray(p, "from");
    d.not_allowed = OptStringArray(p, "not");
    return d;
}

PressureProfile ExtractPressure(const Value& p) {
    return PressureProfile{OptString(p, "style"), OptString(p, "turnRetention"), OptString(p, "cornerCarry")};
}

RangeProfile ExtractRange(const Value& p) {
    return RangeProfile{OptString(p, "band"), OptNumber(p, "dominant"), OptString(p, "deadzone")};
}

RecoveryProfile ExtractRecovery(const Value& p) {
    return RecoveryProfile{OptString(p, "categorical"), OptNumber(p, "settleBeats")};
}

ImpactProfile ExtractImpact(const Value& p) {
    return ImpactProfile{OptString(p, "hitstop"), OptString(p, "cameraShake"), OptString(p, "soundIntent")};
}

std::optional<ReadEngine> ExtractReadEngine(const Value& root) {
    const Value* re = root.Get("read_engine");
    if (!re) return std::nullopt;  // genuinely absent for Rocket/Static -- not an error
    if (!re->IsObject()) throw std::runtime_error("'read_engine' present but not an object");

    ReadEngine engine;
    const Value* maxReads = re->Get("max_reads");
    if (!maxReads || !maxReads->IsNumber()) throw std::runtime_error("read_engine.max_reads missing or not a number");
    engine.max_reads = maxReads->AsNumber();

    engine.gain_on = ReqStringArray(*re, "gain_on");
    engine.lose_on = ReqStringArray(*re, "lose_on");
    engine.lose_law = OptString(*re, "_lose_law");

    const Value* tiers = re->Get("tiers");
    if (!tiers || !tiers->IsArray()) throw std::runtime_error("read_engine.tiers missing or not an array");
    for (const auto& tierVal : tiers->AsArray()) {
        if (!tierVal.IsObject()) throw std::runtime_error("read_engine.tiers contains a non-object entry");
        const Value* reads = tierVal.Get("reads");
        const Value* name = tierVal.Get("name");
        const Value* mult = tierVal.Get("damage_mult");
        if (!reads || !reads->IsNumber()) throw std::runtime_error("read_engine tier missing numeric 'reads'");
        if (!name || !name->IsString()) throw std::runtime_error("read_engine tier missing string 'name'");
        if (!mult || !mult->IsNumber()) throw std::runtime_error("read_engine tier missing numeric 'damage_mult'");
        ReadEngineTier tier;
        tier.reads = reads->AsNumber();
        tier.name = name->AsString();
        tier.damage_mult = mult->AsNumber();
        tier.note = OptString(tierVal, "note");
        engine.tiers.push_back(std::move(tier));
    }

    const Value& decayObj = ReqObject(*re, "decay");
    const Value* frames = decayObj.Get("frames");
    const Value* amount = decayObj.Get("amount");
    if (!frames || !frames->IsNumber()) throw std::runtime_error("read_engine.decay.frames missing or not a number");
    if (!amount || !amount->IsNumber()) throw std::runtime_error("read_engine.decay.amount missing or not a number");
    engine.decay.frames = frames->AsNumber();
    engine.decay.amount = amount->AsNumber();
    engine.decay.note = OptString(decayObj, "_note");

    return engine;
}

}  // namespace

Result<HitmCombatGenome> HitmCombatGenome::FromRecord(const HitmIdentityRecord& record) {
    HitmCombatGenome g;
    g.fighter_id_ = record.fighter_id;
    g.raw_ = record.combat_genome;  // the untouched source of truth -- see header comment

    try {
        const Value& root = record.combat_genome;
        if (!root.IsObject()) {
            throw std::runtime_error("combat_genome is not a JSON object");
        }

        // These 4 keys are already guaranteed present by Module 1's
        // HitmIdentityImporter (see its RequiredKeys() for combat_genome.json);
        // this call still validates their TYPE, which Module 1 does not.
        g.archetype_ = ReqString(root, "archetype");
        g.philosophy_ = ReqString(root, "philosophy");
        g.frame_dna_ = ReqString(root, "frame_dna");
        g.ai_intent_ = ReqStringArray(root, "ai_intent");
        g.combat_verbs_ = ReqStringArray(root, "combat_verbs");

        g.rhythm_ = ExtractRhythm(ReqObject(root, "rhythm_profile"));
        g.weight_ = ExtractWeight(ReqObject(root, "weight_profile"));
        g.risk_ = ExtractRisk(ReqObject(root, "risk_profile"));
        g.defense_ = ExtractDefense(ReqObject(root, "defense_profile"));
        g.pressure_ = ExtractPressure(ReqObject(root, "pressure_profile"));
        g.range_ = ExtractRange(ReqObject(root, "range_profile"));
        g.recovery_ = ExtractRecovery(ReqObject(root, "recovery_profile"));
        g.impact_ = ExtractImpact(ReqObject(root, "impact_profile"));

        g.read_engine_ = ExtractReadEngine(root);
    } catch (const std::exception& e) {
        return Result<HitmCombatGenome>::Fail(
            "HitmCombatGenome::FromRecord(" + record.fighter_id + "): " + e.what());
    }

    return Result<HitmCombatGenome>::Ok(std::move(g));
}

}  // namespace dominus::character::hitm
