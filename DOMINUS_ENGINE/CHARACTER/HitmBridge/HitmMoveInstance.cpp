// CHARACTER/HitmBridge/HitmMoveInstance.cpp
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"

namespace dominus::character::hitm {

using core::Result;
using core::json::Value;

namespace {

std::string ReqString(const Value& obj, const char* key, const std::string& context) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(context + ": missing required field '" + key + "'");
    if (!v->IsString()) throw std::runtime_error(context + ": field '" + key + "' is not a string");
    return v->AsString();
}

double ReqNumber(const Value& obj, const char* key, const std::string& context) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(context + ": missing required field '" + key + "'");
    if (!v->IsNumber()) throw std::runtime_error(context + ": field '" + key + "' is not a number");
    return v->AsNumber();
}

HitmHitstopCategory ParseHitstopCategory(const std::string& s, const std::string& context) {
    if (s == "light") return HitmHitstopCategory::kLight;
    if (s == "heavy") return HitmHitstopCategory::kHeavy;
    if (s == "counter") return HitmHitstopCategory::kCounter;
    throw std::runtime_error(context + ": unrecognized hitstop category '" + s +
                              "' -- real values observed are 'light'/'heavy'/'counter' only, refusing to guess");
}

}  // namespace

Result<HitmMoveInstance> HitmMoveInstance::Extract(const HitmIdentityRecord& record, const std::string& moveKey) {
    HitmMoveInstance instance;
    instance.move_key = moveKey;
    std::string context = "HitmMoveInstance::Extract(" + record.fighter_id + ", " + moveKey + ")";

    try {
        const Value* movesObj = record.signature.Get("moves");
        if (!movesObj || !movesObj->IsObject()) {
            throw std::runtime_error(context + ": signature.json has no 'moves' object");
        }
        const Value* moveVal = movesObj->Get(moveKey);
        if (!moveVal || !moveVal->IsObject()) {
            throw std::runtime_error(context + ": no move named '" + moveKey + "' in signature.json's 'moves'");
        }
        const Value& move = *moveVal;

        instance.move_def.name = ReqString(move, "name", context);
        instance.input_token = ReqString(move, "input", context);
        instance.move_def.frames.startup = static_cast<int>(ReqNumber(move, "startup", context));
        instance.move_def.frames.active = static_cast<int>(ReqNumber(move, "active", context));
        instance.move_def.frames.recovery = static_cast<int>(ReqNumber(move, "recovery", context));
        instance.move_def.power = static_cast<float>(ReqNumber(move, "damage", context));

        instance.hitstun_frames = static_cast<int>(ReqNumber(move, "hitstun", context));

        // A real `rush` sub-object (Rocket's real "Ghost Dash") switches
        // the required-field set -- see this file's own top comment,
        // "A THIRD REAL FINDING". Every field check below runs in
        // EXACTLY the same relative order as before this branch existed,
        // for every field both schemas share -- a melee-type move's
        // real error messages/behavior are byte-for-byte unchanged.
        const Value* rushVal = move.Get("rush");
        bool isRush = rushVal != nullptr && rushVal->IsObject();

        if (!isRush) {
            instance.blockstun_frames = static_cast<int>(ReqNumber(move, "blockstun", context));
        }
        instance.meter_gain = static_cast<int>(ReqNumber(move, "meterGain", context));
        instance.hitstop_category = ParseHitstopCategory(ReqString(move, "hitstop", context), context);
        if (!isRush) {
            instance.range = ReqNumber(move, "range", context);
            instance.height = ReqNumber(move, "height", context);
        }
        instance.cooldown_frames = static_cast<int>(ReqNumber(move, "cooldown", context));

        if (isRush) {
            std::string rushContext = context + ".rush";
            HitmRushData rush;
            rush.velocity_x = ReqNumber(*rushVal, "velocityX", rushContext);
            rush.friction = ReqNumber(*rushVal, "friction", rushContext);
            rush.hit_range_x = ReqNumber(*rushVal, "hitRangeX", rushContext);
            rush.hit_range_y = ReqNumber(*rushVal, "hitRangeY", rushContext);
            instance.rush = rush;
        }
    } catch (const std::exception& e) {
        return Result<HitmMoveInstance>::Fail(e.what());
    }

    return Result<HitmMoveInstance>::Ok(std::move(instance));
}

}  // namespace dominus::character::hitm
