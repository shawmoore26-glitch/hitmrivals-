// REGISTRY/CombatStyleGenomeCompiler.h
// Same honest scope as CreatureGenomeCompiler: Validator -> Canonical
// Serializer -> SHA-256 Hash, no ImmutableArtifact. ImmutableArtifact
// bakes in CombatIdentity-specific DecisionWeights via GenomeDecoder;
// there is no CombatStyleGenome decoder (nothing translates aggression/
// defense/mobility/... into runtime decision weights yet), so producing
// a full artifact would mean faking that step. Hash + canonical bytes
// only, same as CreatureGenomeCompiler -- a third data point for the
// still-open "does ImmutableArtifact generalize" question, not an
// answer to it.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/CombatStyleGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct CombatStyleCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class CombatStyleGenomeCompiler {
public:
    static CombatStyleCompileResult Compile(const character::CombatStyleGenome& genome) {
        CombatStyleCompileResult result;
        if (genome.style_name.empty()) {
            result.errors.push_back("CombatStyleGenome compile failed: style_name is empty");
            return result;
        }

        result.canonical_bytes = CanonicalSerializer::SerializeCombatStyleGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
