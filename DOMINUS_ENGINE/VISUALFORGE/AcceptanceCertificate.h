// VISUALFORGE/AcceptanceCertificate.h
// The same real, generated (never hand-typed) certificate discipline
// RIG's CharacterAcceptanceHarness proved for Brooklyn -- applied to
// RendererPackage. `structurally_sound` and `renderable` are tracked
// as TWO SEPARATE booleans, on purpose: "the data is internally
// consistent and deterministic" (structurally_sound) and "the data
// compiles into a real, deterministic Frame" (renderable) are
// different, real claims, and neither one is "looks correct on
// screen." There is still no rasterizer anywhere in this engine --
// RenderedOutput stays NOT_DECLARED and is never counted toward
// either boolean. This is why this certificate's terminal states are
// deliberately named differently from RIG's ACTIVE: nothing here
// claims behavioral/visual equivalence to a real running system,
// because nothing renders pixels yet.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "REGISTRY/Hash/Sha256.h"

namespace dominus::visualforge {

struct AcceptanceSection {
    std::string name;    // "BlueprintValidity" | "DependencyIntegrity" | "PackageDeterminism" |
                          // "ProvenanceCompleteness" | "RenderedOutput" | "Renderable" | "RenderDeterminism"
    bool passed = false;
    std::string detail;
};

struct AcceptanceCertificate {
    std::string entity_id;
    std::string package_hash;  // the RendererPackage this certificate is about
    std::vector<AcceptanceSection> sections;

    // Structural claim only: BlueprintValidity/DependencyIntegrity/
    // PackageDeterminism/ProvenanceCompleteness. Says nothing about
    // whether the data can be rendered.
    bool structurally_sound = false;

    // Rendering claim only: Renderable/RenderDeterminism. Says nothing
    // about visual correctness -- there is no rasterizer, so nothing
    // here has ever seen a pixel.
    bool renderable = false;

    std::string certificate_hash;
};

class VisualAcceptanceCertificateForge {
public:
    static AcceptanceCertificate Generate(const std::string& entityId, const std::string& packageHash,
                                           std::vector<AcceptanceSection> sections) {
        AcceptanceCertificate cert;
        cert.entity_id = entityId;
        cert.package_hash = packageHash;
        cert.sections = std::move(sections);

        static const std::vector<std::string> kStructuralNames = {"BlueprintValidity", "DependencyIntegrity",
                                                                    "PackageDeterminism", "ProvenanceCompleteness"};
        static const std::vector<std::string> kRenderNames = {"Renderable", "RenderDeterminism"};

        cert.structurally_sound = ComputeRollup(cert.sections, kStructuralNames);
        cert.renderable = ComputeRollup(cert.sections, kRenderNames);

        std::ostringstream out;
        out << "entity_id=" << cert.entity_id << ";package_hash=" << cert.package_hash
            << ";structurally_sound=" << (cert.structurally_sound ? "true" : "false")
            << ";renderable=" << (cert.renderable ? "true" : "false");
        for (const auto& s : cert.sections) {
            out << ";[name=" << s.name << ";passed=" << (s.passed ? "true" : "false") << ";detail=" << s.detail
                << "]";
        }
        cert.certificate_hash = registry::Sha256::Hash(out.str());
        return cert;
    }

private:
    // True iff every section whose name is in `relevantNames` is
    // present AND passed. A rollup with zero matching sections present
    // is false (nothing to claim), not vacuously true.
    static bool ComputeRollup(const std::vector<AcceptanceSection>& sections,
                               const std::vector<std::string>& relevantNames) {
        int found = 0;
        for (const auto& name : relevantNames) {
            for (const auto& s : sections) {
                if (s.name != name) continue;
                found++;
                if (!s.passed) return false;
            }
        }
        return found == static_cast<int>(relevantNames.size());
    }
};

}  // namespace dominus::visualforge
