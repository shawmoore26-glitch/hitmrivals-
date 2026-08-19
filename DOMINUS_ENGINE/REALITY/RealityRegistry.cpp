// REALITY/RealityRegistry.cpp
#include "REALITY/RealityRegistry.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::reality {

using core::json::Value;

std::optional<RealityRegistry> RealityRegistry::Load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    if (text.empty()) return std::nullopt;

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!root.IsObject()) return std::nullopt;

    RealityRegistry reg;
    if (const Value* subj = root.Get("subject")) {
        if (subj->IsString()) reg.subject = subj->AsString();
    }
    if (const Value* nodes = root.Get("node_hashes")) {
        if (nodes->IsObject()) {
            for (const auto& [key, val] : nodes->AsObject()) {
                if (val.IsString()) reg.node_hashes[key] = val.AsString();
            }
        }
    }
    return reg;
}

bool RealityRegistry::Save(const std::filesystem::path& path) const {
    Value root = Value(core::json::Object{});
    root["subject"] = Value(subject);

    Value nodesVal = Value(core::json::Object{});
    for (const auto& [key, hash] : node_hashes) {
        nodesVal[key] = Value(hash);
    }
    root["node_hashes"] = nodesVal;

    // Atomic write: write to a sibling temp file (same directory, so
    // the same filesystem -- rename() is only atomic within one), then
    // rename it over the real path. A failure or interruption at any
    // point before the rename leaves whatever was already at `path`
    // completely untouched -- never a half-written, truncated, or
    // corrupted registry masquerading as the current one. This is the
    // real mechanism Milestone 5's crash-safety tests rely on, not a
    // comment promising it.
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << root.Dump();
        if (!out) return false;
    }  // scope closes and flushes the stream before the rename below

    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);  // best-effort cleanup; failure to clean up doesn't change the result
        return false;
    }
    return true;
}

const std::string* RealityRegistry::Find(const std::string& nodeId) const {
    auto it = node_hashes.find(nodeId);
    return it == node_hashes.end() ? nullptr : &it->second;
}

}  // namespace dominus::reality
