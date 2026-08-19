// REALITY/BrooklynDomainCompilers.h
// Internal, shared real-compiler entry points for Brooklyn -- factored
// out of RealityCompiler.cpp so Milestone 4's RealityRebuilder can
// invoke the exact same real RIG/VisualForge compilation calls
// RealityCompiler::CompileBrooklyn already uses, instead of a second,
// possibly-diverging copy. This is a pure extraction: CompileRig and
// CompileVisualForge's logic and output are unchanged, verified by
// the full pre-existing REALITY test suite passing unmodified after
// the move. REALITY/EvidenceGraph.h and the graph-construction code
// in RealityCompiler.cpp are not touched by this file at all (the old
// REALITY/DependencyGraph.h this comment originally referenced has
// since been fully retired -- see REALITY/README.md's dependency
// graph migration record).
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "RIG/AcceptanceCertificate.h"
#include "VISUALFORGE/AcceptanceCertificate.h"
#include "VISUALFORGE/DependencyGraph.h"

namespace dominus::reality::internal {

struct VisualForgeCompileResult {
    visualforge::AcceptanceCertificate certificate;
    visualforge::DependencyGraph dependencies;  // the real hashes VISUALFORGE::DependencyGraphForge computed
};

// Domain 1: RIG. The exact same five checks `dominus-cli
// brooklyn-acceptance` runs -- CharacterAcceptanceHarness is the
// authority; this function only calls it and forges the certificate.
rig::AcceptanceCertificate CompileRig(const std::filesystem::path& dir);

// Domain 2: VISUALFORGE. The exact same seven checks `dominus-cli
// visual-acceptance` runs -- RendererPackageAcceptanceHarness is the
// authority. Returns std::nullopt on a real load/bind failure, never a
// fabricated empty pass.
std::optional<VisualForgeCompileResult> CompileVisualForge(const std::filesystem::path& dominusPath);

const rig::AcceptanceSection* FindRigSection(const rig::AcceptanceCertificate& cert, const std::string& name);

// Sha256(entity, compiler_identity, rig_certificate_hash,
// visualforge_certificate_hash) -- the one formula a full compile and
// a selective rebuild must both produce identically for the same
// inputs. Moved here so there is exactly one place this string format
// is defined.
std::string ComputeRealityArtifactHash(const std::string& entity, const std::string& compilerIdentity,
                                        const std::string& rigCertificateHash,
                                        const std::string& visualforgeCertificateHash);

}  // namespace dominus::reality::internal
