// tests/reality/test_reality_compiler.cpp
// The Reality Pipeline, live: proves REALITY can coordinate RIG and
// VISUALFORGE's own real acceptance harnesses for Brooklyn -- without
// reimplementing either one -- through DECLARED -> COMPILING ->
// VALIDATED -> REGISTERED -> EXECUTABLE, and proves the directive's
// own "SOURCE -> COMPILE -> ARTIFACT A -> RECONSTRUCT -> ARTIFACT B ->
// A == B" claim by actually running the whole pipeline twice.
#include "REALITY/CompilationContext.h"
#include "REALITY/RealityCompiler.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::reality::CompilationContext;
using dominus::reality::PipelineLifecycle;
using dominus::reality::PipelineState;
using dominus::reality::RealityCompilationResult;
using dominus::reality::RealityCompiler;

namespace {
std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}
}  // namespace

// --- End-to-end: Brooklyn through the real pipeline -------------------------
//
// Both domains now genuinely validate: brooklyn_canonical.dominus
// carries real skeleton/animation/combat/runtime evidence (RIG) AND a
// real, bound visual_genome/material_genome/visual_style_genome
// (VISUALFORGE) -- the same, unaltered appearance data
// brooklyn.dominus already carried, bound by reference (not copied,
// not invented) because none of that content is skeleton-coupled. See
// REALITY/README.md and tests/fixtures/brooklyn_canonical.dominus's
// own provenance block (creation_method:
// "rig_migration_visual_carry_forward", parent_entities: ["brooklyn"])
// for the full trace. These tests assert the current, real,
// EXECUTABLE outcome -- if this fixture ever regresses (a rig
// migration that forgets to carry the visual genome forward again),
// these tests fail loudly instead of staying silently green.

DOMINUS_TEST(RealityCompiler_CompileBrooklyn_RunsBothDomainsAndBothValidate) {
    auto result = RealityCompiler::CompileBrooklyn(FixtureDir());
    DOMINUS_EXPECT(result.ran);
    DOMINUS_EXPECT(!result.rig_certificate.certificate_hash.empty());
    DOMINUS_EXPECT(!result.visualforge_certificate.certificate_hash.empty());
    DOMINUS_EXPECT(result.context.stages.size() == 2);
    DOMINUS_EXPECT(result.context.stages[0].stage == "RIG");
    DOMINUS_EXPECT(result.context.stages[0].state == PipelineState::kValidated);
    DOMINUS_EXPECT(result.context.stages[1].stage == "VISUALFORGE");
    DOMINUS_EXPECT(result.context.stages[1].state == PipelineState::kValidated);
}

DOMINUS_TEST(RealityCompiler_CompileBrooklyn_ReachesExecutable) {
    auto result = RealityCompiler::CompileBrooklyn(FixtureDir());
    DOMINUS_EXPECT(result.rig_certificate.overall_pass);
    DOMINUS_EXPECT(result.visualforge_certificate.structurally_sound);
    DOMINUS_EXPECT(result.visualforge_certificate.renderable);
    DOMINUS_EXPECT(result.context.state == PipelineState::kExecutable);
    DOMINUS_EXPECT(!result.context.artifact_hash.empty());
    DOMINUS_EXPECT(result.execution_certificate.executable);
    DOMINUS_EXPECT(result.context.diagnostics.empty());
}

DOMINUS_TEST(RealityCompiler_ExecutionCertificate_CitesRigRuntimeAndDeterminismSections) {
    auto result = RealityCompiler::CompileBrooklyn(FixtureDir());
    bool foundRuntime = false;
    bool foundDeterminism = false;
    for (const auto& s : result.execution_certificate.sections) {
        if (s.name == "Execution.Runtime") {
            foundRuntime = true;
            DOMINUS_EXPECT(s.passed);
        }
        if (s.name == "Execution.Determinism") {
            foundDeterminism = true;
            DOMINUS_EXPECT(s.passed);
        }
    }
    DOMINUS_EXPECT(foundRuntime);
    DOMINUS_EXPECT(foundDeterminism);
}

DOMINUS_TEST(RealityCompiler_ArtifactHash_DerivedFromBothCertificateHashes) {
    auto result = RealityCompiler::CompileBrooklyn(FixtureDir());
    // Changing either component certificate hash must change the
    // artifact hash -- proven indirectly: the artifact hash must
    // differ from either certificate hash alone (it's a real
    // combination, not an alias for one side).
    DOMINUS_EXPECT(result.context.artifact_hash != result.context.rig_certificate_hash);
    DOMINUS_EXPECT(result.context.artifact_hash != result.context.visualforge_certificate_hash);
    DOMINUS_EXPECT(result.context.dependency_identities.size() == 2);
}

// --- RECONSTRUCT -> REPRODUCE: the directive's own A == B test -------------
// Run from the canonical SOURCE fixtures both times -- CompileBrooklyn
// always does a fresh DominusSerializer::Load + fresh RigBinder::Bind
// + fresh CharacterAcceptanceHarness/RendererPackageAcceptanceHarness
// run, never reuses a previous run's in-memory certificate. Two
// completely independent compiles, from identical source inputs, must
// converge on the identical artifact_hash -- and do.

DOMINUS_TEST(RealityCompiler_ReproduceAndVerify_ProducesIdenticalArtifactHashAcrossIndependentRuns) {
    std::string hashA, hashB;
    bool identical = RealityCompiler::ReproduceAndVerify(FixtureDir(), hashA, hashB);
    DOMINUS_EXPECT(identical);
    DOMINUS_EXPECT(!hashA.empty());
    DOMINUS_EXPECT(hashA == hashB);
}

