// VALIDATION/BuildPipeline.h
// The honest, scoped version of "The Dominus Pipeline"'s Layer 2
// (Validation) + Layer 9 (Build System: one button, BUILD FAILED if
// anything fails). Not new validation logic -- pure orchestration of
// `PackageValidator`, which already exists (Phase 7). Discovers every
// `.dominus` file under a project directory and validates each one,
// same as a developer running `validate-package` by hand against every
// file, just automated into one pass with a single pass/fail verdict.
//
// Deliberately does NOT implement the source document's Layer 3
// ("Compile everything into .domchar/.domworld/.domcombat binary
// formats, don't load JSON directly at runtime") -- that would mean
// switching the engine's actual runtime load path away from
// `DominusSerializer`, which is exactly the "should the Registry
// Prototype become the load path" decision this engine has flagged as
// open and unresolved multiple times already. A build pipeline
// shouldn't quietly make that decision as a side effect.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dominus::validation {

struct BuildFileResult {
    std::string file_path;
    bool passed = false;
    size_t error_count = 0;
    size_t warning_count = 0;
    std::vector<std::string> error_messages;  // only populated on failure, for the build log
};

struct BuildReport {
    std::vector<BuildFileResult> results;

    bool AllPassed() const {
        for (auto& r : results) {
            if (!r.passed) return false;
        }
        return true;
    }

    size_t FailedCount() const {
        size_t n = 0;
        for (auto& r : results) {
            if (!r.passed) ++n;
        }
        return n;
    }
};

class BuildPipeline {
public:
    // Recursively discovers every *.dominus file under `projectDir` and
    // runs PackageValidator::ValidateFile against each, using the file's
    // own parent directory as the ref-resolution base (matching how
    // every other tool in this engine resolves refs). An empty project
    // (zero .dominus files found) reports AllPassed() == true -- there
    // is nothing to fail.
    static BuildReport Run(const std::filesystem::path& projectDir);
};

}  // namespace dominus::validation
