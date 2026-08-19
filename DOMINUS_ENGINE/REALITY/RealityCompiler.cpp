// REALITY/RealityCompiler.cpp
#include "REALITY/RealityCompiler.h"

#include <optional>

#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "REALITY/BrooklynDomainCompilers.h"
#include "REALITY/BrooklynEvidenceGraphBuilder.h"
#include "RIG/RigProfile.h"
#include "RIG/RigProfileValidator.h"

namespace dominus::reality {

namespace {

// Domain 1: RIG. See REALITY/BrooklynDomainCompilers.cpp -- reused via
// internal::CompileRig below (Milestone 1/2 material, not modified by
// Milestone 3/7's moves to shared graph infrastructure).

// Builds the real EvidenceGraph structure via BrooklynEvidenceGraphBuilder::
// BuildStructure (Milestone 7's own real discovery/edge logic, reused
// not reimplemented -- the exact same structure RealityRebuilder
// itself uses), then fills in whichever cert/artifact hashes
// CompileBrooklyn has ALREADY computed from its own real
// internal::CompileRig/CompileVisualForge calls. Never re-runs those
// real, expensive checks a second time just to populate a graph for
// display -- the entire point of BuildStructure's fast path is to
// avoid exactly that.
EvidenceGraph AssembleGraph(const std::filesystem::path& fixtureDir, const std::string& rigCertificateHash,
                             bool rigOverallPass, const std::string& visualforgeCertificateHash, bool visualPass,
                             const std::string& realityArtifactHash) {
    EvidenceGraph graph = BrooklynEvidenceGraphBuilder::BuildStructure(fixtureDir);
    for (auto& node : graph.nodes) {
        if (node.node_id == "brooklyn.rig_certificate") {
            node.artifact_hash = rigCertificateHash;
            node.state = rigOverallPass ? "PRESENT" : "MISSING";
        } else if (node.node_id == "brooklyn.visualforge_certificate") {
            node.artifact_hash = visualforgeCertificateHash;
            node.state = visualPass ? "PRESENT" : "MISSING";
        } else if (node.node_id == "brooklyn.reality_artifact") {
            node.artifact_hash = realityArtifactHash;
            node.state = realityArtifactHash.empty() ? "MISSING" : "PRESENT";
        }
    }
    return graph;
}

}  // namespace

RealityCompilationResult RealityCompiler::CompileBrooklyn(const std::filesystem::path& fixtureDir) {
    RealityCompilationResult result;
    CompilationContext& ctx = result.context;

    std::vector<std::string> sourceIdentities = {
        (fixtureDir / "brooklyn.skel.json").string(),
        (fixtureDir / "brooklyn_canonical.skel.json").string(),
        (fixtureDir / "brooklyn_canonical.dominus").string(),
        (fixtureDir / "brooklyn_canonical_hurtboxes.json").string(),
    };
    if (!PipelineLifecycle::Declare(ctx, "brooklyn", sourceIdentities)) return result;
    if (!PipelineLifecycle::BeginCompiling(ctx)) return result;

    // --- COMPILE: RIG ------------------------------------------------
    result.rig_certificate = internal::CompileRig(fixtureDir);
    ctx.rig_certificate_hash = result.rig_certificate.certificate_hash;
    ctx.stages.push_back(
        {"RIG", result.rig_certificate.overall_pass ? PipelineState::kValidated : PipelineState::kRejected,
         "certificate_hash=" + result.rig_certificate.certificate_hash.substr(0, 12) +
             " overall_pass=" + (result.rig_certificate.overall_pass ? "true" : "false")});

    // --- COMPILE: VISUALFORGE ------------------------------------------
    auto visualResult = internal::CompileVisualForge(fixtureDir / "brooklyn_canonical.dominus");
    bool visualPass = false;
    if (!visualResult.has_value()) {
        // A real, named refusal -- brooklyn_canonical.dominus (RIG's
        // actual migrated artifact) carries no visual_genome block, so
        // RendererPackageForge::Build refuses before any AcceptanceSection
        // can even run. Recorded as a genuine VISUALFORGE stage
        // rejection, not silently skipped -- `stages` always has one
        // entry per domain this compiler attempted.
        ctx.stages.push_back({"VISUALFORGE", PipelineState::kRejected,
                               "package build refused: brooklyn_canonical.dominus failed to load, or has no "
                               "bound VisualGenome (see REALITY/README.md)"});
        PipelineLifecycle::Reject(ctx, "VisualForge stage rejected: brooklyn_canonical.dominus has no bound "
                                        "VisualGenome -- RIG's migrated artifact and VisualForge's authored data "
                                        "have never been unified onto one object");
        result.ran = true;
        result.dependency_graph = AssembleGraph(fixtureDir, ctx.rig_certificate_hash,
                                                  result.rig_certificate.overall_pass, "", false, "");

        std::vector<ExecutionSection> sections = {
            {"Authority.RIG", result.rig_certificate.overall_pass, "see RIG certificate"},
            {"Authority.VisualForge", false, ctx.stages.back().detail},
        };
        result.execution_certificate = ExecutionCertificateForge::Generate(
            "brooklyn", ctx.compiler_identity, ctx.rig_certificate_hash, "", "", sections);
        return result;
    }
    result.visualforge_certificate = visualResult->certificate;
    ctx.visualforge_certificate_hash = result.visualforge_certificate.certificate_hash;
    visualPass = result.visualforge_certificate.structurally_sound && result.visualforge_certificate.renderable;
    ctx.stages.push_back({"VISUALFORGE", visualPass ? PipelineState::kValidated : PipelineState::kRejected,
                           "certificate_hash=" + result.visualforge_certificate.certificate_hash.substr(0, 12) +
                               " structurally_sound=" +
                               (result.visualforge_certificate.structurally_sound ? "true" : "false") +
                               " renderable=" + (result.visualforge_certificate.renderable ? "true" : "false")});

    result.ran = true;

    // --- VALIDATE ---------------------------------------------------------
    if (!PipelineLifecycle::AdvanceToValidated(ctx)) {
        std::string reason = "one or more domain stages did not validate:";
        for (const auto& s : ctx.stages) {
            if (s.state != PipelineState::kValidated) reason += " " + s.stage;
        }
        PipelineLifecycle::Reject(ctx, reason);
        result.dependency_graph =
            AssembleGraph(fixtureDir, ctx.rig_certificate_hash, result.rig_certificate.overall_pass,
                           ctx.visualforge_certificate_hash, visualPass, "");

        std::vector<ExecutionSection> sections = {
            {"Authority.RIG", result.rig_certificate.overall_pass, "see RIG certificate"},
            {"Authority.VisualForge", visualPass, "see VisualForge certificate"},
        };
        result.execution_certificate = ExecutionCertificateForge::Generate(
            "brooklyn", ctx.compiler_identity, ctx.rig_certificate_hash, ctx.visualforge_certificate_hash, "",
            sections);
        return result;
    }

    // --- REGISTER -----------------------------------------------------------
    // artifact_hash is derived ONLY from the two component certificate
    // hashes plus the entity/compiler identity -- "identity depends on
    // real dependency hashes, never a guess," the same discipline
    // VISUALFORGE::ProductionSnapshot already established for its own
    // version bumps.
    std::string artifactHash = internal::ComputeRealityArtifactHash(
        ctx.source_entity, ctx.compiler_identity, ctx.rig_certificate_hash, ctx.visualforge_certificate_hash);
    ctx.dependency_identities = {ctx.rig_certificate_hash, ctx.visualforge_certificate_hash};
    if (!PipelineLifecycle::AdvanceToRegistered(ctx, artifactHash)) return result;

    // --- EXECUTE -----------------------------------------------------------
    // The real execution proof: drive the actual RigProfileLifecycle
    // through ACCEPTED to ACTIVE, gated on the real RIG certificate --
    // identical to what `dominus-cli brooklyn-acceptance` already
    // proves, cited here rather than re-derived a second way.
    rig::RigProfile profile;
    profile.profile_id = "brooklyn_canonical_identity_v1";
    profile.entity_id = "brooklyn";
    auto canonicalSkel = animation::SkeletonLoader::LoadFromFile(fixtureDir / "brooklyn_canonical.skel.json");
    bool reachedActive = false;
    if (canonicalSkel.ok) {
        for (const auto& bone : canonicalSkel.value->Bones()) profile.mappings.push_back({bone.name, bone.name});
        rig::RigProfileLifecycle::AdvanceToMapped(profile);
        auto profileReport = rig::RigProfileValidator::Validate(*canonicalSkel.value, profile);
        rig::RigProfileLifecycle::AdvanceToValidated(profile, profileReport.valid);
        rig::RigProfileLifecycle::DeclareCompatible(profile, "REALITY pipeline: structural migration map verified");
        bool accepted = rig::RigProfileLifecycle::AdvanceToAccepted(profile, result.rig_certificate.overall_pass,
                                                                       result.rig_certificate.certificate_hash);
        if (accepted) {
            reachedActive = rig::RigProfileLifecycle::DeclareActive(
                profile, result.rig_certificate.certificate_hash,
                "REALITY pipeline run, certificate " + result.rig_certificate.certificate_hash.substr(0, 12));
        }
    }

    bool executionProofPassed = reachedActive && visualPass;
    PipelineLifecycle::AdvanceToExecutable(ctx, executionProofPassed);

    result.dependency_graph =
        AssembleGraph(fixtureDir, ctx.rig_certificate_hash, result.rig_certificate.overall_pass,
                       ctx.visualforge_certificate_hash, visualPass, ctx.artifact_hash);

    // --- ExecutionCertificate: cites RIG's own Runtime/Determinism
    // sections directly -- never recomputed. ------------------------------
    const rig::AcceptanceSection* runtimeSection = internal::FindRigSection(result.rig_certificate, "Runtime");
    const rig::AcceptanceSection* determinismSection = internal::FindRigSection(result.rig_certificate, "Determinism");

    std::vector<ExecutionSection> sections = {
        {"Authority.RIG", result.rig_certificate.overall_pass,
         "certificate_hash=" + result.rig_certificate.certificate_hash.substr(0, 12)},
        {"Authority.VisualForge", visualPass,
         "certificate_hash=" + result.visualforge_certificate.certificate_hash.substr(0, 12)},
        {"Execution.Runtime", runtimeSection != nullptr && runtimeSection->passed,
         runtimeSection ? runtimeSection->detail : "Runtime section missing from RIG certificate"},
        {"Execution.Determinism", determinismSection != nullptr && determinismSection->passed,
         determinismSection ? determinismSection->detail : "Determinism section missing from RIG certificate"},
        {"Registration", !ctx.artifact_hash.empty(), "artifact_hash=" + ctx.artifact_hash.substr(0, 12)},
        {"ExecutionProof.ProfileActive", reachedActive,
         reachedActive ? "RigProfileLifecycle reached ACTIVE through its own unmodified gate"
                        : "RigProfile did not reach ACTIVE -- see profile lifecycle for the real refusal point"},
    };
    result.execution_certificate = ExecutionCertificateForge::Generate(
        "brooklyn", ctx.compiler_identity, ctx.rig_certificate_hash, ctx.visualforge_certificate_hash,
        ctx.artifact_hash, sections);

    return result;
}

bool RealityCompiler::ReproduceAndVerify(const std::filesystem::path& fixtureDir, std::string& outHashA,
                                          std::string& outHashB) {
    // Two completely independent calls -- no shared state, no cached
    // certificates, fresh file loads both times. This is the
    // mechanism itself, not a comment promising determinism.
    auto runA = CompileBrooklyn(fixtureDir);
    auto runB = CompileBrooklyn(fixtureDir);

    outHashA = runA.context.artifact_hash;
    outHashB = runB.context.artifact_hash;

    if (outHashA.empty() || outHashB.empty()) return false;
    return outHashA == outHashB;
}

}  // namespace dominus::reality
