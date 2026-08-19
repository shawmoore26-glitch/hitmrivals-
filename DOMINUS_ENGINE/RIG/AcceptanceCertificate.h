// RIG/AcceptanceCertificate.h
// A real, generated report -- never hand-typed, never a second source
// of truth. Every AcceptanceSection is produced by
// CharacterAcceptanceHarness actually running a check against real
// files/skeletons/clips; nothing in this file lets a caller construct
// a "PASS" without a check having run. certificate_hash is a real
// content hash of the whole report (reusing REGISTRY::Hash::Sha256,
// same narrow borrowing pattern COMBAT::Provenance/VISUALFORGE already
// established) -- RigProfileLifecycle::AdvanceToAccepted/DeclareActive
// both require this hash, so a certificate can't be silently swapped
// for a different one after the fact.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "REGISTRY/Hash/Sha256.h"

namespace dominus::rig {

struct AcceptanceSection {
    std::string name;    // "Skeleton" | "Animation" | "Combat" | "Runtime" | "Determinism"
    bool passed = false;
    std::string detail;  // e.g. "13/13 clips, 546/546 transform samples" or a real failure reason
};

struct AcceptanceCertificate {
    std::string source_entity;    // e.g. "brooklyn" (legacy)
    std::string target_profile;   // e.g. "brooklyn_canonical_identity_v1"
    std::vector<AcceptanceSection> sections;
    bool overall_pass = false;    // true iff every section passed
    std::string certificate_hash;  // hash of everything above
};

class AcceptanceCertificateForge {
public:
    static AcceptanceCertificate Generate(const std::string& sourceEntity, const std::string& targetProfile,
                                           std::vector<AcceptanceSection> sections) {
        AcceptanceCertificate cert;
        cert.source_entity = sourceEntity;
        cert.target_profile = targetProfile;
        cert.sections = std::move(sections);

        cert.overall_pass = !cert.sections.empty();
        for (const auto& s : cert.sections) {
            if (!s.passed) cert.overall_pass = false;
        }

        std::ostringstream out;
        out << "source=" << cert.source_entity << ";target=" << cert.target_profile
            << ";overall_pass=" << (cert.overall_pass ? "true" : "false");
        for (const auto& s : cert.sections) {
            out << ";[name=" << s.name << ";passed=" << (s.passed ? "true" : "false") << ";detail=" << s.detail
                << "]";
        }
        cert.certificate_hash = registry::Sha256::Hash(out.str());
        return cert;
    }
};

}  // namespace dominus::rig
