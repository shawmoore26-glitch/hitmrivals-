// VALIDATION/BuildPipeline.cpp
#include "VALIDATION/BuildPipeline.h"

#include "VALIDATION/PackageValidator.h"

namespace dominus::validation {

BuildReport BuildPipeline::Run(const std::filesystem::path& projectDir) {
    BuildReport report;

    std::error_code ec;
    if (!std::filesystem::exists(projectDir, ec) || ec) {
        // No project directory at all -- report as zero files found,
        // same as an empty project. The CLI layer is responsible for
        // surfacing "directory doesn't exist" as its own error before
        // ever calling this.
        return report;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(projectDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".dominus") continue;

        BuildFileResult fileResult;
        fileResult.file_path = entry.path().string();

        auto validation = PackageValidator::ValidateFile(entry.path(), entry.path().parent_path());
        fileResult.passed = validation.Passed();
        fileResult.error_count = validation.ErrorCount();
        fileResult.warning_count = validation.WarningCount();
        if (!fileResult.passed) {
            for (auto& issue : validation.issues) {
                if (issue.severity == Severity::kError) {
                    fileResult.error_messages.push_back(issue.category + ": " + issue.message);
                }
            }
        }

        report.results.push_back(std::move(fileResult));
    }

    return report;
}

}  // namespace dominus::validation
