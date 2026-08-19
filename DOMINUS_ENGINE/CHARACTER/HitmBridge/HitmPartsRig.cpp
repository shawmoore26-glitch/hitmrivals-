// CHARACTER/HitmBridge/HitmPartsRig.cpp
#include "CHARACTER/HitmBridge/HitmPartsRig.h"

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

std::string ReqString(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsString()) throw std::runtime_error(std::string("field '") + key + "' is not a string");
    return v->AsString();
}

double ReqNumber(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsNumber()) throw std::runtime_error(std::string("field '") + key + "' is not a number");
    return v->AsNumber();
}

const Value& ReqArray(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsArray()) throw std::runtime_error(std::string("field '") + key + "' is not an array");
    return *v;
}

const Value& ReqObject(const Value& obj, const char* key) {
    const Value* v = obj.Get(key);
    if (!v) throw std::runtime_error(std::string("missing required field '") + key + "'");
    if (!v->IsObject()) throw std::runtime_error(std::string("field '") + key + "' is not an object");
    return *v;
}

// [x, y] pair of numbers, e.g. handPoint / a part's pivot.
std::pair<double, double> ReqNumberPair(const Value& obj, const char* key) {
    const Value& arr = ReqArray(obj, key);
    if (arr.AsArray().size() != 2) throw std::runtime_error(std::string("field '") + key + "' must have exactly 2 elements");
    for (const auto& e : arr.AsArray()) {
        if (!e.IsNumber()) throw std::runtime_error(std::string("field '") + key + "' contains a non-number element");
    }
    return {arr.AsArray()[0].AsNumber(), arr.AsArray()[1].AsNumber()};
}

std::vector<std::string> ReqStringArray(const Value& obj, const char* key) {
    const Value& arr = ReqArray(obj, key);
    std::vector<std::string> out;
    out.reserve(arr.AsArray().size());
    for (const auto& e : arr.AsArray()) {
        if (!e.IsString()) throw std::runtime_error(std::string("field '") + key + "' array contains a non-string element");
        out.push_back(e.AsString());
    }
    return out;
}

HitmBoneFollow ExtractFollow(const Value& f) {
    HitmBoneFollow follow;
    follow.stiffness = ReqNumber(f, "stiffness");
    follow.damping = ReqNumber(f, "damping");
    follow.lag_beats = ReqNumber(f, "lagBeats");
    follow.max_angle = ReqNumber(f, "maxAngle");
    follow.gravity = ReqNumber(f, "gravity");
    return follow;
}

HitmBoneEntry ExtractBone(const Value& b) {
    if (!b.IsObject()) throw std::runtime_error("'bones' array contains a non-object entry");
    HitmBoneEntry entry;
    entry.name = ReqString(b, "name");
    entry.why = ReqString(b, "_why");

    const Value* parent = b.Get("parent");
    if (!parent) {
        throw std::runtime_error("bone '" + entry.name + "' missing required field 'parent'");
    } else if (parent->IsNull()) {
        entry.parent = std::nullopt;  // root
    } else if (parent->IsString()) {
        entry.parent = parent->AsString();
    } else {
        throw std::runtime_error("bone '" + entry.name + "' field 'parent' is neither null nor a string");
    }

    const Value* len = b.Get("len");
    if (len) {
        if (!len->IsNumber()) throw std::runtime_error("bone '" + entry.name + "' field 'len' is not a number");
        entry.len = len->AsNumber();
    }

    const Value* follow = b.Get("follow");
    if (follow) {
        if (!follow->IsObject()) throw std::runtime_error("bone '" + entry.name + "' field 'follow' is not an object");
        entry.follow = ExtractFollow(*follow);
    }

    return entry;
}

HitmAtlasPart ExtractPart(const std::string& name, const Value& p) {
    HitmAtlasPart part;
    part.name = name;
    auto [px, py] = ReqNumberPair(p, "pivot");
    part.pivot_x = px;
    part.pivot_y = py;
    part.norm_w = ReqNumber(p, "normW");
    part.norm_h = ReqNumber(p, "normH");

    const Value& frame = ReqArray(p, "frame");
    if (frame.AsArray().size() != 4) throw std::runtime_error("part '" + name + "' field 'frame' must have exactly 4 elements");
    for (const auto& e : frame.AsArray()) {
        if (!e.IsNumber()) throw std::runtime_error("part '" + name + "' field 'frame' contains a non-number element");
    }
    part.frame_x = static_cast<int>(frame.AsArray()[0].AsNumber());
    part.frame_y = static_cast<int>(frame.AsArray()[1].AsNumber());
    part.frame_w = static_cast<int>(frame.AsArray()[2].AsNumber());
    part.frame_h = static_cast<int>(frame.AsArray()[3].AsNumber());
    return part;
}

}  // namespace

