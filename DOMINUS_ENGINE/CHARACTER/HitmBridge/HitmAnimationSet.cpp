// CHARACTER/HitmBridge/HitmAnimationSet.cpp
#include "CHARACTER/HitmBridge/HitmAnimationSet.h"

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

HitmKeyframe ExtractKeyframe(const std::string& clipName, const std::string& trackName, const Value& kf) {
    if (!kf.IsArray() || kf.AsArray().size() != 4) {
        throw std::runtime_error("clip '" + clipName + "' track '" + trackName +
                                  "' has a keyframe that is not the real 4-element [frame, rot, dx, dy] shape");
    }
    const auto& arr = kf.AsArray();
    for (const auto& e : arr) {
        if (!e.IsNumber()) {
            throw std::runtime_error("clip '" + clipName + "' track '" + trackName +
                                      "' has a keyframe with a non-number element");
        }
    }
    HitmKeyframe out;
    out.frame = static_cast<int>(arr[0].AsNumber());
    out.rotation_deg = arr[1].AsNumber();
    out.offset_x = arr[2].AsNumber();
    out.offset_y = arr[3].AsNumber();
    return out;
}

HitmAnimationClip ExtractClip(const std::string& name, const Value& clipVal) {
    if (!clipVal.IsObject()) throw std::runtime_error("clip '" + name + "' is not an object");

    HitmAnimationClip clip;
    clip.name = name;

    const Value* loop = clipVal.Get("loop");
    if (!loop || !loop->IsBool()) throw std::runtime_error("clip '" + name + "' missing boolean field 'loop'");
    clip.loop = loop->AsBool();

    const Value* len = clipVal.Get("len");
    if (!len || !len->IsNumber()) throw std::runtime_error("clip '" + name + "' missing numeric field 'len'");
    clip.len = static_cast<int>(len->AsNumber());
    if (clip.len <= 0) throw std::runtime_error("clip '" + name + "' has non-positive 'len' (" + std::to_string(clip.len) + ")");

    const Value* tracks = clipVal.Get("tracks");
    if (!tracks || !tracks->IsObject()) throw std::runtime_error("clip '" + name + "' missing object field 'tracks'");

    for (const auto& [trackName, trackVal] : tracks->AsObject()) {
        if (!trackVal.IsArray() || trackVal.AsArray().empty()) {
            throw std::runtime_error("clip '" + name + "' track '" + trackName + "' is empty or not an array");
        }
        // NOT validated: strictly-increasing (or even non-decreasing)
        // frame order. Verified false as a real invariant -- see
        // HitmAnimationSet.h's header comment for the evidence (real
        // "anticipation snap" / vestigial keyframes across all three
        // real fighters, e.g. Brooklyn's real `light1`/`legFarU` track:
        // `[[0,0,0,0],[1,12,0,0],[0,-18,0,0],[11,0,0,0]]`). Asserting
        // monotonicity here would reject real, valid, generated HITM
        // data -- exactly the DOMINUS-invented rule this track refuses
        // to add. `Sample()`'s bracket search is written to tolerate
        // this by construction (a direct port of hitm-engine's own
        // `_sample()`, which never assumed monotonicity either).
        std::vector<HitmKeyframe> keyframes;
        keyframes.reserve(trackVal.AsArray().size());
        for (const auto& kfVal : trackVal.AsArray()) {
            keyframes.push_back(ExtractKeyframe(name, trackName, kfVal));
        }
        clip.tracks.emplace(trackName, std::move(keyframes));
    }

    return clip;
}

}  // namespace

HitmLocalPose HitmAnimationClip::Sample(const std::string& trackName, double frame) const {
    // Direct port of hitm-engine's SkeletonSystem.js `_sample()` -- see
    // this file's header comment. `!track || !track.length` -> zero pose.
    auto it = tracks.find(trackName);
    if (it == tracks.end() || it->second.empty()) return HitmLocalPose{};

    const auto& track = it->second;
    if (frame <= track.front().frame) {
        const auto& k = track.front();
        return HitmLocalPose{k.rotation_deg, k.offset_x, k.offset_y};
    }
    const auto& last = track.back();
    if (frame >= last.frame) {
        return HitmLocalPose{last.rotation_deg, last.offset_x, last.offset_y};
    }
    for (size_t i = 0; i + 1 < track.size(); ++i) {
        const auto& a = track[i];
        const auto& b = track[i + 1];
        if (frame >= a.frame && frame <= b.frame) {
            double t = (b.frame == a.frame) ? 0.0 : (frame - a.frame) / static_cast<double>(b.frame - a.frame);
            double s = t * t * (3.0 - 2.0 * t);  // smoothstep -- matches the real engine exactly
            return HitmLocalPose{a.rotation_deg + (b.rotation_deg - a.rotation_deg) * s,
                                  a.offset_x + (b.offset_x - a.offset_x) * s, a.offset_y + (b.offset_y - a.offset_y) * s};
        }
    }
    // Unreachable given the strictly-increasing invariant Import()
    // enforces, but matches the real function's own trailing fallback.
    return HitmLocalPose{last.rotation_deg, last.offset_x, last.offset_y};
}

Result<HitmAnimationSet> HitmAnimationSet::Import(const std::filesystem::path& characterDir) {
    if (!std::filesystem::exists(characterDir) || !std::filesystem::is_directory(characterDir)) {
        return Result<HitmAnimationSet>::Fail("Character directory does not exist: " + characterDir.string());
    }

    HitmAnimationSet set;
    set.fighter_id_ = characterDir.filename().string();
    std::filesystem::path animPath = characterDir / "anim.json";

    try {
        std::string text = ReadFile(animPath);
        Value root;
        try {
            root = Value::Parse(text);
        } catch (const std::exception& e) {
            throw std::runtime_error(animPath.string() + ": parse error: " + e.what());
        }
        if (!root.IsObject()) throw std::runtime_error(animPath.string() + ": expected a JSON object at the top level");

        for (const auto& [clipName, clipVal] : root.AsObject()) {
            set.clips_.emplace(clipName, ExtractClip(clipName, clipVal));
        }
    } catch (const std::exception& e) {
        return Result<HitmAnimationSet>::Fail("HitmAnimationSet::Import(" + characterDir.string() + "): " + e.what());
    }

    return Result<HitmAnimationSet>::Ok(std::move(set));
}

const HitmAnimationClip* HitmAnimationSet::Clip(const std::string& name) const {
    auto it = clips_.find(name);
    return it == clips_.end() ? nullptr : &it->second;
}

std::vector<std::string> HitmAnimationSet::ClipNames() const {
    std::vector<std::string> names;
    names.reserve(clips_.size());
    for (const auto& [name, clip] : clips_) names.push_back(name);
    return names;
}

}  // namespace dominus::character::hitm
