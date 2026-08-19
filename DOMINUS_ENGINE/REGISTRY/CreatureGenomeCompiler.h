// REGISTRY/CreatureGenomeCompiler.h
// The second half of the "does CanonicalSerializer/hash discipline
// generalize" test: Validator -> Canonical Serializer -> SHA-256 Hash,
// same three stages GenomeCompiler runs for CombatGenome. Deliberately
// does NOT produce a full ImmutableArtifact -- that type bakes in
// character::DecisionWeights, which is CombatGenome-specific, and no
// CreatureGenome decoder exists to produce an equivalent. Returning
// hash+canonical bytes only, rather than reusing/misusing
// ImmutableArtifact, is the honest choice: an artifact claiming to hold
// "decoded weights" that are actually just zeros would be exactly the
// kind of fake data this engine has consistently refused to produce.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CHARACTER/Genome/CreatureGenome.h"
#include "CHARACTER/Genome/CreatureGenomeLoader.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct CreatureCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class CreatureGenomeCompiler {
public:
    // The genome is assumed already validated by CreatureGenomeLoader
    // (which refuses to construct an invalid CreatureGenome in the first
    // place) -- this compiler's own "Validator" stage re-checks the one
    // invariant that matters for hashing specifically: a genome with no
    // species name has no meaningful identity to hash against.
    static CreatureCompileResult Compile(const character::CreatureGenome& genome) {
        CreatureCompileResult result;
        if (genome.identity.species_name.empty()) {
            result.errors.push_back("CreatureGenome compile failed: species_name is empty");
            return result;
        }

        result.canonical_bytes = CanonicalSerializer::SerializeCreatureGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
