// REGISTRY/CombatPhysicsGenomeCompiler.h
// Same honest scope as CreatureGenomeCompiler/CombatStyleGenomeCompiler:
// Validator -> Canonical Serializer -> SHA-256 Hash, no ImmutableArtifact
// (no decoder exists that translates body/energy/impact data into
// runtime DecisionWeights). A fourth data point for the still-open
// "does the hash pipeline generalize" question.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/CombatPhysicsGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct CombatPhysicsCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class CombatPhysicsGenomeCompiler {
public:
    // No single "identity" field to check for emptiness here (unlike
    // species_name/style_name) -- mass_kg <= 0 is the closest analogue
    // to "this genome has no real body to compile," since every other
    // field has a sane, valid default.
    static CombatPhysicsCompileResult Compile(const character::CombatPhysicsGenome& genome) {
        CombatPhysicsCompileResult result;
        if (genome.body.mass_kg <= 0.0f) {
            result.errors.push_back("CombatPhysicsGenome compile failed: body.mass_kg is not positive");
            return result;
        }

        result.canonical_bytes = CanonicalSerializer::SerializeCombatPhysicsGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
