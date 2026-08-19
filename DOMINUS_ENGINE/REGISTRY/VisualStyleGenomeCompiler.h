// REGISTRY/VisualStyleGenomeCompiler.h
// Same honest scope as every non-CombatGenome compiler: Validator ->
// Canonical Serializer -> SHA-256 Hash, no ImmutableArtifact. An eighth
// data point for the still-open "does the hash pipeline generalize"
// question.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/VisualStyleGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct VisualStyleCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class VisualStyleGenomeCompiler {
public:
    static VisualStyleCompileResult Compile(const character::VisualStyleGenome& genome) {
        VisualStyleCompileResult result;
        if (genome.style_id.empty()) {
            result.errors.push_back("VisualStyleGenome compile failed: style_id is empty");
            return result;
        }

        result.canonical_bytes = CanonicalSerializer::SerializeVisualStyleGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
