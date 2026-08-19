// REGISTRY/VisualGenomeCompiler.h
// Same honest scope as every non-CombatGenome compiler in this module:
// Validator -> Canonical Serializer -> SHA-256 Hash, no
// ImmutableArtifact (no decoder exists). A sixth data point for the
// still-open "does the hash pipeline generalize" question -- six real
// genome types now compile cleanly through the same discipline.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/VisualGenome.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::registry {

struct VisualCompileResult {
    bool ok = false;
    std::string canonical_bytes;
    std::string hash;
    std::vector<std::string> errors;
};

class VisualGenomeCompiler {
public:
    // No single required identity field here (form/skin/clothing/
    // presence are all either free-form or already-validated numerics)
    // -- a default-constructed VisualGenome is itself valid data, so
    // this always succeeds. Distinguishing "empty but valid" from "an
    // error" is the loader's job (VisualGenomeLoader), not the
    // compiler's.
    static VisualCompileResult Compile(const character::VisualGenome& genome) {
        VisualCompileResult result;
        result.canonical_bytes = CanonicalSerializer::SerializeVisualGenome(genome);
        result.hash = Sha256::Hash(result.canonical_bytes);
        result.ok = true;
        return result;
    }
};

}  // namespace dominus::registry
