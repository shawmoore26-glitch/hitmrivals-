// REGISTRY/RuntimeSnapshot.h
// "Runtime never touches authored data" as a structural fact, not a
// convention someone could accidentally violate: RuntimeSnapshot holds
// nothing but a value-copy of DecisionWeights and a hash string. It has
// no reference, pointer, or path back to a CombatIdentity, a .dominus
// file, or anything mutable. SnapshotBuilder::Build takes only an
// ImmutableArtifact -- there is no overload that accepts source data, so
// there is no way to build a snapshot that skips the compiled artifact.
#pragma once

#include <string>

#include "CHARACTER/Genome/DecisionWeights.h"
#include "REGISTRY/ImmutableArtifact.h"

namespace dominus::registry {

class RuntimeSnapshot {
public:
    RuntimeSnapshot(character::DecisionWeights weights, std::string sourceHash)
        : weights_(weights), source_hash_(std::move(sourceHash)) {}

    const character::DecisionWeights& Weights() const { return weights_; }
    const std::string& SourceHash() const { return source_hash_; }

private:
    character::DecisionWeights weights_;
    std::string source_hash_;
};

class SnapshotBuilder {
public:
    static RuntimeSnapshot Build(const ImmutableArtifact& artifact) {
        return RuntimeSnapshot(artifact.Weights(), artifact.Hash());
    }
};

}  // namespace dominus::registry
