// REGISTRY/GameDesignGenomeCompiler.h
// Same honest scope as every other non-CombatGenome compiler in this
// module: Validator -> Canonical Serializer -> SHA-256 Hash, no
// ImmutableArtifact (no decoder exists). A fifth data point for the
// still-open "does the hash pipeline generalize" question.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/GameDesignGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct GameDesignCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class GameDesignGenomeCompiler {
public:
    static GameDesignCompileResult Compile(const character::GameDesignGenome& genome) {
        GameDesignCompileResult result;
        if (genome.genre.empty()) {
            result.errors.push_back("GameDesignGenome compile failed: genre is empty");
            return result;
        }

        result.canonical_bytes = CanonicalSerializer::SerializeGameDesignGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