DOMINUS_TEST(RealityCompiler_ReproduceAndVerify_BothRunsIndependentlyReachExecutable) {
    auto runA = RealityCompiler::CompileBrooklyn(FixtureDir());
    auto runB = RealityCompiler::CompileBrooklyn(FixtureDir());
    DOMINUS_EXPECT(runA.context.state == PipelineState::kExecutable);
    DOMINUS_EXPECT(runB.context.state == PipelineState::kExecutable);
    DOMINUS_EXPECT(runA.rig_certificate.certificate_hash == runB.rig_certificate.certificate_hash);
    DOMINUS_EXPECT(runA.visualforge_certificate.certificate_hash == runB.visualforge_certificate.certificate_hash);
    DOMINUS_EXPECT(runA.context.artifact_hash == runB.context.artifact_hash);
}

// --- Pipeline state machine: mechanical gating, no shortcuts ---------------

DOMINUS_TEST(PipelineLifecycle_Declare_RequiresNonEmptySourceEntityAndIdentities) {
    CompilationContext ctx;
    DOMINUS_EXPECT(!PipelineLifecycle::Declare(ctx, "", {"a"}));
    DOMINUS_EXPECT(!PipelineLifecycle::Declare(ctx, "brooklyn", {}));
    DOMINUS_EXPECT(ctx.state == PipelineState::kNotDeclared);
    DOMINUS_EXPECT(PipelineLifecycle::Declare(ctx, "brooklyn", {"a"}));
    DOMINUS_EXPECT(ctx.state == PipelineState::kDeclared);
}

DOMINUS_TEST(PipelineLifecycle_CannotSkipStates) {
    CompilationContext ctx;
    // Can't begin compiling before declaring.
    DOMINUS_EXPECT(!PipelineLifecycle::BeginCompiling(ctx));
    // Can't validate before compiling.
    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToValidated(ctx));
    // Can't register before validating.
    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToRegistered(ctx, "somehash"));
    // Can't reach executable before registering.
    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToExecutable(ctx, true));
}

DOMINUS_TEST(PipelineLifecycle_AdvanceToValidated_RequiresEveryStageToHaveValidated) {
    CompilationContext ctx;
    PipelineLifecycle::Declare(ctx, "brooklyn", {"a"});
    PipelineLifecycle::BeginCompiling(ctx);
    // Empty stage list: never validates.
    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToValidated(ctx));

    ctx.stages.push_back({"RIG", PipelineState::kValidated, "ok"});
    ctx.stages.push_back({"VISUALFORGE", PipelineState::kRejected, "bad"});
    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToValidated(ctx));

    ctx.stages[1].state = PipelineState::kValidated;
    DOMINUS_EXPECT(PipelineLifecycle::AdvanceToValidated(ctx));
    DOMINUS_EXPECT(ctx.state == PipelineState::kValidated);
}

DOMINUS_TEST(PipelineLifecycle_AdvanceToRegistered_RequiresNonEmptyArtifactHash) {
    CompilationContext ctx;
    PipelineLifecycle::Declare(ctx, "brooklyn", {"a"});
    PipelineLifecycle::BeginCompiling(ctx);
    ctx.stages.push_back({"RIG", PipelineState::kValidated, "ok"});
    PipelineLifecycle::AdvanceToValidated(ctx);

    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToRegistered(ctx, ""));
    DOMINUS_EXPECT(ctx.state == PipelineState::kValidated);
    DOMINUS_EXPECT(PipelineLifecycle::AdvanceToRegistered(ctx, "abc123"));
    DOMINUS_EXPECT(ctx.state == PipelineState::kRegistered);
}

DOMINUS_TEST(PipelineLifecycle_Reject_CannotFireOnceRegisteredOrExecutable) {
    CompilationContext ctx;
    PipelineLifecycle::Declare(ctx, "brooklyn", {"a"});
    PipelineLifecycle::BeginCompiling(ctx);
    ctx.stages.push_back({"RIG", PipelineState::kValidated, "ok"});
    PipelineLifecycle::AdvanceToValidated(ctx);
    PipelineLifecycle::AdvanceToRegistered(ctx, "abc123");

    DOMINUS_EXPECT(!PipelineLifecycle::Reject(ctx, "too late"));
    DOMINUS_EXPECT(ctx.state == PipelineState::kRegistered);

    DOMINUS_EXPECT(PipelineLifecycle::AdvanceToExecutable(ctx, true));
    DOMINUS_EXPECT(!PipelineLifecycle::Reject(ctx, "still too late"));
    DOMINUS_EXPECT(ctx.state == PipelineState::kExecutable);
}

DOMINUS_TEST(PipelineLifecycle_AdvanceToExecutable_RequiresRealProof) {
    CompilationContext ctx;
    PipelineLifecycle::Declare(ctx, "brooklyn", {"a"});
    PipelineLifecycle::BeginCompiling(ctx);
    ctx.stages.push_back({"RIG", PipelineState::kValidated, "ok"});
    PipelineLifecycle::AdvanceToValidated(ctx);
    PipelineLifecycle::AdvanceToRegistered(ctx, "abc123");

    DOMINUS_EXPECT(!PipelineLifecycle::AdvanceToExecutable(ctx, false));
    DOMINUS_EXPECT(ctx.state == PipelineState::kRegistered);
    DOMINUS_EXPECT(PipelineLifecycle::AdvanceToExecutable(ctx, true));
    DOMINUS_EXPECT(ctx.state == PipelineState::kExecutable);
}
