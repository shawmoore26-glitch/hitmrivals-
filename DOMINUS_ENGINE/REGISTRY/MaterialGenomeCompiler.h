// REGISTRY/MaterialGenomeCompiler.h
// Same honest scope as every non-CombatGenome compiler: Validator ->
// Canonical Serializer -> SHA-256 Hash, no ImmutableArtifact.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/MaterialGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct MaterialCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class MaterialGenomeCompiler {
public:
    static MaterialCompileResult Compile(const character::MaterialGenome& genome) {
        MaterialCompileResult result;
        if (genome.material_id.empty()) {
            result.errors.push_back("MaterialGenome compile failed: material_id is empty");
            return result;
        }

        result.canonical_bytes = CanonicalSerializer::SerializeMaterialGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
