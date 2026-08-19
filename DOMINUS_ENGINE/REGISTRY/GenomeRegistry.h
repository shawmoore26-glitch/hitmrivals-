// REGISTRY/GenomeRegistry.h
// Content-addressed storage: artifacts are keyed by their own hash, not
// by a mutable name/slot. Registering the same content twice is
// idempotent, not an error and not a second entry -- same content always
// hashes to the same key, so "overwriting" is structurally impossible.
// Lineage is tracked separately, per entity: an ordered list of hashes
// (v1 -> v2 -> v3), each artifact still individually immutable and still
// individually retrievable by its own hash even after newer versions
// exist -- nothing is ever deleted or replaced in place.
//
// MaterialGenome authority registration (this phase): the SAME class,
// extended, not a second registry. Before this phase, `artifacts_`/
// `lineage_` were keyed purely by hash/entityId -- safe only because
// exactly one genome kind (Combat, via ImmutableArtifact) could ever
// exist. Adding a second kind (Material) made that a real, if
// previously latent, gap: nothing stopped a MaterialGenome and a
// CombatGenome from colliding under the same key. Fixed by composing
// GenomeKind into both internal keys -- real type discrimination, not
// reliance on SHA-256 collision being merely astronomically unlikely.
// Every public method gained an optional GenomeKind parameter
// defaulting to kCombat, so every pre-existing call site (all of them
// CombatGenome-only) compiles and behaves identically, unchanged.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "REGISTRY/ImmutableArtifact.h"

namespace dominus::registry {

class GenomeRegistry {
public:
    // Returns true whether this was a new artifact or an idempotent
    // re-registration of identical content -- both are "success", the
    // distinction isn't meaningful to a content-addressed store.
    bool Register(ImmutableArtifact artifact) {
        const GenomeKind kind = artifact.Kind();
        const std::string hash = artifact.Hash();
        const std::string entityId = artifact.EntityId();
        const std::string artifactKey = CompositeKey(kind, hash);

        if (artifacts_.find(artifactKey) == artifacts_.end()) {
            artifacts_.emplace(artifactKey, std::move(artifact));
        }

        const std::string lineageKey = CompositeKey(kind, entityId);
        auto& lineage = lineage_[lineageKey];
        if (lineage.empty() || lineage.back() != hash) {
            lineage.push_back(hash);
        }
        return true;
    }

    const ImmutableArtifact* Find(const std::string& hash, GenomeKind kind = GenomeKind::kCombat) const {
        auto it = artifacts_.find(CompositeKey(kind, hash));
        return it == artifacts_.end() ? nullptr : &it->second;
    }

    std::vector<std::string> Lineage(const std::string& entityId, GenomeKind kind = GenomeKind::kCombat) const {
        auto it = lineage_.find(CompositeKey(kind, entityId));
        return it == lineage_.end() ? std::vector<std::string>{} : it->second;
    }

    const ImmutableArtifact* Latest(const std::string& entityId, GenomeKind kind = GenomeKind::kCombat) const {
        auto it = lineage_.find(CompositeKey(kind, entityId));
        if (it == lineage_.end() || it->second.empty()) return nullptr;
        return Find(it->second.back(), kind);
    }

    size_t ArtifactCount() const { return artifacts_.size(); }

private:
    static std::string CompositeKey(GenomeKind kind, const std::string& id) {
        return (kind == GenomeKind::kCombat ? std::string("combat:") : std::string("material:")) + id;
    }

    std::unordered_map<std::string, ImmutableArtifact> artifacts_;
    std::unordered_map<std::string, std::vector<std::string>> lineage_;
};

}  // namespace dominus::registry