Result<HitmPartsRig> HitmPartsRig::Import(const std::filesystem::path& characterDir) {
    if (!std::filesystem::exists(characterDir) || !std::filesystem::is_directory(characterDir)) {
        return Result<HitmPartsRig>::Fail("Character directory does not exist: " + characterDir.string());
    }

    HitmPartsRig rig;
    rig.fighter_id_ = characterDir.filename().string();
    std::filesystem::path partsPath = characterDir / "parts.json";

    try {
        std::string text = ReadFile(partsPath);
        Value root;
        try {
            root = Value::Parse(text);
        } catch (const std::exception& e) {
            throw std::runtime_error(partsPath.string() + ": parse error: " + e.what());
        }
        if (!root.IsObject()) throw std::runtime_error(partsPath.string() + ": expected a JSON object at the top level");
        rig.raw_ = root;

        rig.atlas_ = ReqString(root, "atlas");

        const std::string expectedAtlas = rig.fighter_id_ + "_atlas";
        if (rig.atlas_ != expectedAtlas) {
            throw std::runtime_error("atlas '" + rig.atlas_ + "' does not match the expected '" + expectedAtlas +
                                      "' for directory '" + rig.fighter_id_ + "' (real convention, evidenced against " +
                                      "brooklyn/rocket/static -- a mismatch here means this parts.json belongs to a " +
                                      "different fighter than the directory claims)");
        }

        auto [sw, sh] = ReqNumberPair(root, "sourceSize");
        rig.source_width_ = static_cast<int32_t>(sw);
        rig.source_height_ = static_cast<int32_t>(sh);

        rig.hand_bone_ = ReqString(root, "handBone");
        auto [hx, hy] = ReqNumberPair(root, "handPoint");
        rig.hand_point_x_ = hx;
        rig.hand_point_y_ = hy;

        rig.draw_order_ = ReqStringArray(root, "drawOrder");

        const Value& bonesArr = ReqArray(root, "bones");
        rig.bones_.reserve(bonesArr.AsArray().size());
        for (const auto& b : bonesArr.AsArray()) {
            rig.bones_.push_back(ExtractBone(b));
        }

        // Every non-root bone's parent must name a bone that actually
        // exists in this same file -- a dangling parent reference means
        // the hierarchy cannot actually be walked, the same class of bug
        // ANIMATION::Skeleton::AddBone's own "parent-before-child"
        // contract exists to prevent on the DOMINUS side.
        {
            std::vector<std::string> boneNames;
            boneNames.reserve(rig.bones_.size());
            for (const auto& b : rig.bones_) boneNames.push_back(b.name);
            for (const auto& b : rig.bones_) {
                if (!b.parent.has_value()) continue;
                bool found = false;
                for (const auto& n : boneNames) {
                    if (n == *b.parent) { found = true; break; }
                }
                if (!found) throw std::runtime_error("bone '" + b.name + "' has parent '" + *b.parent + "' which does not name any bone");
            }
        }

        // handBone must resolve to a real bone in this same file -- a
        // dangling reference here is exactly the kind of "loaded but
        // pointing at nothing" bug this track refuses to pass through
        // silently.
        bool handBoneExists = false;
        for (const auto& b : rig.bones_) {
            if (b.name == rig.hand_bone_) { handBoneExists = true; break; }
        }
        if (!handBoneExists) {
            throw std::runtime_error("handBone '" + rig.hand_bone_ + "' does not name any bone in 'bones'");
        }

        const Value& partsObj = ReqObject(root, "parts");
        rig.parts_.reserve(partsObj.AsObject().size());
        for (const auto& [name, val] : partsObj.AsObject()) {
            rig.parts_.push_back(ExtractPart(name, val));
        }

        // drawOrder must name exactly the parts this file actually has --
        // a part with no draw order, or a draw-order entry naming a part
        // that doesn't exist, both mean the file cannot actually be drawn
        // as it claims.
        for (const auto& partName : rig.draw_order_) {
            bool found = false;
            for (const auto& p : rig.parts_) {
                if (p.name == partName) { found = true; break; }
            }
            if (!found) throw std::runtime_error("drawOrder names part '" + partName + "' which is not in 'parts'");
        }
        if (rig.draw_order_.size() != rig.parts_.size()) {
            throw std::runtime_error("drawOrder has " + std::to_string(rig.draw_order_.size()) + " entries but 'parts' has " +
                                      std::to_string(rig.parts_.size()));
        }
    } catch (const std::exception& e) {
        return Result<HitmPartsRig>::Fail("HitmPartsRig::Import(" + characterDir.string() + "): " + e.what());
    }

    return Result<HitmPartsRig>::Ok(std::move(rig));
}

}  // namespace dominus::character::hitm
