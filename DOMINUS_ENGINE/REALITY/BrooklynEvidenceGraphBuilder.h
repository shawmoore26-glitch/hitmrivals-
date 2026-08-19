// REALITY/BrooklynEvidenceGraphBuilder.h
// Builds Brooklyn's real EvidenceGraph -- the concrete demonstration
// that REALITY/EvidenceGraph.h's primitives can be filled in from
// actual repository evidence rather than a hand-maintained list.
//
// The core mechanism: `brooklyn_canonical.dominus` is parsed
// structurally (via CORE::json, the same parser DominusSerializer
// already uses) and every ref-bearing key is discovered generically --
// this file does not hardcode "visual_genome" or "material_genome" by
// name as a fixed list. If a future .dominus schema change adds a new
// top-level `{"ref": "..."}` block, this builder picks it up
// automatically; the only genuinely hand-written parts are the
// citations for WHICH compiled node consumes each discovered artifact
// (the INPUT/RUNTIME edges), and those citations name the real
// function signature or binder behavior they're derived from -- see
// REALITY/BrooklynEvidenceGraphBuilder.cpp's comments for each one.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "REALITY/EvidenceGraph.h"

namespace dominus::reality {

class BrooklynEvidenceGraphBuilder {
public:
    // Builds the full, real evidence graph for Brooklyn from
    // `fixtureDir`: discovers every ref-bearing artifact declared in
    // brooklyn_canonical.dominus, computes a real hash for each
    // (raw file bytes -- see the .cpp for why this graph uses raw
    // bytes uniformly rather than VISUALFORGE's parsed-content hashing),
    // derives INPUT/RUNTIME/DERIVED edges from real, cited evidence,
    // and additionally runs the real RIG/VisualForge compilers (the
    // same internal::CompileRig/CompileVisualForge Milestone 4 already
    // uses) to give the certificate and artifact nodes real hashes too
    // -- "every node has a real hash" is not scoped down to only the
    // source nodes.
    static EvidenceGraph Build(const std::filesystem::path& fixtureDir);

    // The fast path Build() cannot be: every source-file node/edge is
    // computed identically (same real discovery, same real hashes, same
    // cited edges), but the certificate/artifact nodes are left present
    // with empty hashes rather than actually running internal::
    // CompileRig/CompileVisualForge. Computing a certificate hash means
    // actually running RIG/VisualForge's real acceptance harnesses --
    // real, deliberate work that a caller doing pure change-detection
    // (comparing one source node's live hash against a stored baseline)
    // should not have to pay for on every single call. This mirrors
    // Milestone 3's own "fast graph" discipline
    // (RealityCompiler::BuildBrooklynDependencyGraph) for the new,
    // evidence-derived graph.
    static EvidenceGraph BuildStructure(const std::filesystem::path& fixtureDir);

    // Exposes the same structural ref-discovery `Build` uses
    // internally, without paying for hash computation or the RIG/
    // VisualForge compiler runs -- a real, cheap way for a caller
    // (Milestone 10's ChangeEventNormalizer) to ask "which real files
    // are declared authoritative artifacts, and which node id does
    // each belong to" without needing a full graph build. Returns
    // node_id ("brooklyn.skeleton", "brooklyn.visual_genome", ...) ->
    // the real filename(s) that node's hash is computed from. This is
    // the SAME discovery mechanism `Build` uses, exposed for a second
    // purpose -- not a second, hand-maintained list.
    static std::map<std::string, std::vector<std::string>> DiscoverArtifactFiles(
        const std::filesystem::path& fixtureDir);
};

}  // namespace dominus::reality
