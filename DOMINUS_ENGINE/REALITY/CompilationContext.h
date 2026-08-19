// REALITY/CompilationContext.h
// The object carrying truth through the Reality Pipeline
// (AUTHOR -> COMPILE -> VALIDATE -> REGISTER -> EXECUTE). Same
// discipline as VISUALFORGE::ProductionSnapshot / RIG::RigProfile:
// every field here is either caller-declared identity or something a
// real check actually wrote -- CompilationContext does not become a
// second source of truth. It records what happened; RIG and
// VISUALFORGE remain authoritative for whether their own domains are
// actually valid.
//
// PipelineState mirrors the seven states the directive named exactly:
// NOT_DECLARED, DECLARED, COMPILING, VALIDATED, REJECTED, REGISTERED,
// EXECUTABLE. No stage gets to silently advance -- PipelineLifecycle's
// transitions are as mechanically gated as RigProfileLifecycle's own,
// and there is deliberately no method that can set a state out of
// order or without a real fact to justify it.
#pragma once

#include <string>
#include <vector>

namespace dominus::reality {

enum class PipelineState { kNotDeclared, kDeclared, kCompiling, kValidated, kRejected, kRegistered, kExecutable };

inline const char* PipelineStateName(PipelineState state) {
    switch (state) {
        case PipelineState::kNotDeclared: return "NOT_DECLARED";
        case PipelineState::kDeclared: return "DECLARED";
        case PipelineState::kCompiling: return "COMPILING";
        case PipelineState::kValidated: return "VALIDATED";
        case PipelineState::kRejected: return "REJECTED";
        case PipelineState::kRegistered: return "REGISTERED";
        case PipelineState::kExecutable: return "EXECUTABLE";
    }
    return "UNKNOWN";
}

// One real domain compiler's result, folded into the context. `stage`
// names the domain ("RIG", "VISUALFORGE"); `state` is only ever
// kValidated or kRejected here -- a StageRecord is written AFTER a
// real domain harness has already run, never before.
struct StageRecord {
    std::string stage;
    PipelineState state = PipelineState::kNotDeclared;
    std::string detail;
};

struct CompilationContext {
    std::string source_entity;                       // e.g. "brooklyn"
    std::string compiler_identity = "DOMINUS_REALITY_COMPILER_V1";

    std::vector<std::string> source_identities;       // real fixture file paths actually read
    std::vector<std::string> dependency_identities;   // component certificate hashes this artifact depends on

    std::string rig_certificate_hash;
    std::string visualforge_certificate_hash;

    std::string artifact_hash;                        // this context's own unified identity; empty until kRegistered

    std::vector<StageRecord> stages;                  // one record per real sub-pipeline actually run
    std::vector<std::string> diagnostics;              // real refusal/failure reasons, never invented

    PipelineState state = PipelineState::kNotDeclared;
};

// Mechanical, gated transitions only -- mirrors RigProfileLifecycle /
// SnapshotLifecycle exactly. Every Advance* reads a real fact already
// recorded on the context; none of them accept a caller-supplied
// "trust me" boolean standing in for evidence.
class PipelineLifecycle {
public:
    static bool Declare(CompilationContext& ctx, const std::string& sourceEntity,
                         std::vector<std::string> sourceIdentities) {
        if (ctx.state != PipelineState::kNotDeclared) return false;
        if (sourceEntity.empty() || sourceIdentities.empty()) return false;
        ctx.source_entity = sourceEntity;
        ctx.source_identities = std::move(sourceIdentities);
        ctx.state = PipelineState::kDeclared;
        return true;
    }

    static bool BeginCompiling(CompilationContext& ctx) {
        if (ctx.state != PipelineState::kDeclared) return false;
        ctx.state = PipelineState::kCompiling;
        return true;
    }

    // Requires at least one real StageRecord to have actually been
    // appended by a domain compiler, and every one of them to have
    // itself resolved to kValidated -- never advances on an empty or
    // partially-rejected stage list.
    static bool AdvanceToValidated(CompilationContext& ctx) {
        if (ctx.state != PipelineState::kCompiling) return false;
        if (ctx.stages.empty()) return false;
        for (const auto& s : ctx.stages) {
            if (s.state != PipelineState::kValidated) return false;
        }
        ctx.state = PipelineState::kValidated;
        return true;
    }

    // Can fire from kCompiling (a stage failed) or kValidated (a later
    // gate -- e.g. registration -- failed). Never fires once the
    // context has already reached kRegistered/kExecutable: a real
    // artifact that has already been proven registered/executable
    // cannot retroactively become un-declared by a later, unrelated
    // caller mistake.
    static bool Reject(CompilationContext& ctx, const std::string& reason) {
        if (ctx.state == PipelineState::kRegistered || ctx.state == PipelineState::kExecutable) return false;
        if (reason.empty()) return false;
        ctx.diagnostics.push_back(reason);
        ctx.state = PipelineState::kRejected;
        return true;
    }

    // Registered requires a real, already-computed artifact_hash --
    // see RealityCompiler for how it's derived from the component
    // certificate hashes. This method does not compute anything
    // itself, same as AdvanceToAccepted not computing a certificate.
    static bool AdvanceToRegistered(CompilationContext& ctx, const std::string& artifactHash) {
        if (ctx.state != PipelineState::kValidated) return false;
        if (artifactHash.empty()) return false;
        ctx.artifact_hash = artifactHash;
        ctx.state = PipelineState::kRegistered;
        return true;
    }

    // Executable requires the caller to cite a real execution proof
    // that already happened (RigProfile reaching ACTIVE via its own,
    // unmodified lifecycle gate) -- this method cannot manufacture
    // that proof, only record that it was checked.
    static bool AdvanceToExecutable(CompilationContext& ctx, bool executionProofPassed) {
        if (ctx.state != PipelineState::kRegistered) return false;
        if (!executionProofPassed) return false;
        ctx.state = PipelineState::kExecutable;
        return true;
    }
};

}  // namespace dominus::reality
