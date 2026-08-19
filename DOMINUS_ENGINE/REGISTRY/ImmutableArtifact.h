// REGISTRY/ImmutableArtifact.h
// The compiled output of GenomeCompiler. Structurally immutable: every
// member is private with only const getters, no setters anywhere, no
// mutation API surface at all. This is what "compiled assets never
// change" means in code, not just in a comment -- there is no method on
// this type that can alter it after construction.
//
// DecisionWeights are baked in at compile time (see GenomeCompiler), not
// re-derived at runtime -- the whole point of the pipeline is that once
// this artifact exists, nothing downstream (Registry, RuntimeSnapshot)
// ever needs to touch the original authored CombatIdentity again.
//
// MaterialGenome authority registration (this phase): extended, not
// redesigned. GenomeKind and a second constructor overload were added
// so this SAME class can carry either a compiled CombatGenome
// (DecisionWeights) or a compiled MaterialGenome -- not a parallel
// artifact type. The original 7-argument CombatGenome constructor is
// byte-for-byte unchanged; every existing call site compiles and
// behaves identically. The new MaterialGenome overload is
// unambiguous at the call site (the compiler picks it purely from the
// payload's static type, DecisionWeights vs. character::MaterialGenome)
// -- both fields exist on every instance, but only the one matching
// Kind() is ever meaningful; the other stays default-constructed.
#pragma once

#include <optional>
#include <string>

#include "CHARACTER/Genome/DecisionWeights.h"
#include "CHARACTER/Genome/MaterialGenome.h"

namespace dominus::registry {

enum class GenomeKind { kCombat, kMaterial };

class ImmutableArtifact {
public:
    // CombatGenome artifact -- unchanged signature, unchanged behavior.
    ImmutableArtifact(std::string entityId, std::string canonicalBytes, std::string hash,
                       character::DecisionWeights weights, std::optional<std::string> parentHash, int versionNumber,
                       std::string compiledAt)
        : entity_id_(std::move(entityId)),
          canonical_bytes_(std::move(canonicalBytes)),
          hash_(std::move(hash)),
          kind_(GenomeKind::kCombat),
          weights_(weights),
          parent_hash_(std::move(parentHash)),
          version_number_(versionNumber),
          compiled_at_(std::move(compiledAt)) {}

    // MaterialGenome artifact -- new this phase. Reuses the exact same
    // fields (entityId/canonicalBytes/hash/parentHash/versionNumber/
    // compiledAt); only the payload type and Kind() differ.
    ImmutableArtifact(std::string entityId, std::string canonicalBytes, std::string hash,
                       character::MaterialGenome material, std::optional<std::string> parentHash, int versionNumber,
                       std::string compiledAt)
        : entity_id_(std::move(entityId)),
          canonical_bytes_(std::move(canonicalBytes)),
          hash_(std::move(hash)),
          kind_(GenomeKind::kMaterial),
          material_genome_(std::move(material)),
          parent_hash_(std::move(parentHash)),
          version_number_(versionNumber),
          compiled_at_(std::move(compiledAt)) {}

    const std::string& EntityId() const { return entity_id_; }
    const std::string& CanonicalBytes() const { return canonical_bytes_; }
    const std::string& Hash() const { return hash_; }
    GenomeKind Kind() const { return kind_; }
    // Meaningful only when Kind() == kCombat -- default-constructed
    // (empty) otherwise, the same "only one payload is real" discipline
    // MaterialGenome() below follows in the other direction.
    const character::DecisionWeights& Weights() const { return weights_; }
    // Meaningful only when Kind() == kMaterial.
    const character::MaterialGenome& Material() const { return material_genome_; }
    const std::optional<std::string>& ParentHash() const { return parent_hash_; }
    int VersionNumber() const { return version_number_; }
    const std::string& CompiledAt() const { return compiled_at_; }

private:
    std::string entity_id_;
    std::string canonical_bytes_;
    std::string hash_;
    GenomeKind kind_;
    character::DecisionWeights weights_;
    character::MaterialGenome material_genome_;
    std::optional<std::string> parent_hash_;
    int version_number_;
    std::string compiled_at_;
};

}  // namespace dominus::registry
