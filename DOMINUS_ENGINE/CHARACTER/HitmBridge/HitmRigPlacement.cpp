// CHARACTER/HitmBridge/HitmRigPlacement.cpp
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"

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

HitmPartPlacement ExtractPlacement(const std::string& partName, const Value& p) {
    std::string ctx = "part '" + partName + "'";
    const Value* rectVal = p.Get("rect");
    if (!rectVal || !rectVal->IsArray() || rectVal->AsArray().size() != 4) {
        throw std::runtime_error(ctx + ": field 'rect' must be a 4-element array");
    }
    const auto& rect = rectVal->AsArray();
    for (const auto& e : rect) {
        if (!e.IsNumber()) throw std::runtime_error(ctx + ": field 'rect' contains a non-number element");
    }
    const Value* pivotVal = p.Get("pivot");
    if (!pivotVal || !pivotVal->IsArray() || pivotVal->AsArray().size() != 2) {
        throw std::runtime_error(ctx + ": field 'pivot' must be a 2-element array");
    }
    const auto& pivot = pivotVal->AsArray();
    for (const auto& e : pivot) {
        if (!e.IsNumber()) throw std::runtime_error(ctx + ": field 'pivot' contains a non-number element");
    }

    HitmPartPlacement placement;
    placement.rect_x0 = rect[0].AsNumber();
    placement.rect_y0 = rect[1].AsNumber();
    placement.rect_x1 = rect[2].AsNumber();
    placement.rect_y1 = rect[3].AsNumber();
    placement.pivot_x = pivot[0].AsNumber();
    placement.pivot_y = pivot[1].AsNumber();
    return placement;
}

}  // namespace

Result<HitmRigPlacement> HitmRigPlacement::Import(const std::filesystem::path& characterDir, const HitmPartsRig* crossCheck) {
    if (!std::filesystem::exists(characterDir) || !std::filesystem::is_directory(characterDir)) {
        return Result<HitmRigPlacement>::Fail("Character directory does not exist: " + characterDir.string());
    }

    HitmRigPlacement rig;
    rig.fighter_id_ = characterDir.filename().string();
    std::filesystem::path rigPath = characterDir / "rig.json";

    try {
        std::string text = ReadFile(rigPath);
        Value root;
        try {
            root = Value::Parse(text);
        } catch (const std::exception& e) {
            throw std::runtime_error(rigPath.string() + ": parse error: " + e.what());
        }
        if (!root.IsObject()) throw std::runtime_error(rigPath.string() + ": expected a JSON object at the top level");

        const Value* partsVal = root.Get("parts");
        if (!partsVal || !partsVal->IsObject()) throw std::runtime_error(rigPath.string() + ": missing object field 'parts'");

        for (const auto& [name, val] : partsVal->AsObject()) {
            rig.parts_.emplace(name, ExtractPlacement(name, val));
        }

        if (crossCheck) {
            for (const auto& [name, placement] : rig.parts_) {
                const HitmAtlasPart* atlasPart = nullptr;
                for (const auto& p : crossCheck->Parts()) {
                    if (p.name == name) { atlasPart = &p; break; }
                }
                if (!atlasPart) {
                    throw std::runtime_error("rig.json names part '" + name +
                                              "' which the same fighter's real parts.json does not have");
                }
                if (atlasPart->pivot_x != placement.pivot_x || atlasPart->pivot_y != placement.pivot_y) {
                    throw std::runtime_error("part '" + name + "' pivot disagrees between rig.json (" +
                                              std::to_string(placement.pivot_x) + ", " + std::to_string(placement.pivot_y) +
                                              ") and parts.json (" + std::to_string(atlasPart->pivot_x) + ", " +
                                              std::to_string(atlasPart->pivot_y) + ")");
                }
            }
            // Real, evidenced invariant (3/3 real fighters): the two
            // files name EXACTLY the same set of parts, not just
            // "rig.json is a subset of parts.json". A part parts.json
            // draws that rig.json has no placement for would silently
            // vanish from any real draw list built from this bundle --
            // fail loud here instead.
            for (const auto& p : crossCheck->Parts()) {
                if (!rig.parts_.count(p.name)) {
                    throw std::runtime_error("parts.json names part '" + p.name +
                                              "' which the same fighter's real rig.json does not have");
                }
            }
        }
    } catch (const std::exception& e) {
        return Result<HitmRigPlacement>::Fail("HitmRigPlacement::Import(" + characterDir.string() + "): " + e.what());
    }

    return Result<HitmRigPlacement>::Ok(std::move(rig));
}

const HitmPartPlacement* HitmRigPlacement::Part(const std::string& name) const {
    auto it = parts_.find(name);
    return it == parts_.end() ? nullptr : &it->second;
}

}  // namespace dominus::character::hitm
