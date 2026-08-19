// REALITY/RealityRegistry.h
// Milestone 4's "REGISTRY UPDATE" step, made real. A small, on-disk
// ledger of the last known-good hash for each dependency graph node --
// the baseline `RealityRebuilder` diffs the live filesystem against to
// decide whether a node's authoritative hash actually changed. Uses
// CORE::json::Value, the same JSON round-trip machinery
// DominusSerializer already trusts -- no second, invented
// serialization format.
//
// This file does not depend on any specific graph type -- it only
// stores/loads a subject name and a node_id -> hash map, agnostic to
// whether the caller's graph model is REALITY::DependencyGraph (the
// original, Milestone 3/6 authority) or REALITY::EvidenceGraph (the
// current, Milestone 7 authority `RealityRebuilder` migrated onto).
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace dominus::reality {

struct RealityRegistry {
    std::string subject;
    std::map<std::string, std::string> node_hashes;  // node id -> last known-good hash

    // Loads a previously-saved registry. std::nullopt if the file
    // doesn't exist, is empty, or fails to parse -- never a fabricated
    // empty-but-valid registry silently standing in for "no baseline."
    static std::optional<RealityRegistry> Load(const std::filesystem::path& path);

    // Writes the registry to disk as real JSON, atomically: a temp
    // file is written and then renamed over `path`, so a failure or
    // interruption partway through never leaves a half-written or
    // corrupted file at `path` -- whatever was there before either
    // stays exactly as it was, or is replaced whole. Returns false on
    // a real I/O failure.
    bool Save(const std::filesystem::path& path) const;

    const std::string* Find(const std::string& nodeId) const;
};

}  // namespace dominus::reality
