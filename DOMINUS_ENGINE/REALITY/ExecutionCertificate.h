// REALITY/ExecutionCertificate.h
// The DOMINUS Execution Certificate the directive asked for --
// Identity / Authority / Execution / Result, unified from evidence
// RIG and VISUALFORGE already generated, never recomputed. Same
// forge-from-real-sections discipline as RIG::AcceptanceCertificateForge
// and VISUALFORGE::VisualAcceptanceCertificateForge: nothing here lets
// a caller construct `executable=true` without every cited section
// having actually passed.
//
// Deliberately thin: this certificate does not re-run RIG's Runtime/
// Determinism checks or VisualForge's structural checks. It cites
// their own AcceptanceSection results by name and hash. Coordinating
// without duplicating authority means exactly this -- REALITY reports
// what RIG and VISUALFORGE already proved; it does not re-prove it a
// second, slightly-different way.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "REGISTRY/Hash/Sha256.h"

namespace dominus::reality {

struct ExecutionSection {
    std::string name;  // "Authority.RIG" | "Authority.VisualForge" | "Execution.Runtime" |
                        // "Execution.Determinism" | "Registration" | "Reconstruction"
    bool passed = false;
    std::string detail;
};

struct ExecutionCertificate {
    std::string source_entity;
    std::string compiler_identity;

    std::string rig_certificate_hash;
    std::string visualforge_certificate_hash;
    std::string artifact_hash;

    std::vector<ExecutionSection> sections;
    bool executable = false;  // true iff every section passed AND sections is non-empty
    std::string certificate_hash;
};

class ExecutionCertificateForge {
public:
    static ExecutionCertificate Generate(const std::string& sourceEntity, const std::string& compilerIdentity,
                                          const std::string& rigCertificateHash,
                                          const std::string& visualforgeCertificateHash,
                                          const std::string& artifactHash, std::vector<ExecutionSection> sections) {
        ExecutionCertificate cert;
        cert.source_entity = sourceEntity;
        cert.compiler_identity = compilerIdentity;
        cert.rig_certificate_hash = rigCertificateHash;
        cert.visualforge_certificate_hash = visualforgeCertificateHash;
        cert.artifact_hash = artifactHash;
        cert.sections = std::move(sections);

        cert.executable = !cert.sections.empty();
        for (const auto& s : cert.sections) {
            if (!s.passed) cert.executable = false;
        }

        std::ostringstream out;
        out << "source_entity=" << cert.source_entity << ";compiler_identity=" << cert.compiler_identity
            << ";rig_certificate_hash=" << cert.rig_certificate_hash
            << ";visualforge_certificate_hash=" << cert.visualforge_certificate_hash
            << ";artifact_hash=" << cert.artifact_hash << ";executable=" << (cert.executable ? "true" : "false");
        for (const auto& s : cert.sections) {
            out << ";[name=" << s.name << ";passed=" << (s.passed ? "true" : "false") << ";detail=" << s.detail
                << "]";
        }
        cert.certificate_hash = registry::Sha256::Hash(out.str());
        return cert;
    }
};

}  // namespace dominus::reality
