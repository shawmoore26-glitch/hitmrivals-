// VALIDATION/PackageValidator.h
// Phase 7 (Validator), scoped honestly against what the engine actually
// stores. The original vision names ~20 identity fields (Species, Age,
// Biography, Voice, Alignment, Height, Weight, Victory Quotes, ...) --
// only display_name, faction, and the combat genome fields
// (style/range/pressure/counter/mobility/risk) exist in the schema
// today. This validator checks what's real; it does not fabricate checks
// against fields the engine doesn't store (LAW C014: no fake systems
// applies to validators too -- a check that can't fail because the data
// it inspects doesn't exist is worse than no check).
//
// Sits at the top of the dependency stack by necessity: it inspects
// CORE ref components AND the bound CHARACTER/COMBAT/ANIMATION
// components together, so it depends on all of them. Nothing else in
// the engine depends on VALIDATION -- this is the terminal consumer, not
// a substrate anything else builds on.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CORE/MetaBin/MetaBinObject.h"

namespace dominus::validation {

enum class Severity { kError, kWarning, kInfo };

struct ValidationIssue {
    Severity severity = Severity::kError;
    std::string category;  // "identity" | "asset_ownership" | "naming" | "integrity" | "motion_coverage"
    std::string message;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;

    // Warnings/info don't block a package -- only errors do. A package
    // with zero errors "passes" even with warnings, same convention as a
    // compiler treating warnings as non-fatal by default.
    bool Passed() const {
        for (auto& i : issues) {
            if (i.severity == Severity::kError) return false;
        }
        return true;
    }

    size_t ErrorCount() const { return CountWhere(Severity::kError); }
    size_t WarningCount() const { return CountWhere(Severity::kWarning); }

    void Add(Severity severity, std::string category, std::string message) {
        issues.push_back(ValidationIssue{severity, std::move(category), std::move(message)});
    }

private:
    size_t CountWhere(Severity s) const {
        size_t n = 0;
        for (auto& i : issues) {
            if (i.severity == s) ++n;
        }
        return n;
    }
};

class PackageValidator {
public:
    // The full pipeline: loads `dominusPath` twice (deterministic-rebuild
    // check), binds RigBinder + CombatBinder on one instance, and runs
    // every check below against the result. Binding failures are reported
    // as errors, not thrown -- a broken package should produce a report,
    // not crash the validator.
    static ValidationReport ValidateFile(const std::filesystem::path& dominusPath, const std::filesystem::path& baseDir);

    // Individual checks -- usable standalone against an already-loaded/
    // bound object, without a full file round trip. ValidateFile composes
    // these; tests exercise them both together and in isolation.
    static void CheckIdentityCompleteness(core::MetaBinObject& obj, ValidationReport& report);
    static void CheckAssetOwnership(core::MetaBinObject& obj, const std::filesystem::path& baseDir,
                                     ValidationReport& report);
    static void CheckNamingConsistency(core::MetaBinObject& obj, ValidationReport& report);
    static void CheckDeterministicRebuild(const std::filesystem::path& dominusPath, ValidationReport& report);
};

}  // namespace dominus::validation
