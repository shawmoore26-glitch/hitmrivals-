// REALITY/RealityCompiler.h
// The god-tier move, scoped honestly: one governed execution path for
// Brooklyn -- AUTHOR -> COMPILE -> VALIDATE -> REGISTER -> EXECUTE ->
// RECONSTRUCT -> REPRODUCE -- that coordinates RIG and VISUALFORGE
// without duplicating either one's authority.
//
// What "without duplicating authority" means in this file, concretely:
// CompileBrooklyn() calls RIG::CharacterAcceptanceHarness and
// VISUALFORGE::RendererPackageAcceptanceHarness -- the exact same
// entry points TOOLS/Editor/dominus_cli.cpp's `brooklyn-acceptance`
// and `visual-acceptance` commands already call -- and cites their
// resulting AcceptanceCertificates. It never re-implements a skeleton
// check, a collision check, or a render check itself. RIG and
// VISUALFORGE stay the authoritative source for "is Brooklyn's rig/
// combat/visual data actually correct"; REALITY only answers "did
// every domain that must agree actually agree, and can I prove it
// twice."
//
// Scope, stated plainly: this is Milestone 1 from the directive --
// "one end-to-end subject: Brooklyn. Don't generalize prematurely."
//
// Milestone 3 (Dependency Sovereignty / Impact Graph) originally built
// its own graph here (REALITY/DependencyGraph.h). As of this phase,
// RealityCompiler uses EvidenceGraph (Milestone 7) instead -- the same
// authority RealityRebuilder already migrated onto -- reusing
// BrooklynEvidenceGraphBuilder rather than maintaining a second graph
// abstraction. Still explicitly NOT built: automatic selective
// recompilation ("RECOMPILE ONLY WHAT IS REQUIRED" from the
// directive's own diagram) -- impact analysis can tell you what a
// change would affect; nothing here yet acts on that answer (that's
// RealityRebuilder's job). Generalizing past Brooklyn remains real,
// separate, future work.
#pragma once

#include <filesystem>
#include <string>

#include "REALITY/CompilationContext.h"
#include "REALITY/EvidenceGraph.h"
#include "REALITY/ExecutionCertificate.h"
#include "RIG/AcceptanceCertificate.h"
#include "VISUALFORGE/AcceptanceCertificate.h"

namespace dominus::reality {

struct RealityCompilationResult {
    CompilationContext context;
    rig::AcceptanceCertificate rig_certificate;
    visualforge::AcceptanceCertificate visualforge_certificate;
    ExecutionCertificate execution_certificate;
    EvidenceGraph dependency_graph;

    // True iff both domain-level checks actually ran (their
    // certificates have non-empty section lists). False when, e.g.,
    // Brooklyn's .dominus failed to load or had no bound VisualGenome
    // -- REALITY refuses in that case rather than reporting a
    // misleadingly "empty pass".
    bool ran = false;
};

class RealityCompiler {
public:
    // Runs the full pipeline for Brooklyn against real fixtures in
    // `fixtureDir` (expects the same brooklyn_*.json / *.dominus files
    // tests/fixtures already contains). Every stage transition on the
    // returned context is real: COMPILING only after RIG/VisualForge
    // sections have actually been computed, VALIDATED only if every
    // stage validated, REGISTERED only with a real artifact_hash,
    // EXECUTABLE only if the underlying RigProfile actually reached
    // ACTIVE through its own, unmodified lifecycle gate.
    static RealityCompilationResult CompileBrooklyn(const std::filesystem::path& fixtureDir);

    // RECONSTRUCT -> REPRODUCE: runs CompileBrooklyn() TWICE, from
    // completely independent loads (no shared state, no memoization),
    // and requires an identical artifact_hash both times. This is the
    // directive's own "SOURCE -> COMPILE -> ARTIFACT A -> DESTROY
    // BUILD -> RECONSTRUCT -> ARTIFACT B -> A == B" test, executed for
    // real rather than asserted. Returns true iff both runs produced a
    // non-empty, identical artifact_hash; the two hashes are written
    // to outHashA/outHashB regardless of outcome, for callers that
    // want to report the actual values.
    static bool ReproduceAndVerify(const std::filesystem::path& fixtureDir, std::string& outHashA,
                                    std::string& outHashB);
};

}  // namespace dominus::reality
