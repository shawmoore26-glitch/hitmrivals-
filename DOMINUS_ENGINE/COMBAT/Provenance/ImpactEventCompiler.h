// COMBAT/Provenance/ImpactEventCompiler.h
// Source (ImpactContext + ImpactResult) -> Canonical Serializer ->
// SHA-256 Hash -> ImpactEvent. The exact same pipeline shape
// REGISTRY::GenomeCompiler already established for genomes, applied to
// a live combat event instead of authored data. Deliberately does NOT
// store the context/result themselves on the event -- only their
// hashes -- so an ImpactEvent stays small and hash-addressed, same
// discipline as REGISTRY::ImmutableArtifact.
#pragma once

#include <cstdint>
#include <string>

#include "COMBAT/PhysicsCombat/ImpactSolver.h"
#include "COMBAT/Provenance/ImpactCanonicalSerializer.h"
#include "COMBAT/Provenance/ImpactEvent.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::combat {

class ImpactEventCompiler {
public:
    static ImpactEvent Compile(const std::string& attackerId, const std::string& targetId, const ImpactContext& ctx,
                                const ImpactResult& result, std::uint64_t tick) {
        ImpactEvent event;
        event.attacker = attackerId;
        event.target = targetId;
        event.context_hash = registry::Sha256::Hash(ImpactCanonicalSerializer::SerializeContext(ctx));
        event.result_hash = registry::Sha256::Hash(ImpactCanonicalSerializer::SerializeResult(result));
        event.tick = tick;
        return event;
    }

    // The replay check: recompute both hashes fresh from a context/
    // result pair and confirm they match a previously recorded event.
    // This is what "deterministic combat history" actually means in
    // code -- not narration, a hash comparison against a real
    // recomputation.
    static bool VerifyMatches(const ImpactEvent& event, const ImpactContext& ctx, const ImpactResult& result) {
        std::string recomputedContextHash = registry::Sha256::Hash(ImpactCanonicalSerializer::SerializeContext(ctx));
        std::string recomputedResultHash = registry::Sha256::Hash(ImpactCanonicalSerializer::SerializeResult(result));
        return recomputedContextHash == event.context_hash && recomputedResultHash == event.result_hash;
    }
};

}  // namespace dominus::combat
