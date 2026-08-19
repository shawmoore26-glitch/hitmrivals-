// REGISTRY/GenomeCompiler.h
// The pipeline: CombatGenome Source -> Validator -> Canonical Serializer
// -> SHA-256 Hash -> Immutable Artifact. Everything before this file
// already exists (CombatIdentityLoader reads the source, GenomeDecoder
// decodes it); this is the new stage that turns a live, mutable
// CombatIdentity into a compiled, hash-addressed, self-sufficient
// artifact.
//
// "Validator" here is intentionally independent of
// CombatIdentityLoader's own load-time check (which only requires
// non-empty 'style') -- a compiler stage should not blindly trust that
// whatever loaded successfully is compilable; it validates on its own
// terms, even if today those terms happen to be the same requirement.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CHARACTER/Genome/CombatIdentity.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Genome/MaterialGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"
#include "REGISTRY/ImmutableArtifact.h"
#include "REGISTRY/MaterialGenomeCompiler.h"

namespace dominus::registry {

struct CompileResult {
    bool ok = false;
    std::optional<ImmutableArtifact> artifact;
    std::vector<std::string> errors;
};

class GenomeCompiler {
public:
    static CompileResult CompileCombatGenome(const std::string& entityId, const character::CombatIdentity& identity,
                                              std::optional<std::string> parentHash, int versionNumber,
                                              const std::string& compiledAt) {
        CompileResult result;

        // Validator stage.
        if (identity.style.empty()) {
            result.errors.push_back("CombatGenome validation failed: 'style' is empty");
        }
        if (!result.errors.empty()) {
            result.ok = false;
            return result;
        }

        // Canonical Serializer stage.
        std::string canonical = CanonicalSerializer::SerializeCombatGenome(identity);

        // SHA-256 Hash stage.
        std::string hash = Sha256::Hash(canonical);

        // Decode weights now, at compile time -- baked into the artifact
        // so nothing downstream needs the original `identity` again.
        auto weights = character::GenomeDecoder::Decode(identity);

        // Immutable Artifact stage.
        result.ok = true;
        result.artifact = ImmutableArtifact(entityId, canonical, hash, weights, std::move(parentHash), versionNumber,
                                             compiledAt);
        return result;
    }

    // The MaterialGenome path -- new this phase, added as a sibling to
    // CompileCombatGenome above rather than a separate compiler class,
    // so GenomeCompiler remains the one real "genome -> registered
    // artifact" entry point for every genome kind. Reuses
    // MaterialGenomeCompiler::Compile's existing validation, canonical
    // serialization, and SHA-256 hash verbatim -- no second hashing
    // algorithm, no duplicated identity logic. This function's only
    // real job is wrapping that already-correct result into the same
    // ImmutableArtifact/GenomeRegistry boundary CombatGenome already
    // uses.
    static CompileResult CompileMaterialGenome(const std::string& entityId, const character::MaterialGenome& genome,
                                                std::optional<std::string> parentHash, int versionNumber,
                                                const std::string& compiledAt) {
        CompileResult result;

        MaterialCompileResult materialResult = MaterialGenomeCompiler::Compile(genome);
        if (!materialResult.ok) {
            result.ok = false;
            result.errors = materialResult.errors;
            return result;
        }

        result.ok = true;
        result.artifact = ImmutableArtifact(entityId, materialResult.canonical_bytes, materialResult.hash, genome,
                                             std::move(parentHash), versionNumber, compiledAt);
        return result;
    }
};

}  // namespace dominus::registry
