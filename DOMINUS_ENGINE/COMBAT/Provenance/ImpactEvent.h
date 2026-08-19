// COMBAT/Provenance/ImpactEvent.h
// Phase 4.1.6: the missing proof after Phase 4.1.5 wired
// ImpactContext -> ImpactSolver::Solve() -> ImpactResult ->
// ReactionSystem::Apply() was "can an impact be recorded and replayed?"
// ImpactEvent is the answer's schema -- a small, hash-addressed record
// of WHO hit WHOM, with WHAT inputs, producing WHAT result, at WHICH
// tick. It does not store the full ImpactContext/ImpactResult inline
// (that would make this a second copy of live combat data, not
// provenance) -- it stores their hashes, exactly the same
// content-addressing discipline REGISTRY::GenomeCompiler already
// established for genomes. Determinism is what makes that honest:
// re-running the same ImpactContext through ImpactSolver::Solve()
// always produces the same ImpactResult, so the hash alone is enough
// to prove "this exact impact happened" without duplicating the data.
#pragma once

#include <cstdint>
#include <string>

namespace dominus::combat {

struct ImpactEvent {
    std::string event = "IMPACT";  // free-form event-type tag, same discipline as
                                    // WorldHistory's own event_type field
    std::string attacker;          // entity id
    std::string target;            // entity id
    std::string context_hash;      // SHA-256 of the canonical ImpactContext that produced this impact
    std::string result_hash;       // SHA-256 of the canonical ImpactResult ImpactSolver::Solve() returned
    std::uint64_t tick = 0;        // a deterministic simulation tick, not wall-clock time
};

}  // namespace dominus::combat
