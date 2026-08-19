// REALITY/Reconciler.cpp
#include "REALITY/Reconciler.h"

#include "REALITY/BrooklynEvidenceGraphBuilder.h"

namespace dominus::reality {

namespace {

std::vector<RawFileEvent> SynthesizeFullScanEvents(const std::filesystem::path& fixtureDir) {
    // The SAME real, evidence-derived discovery Milestone 7/10 already
    // built -- reused, not reimplemented. Reconciliation's only
    // "logic" is turning that discovery into a synthetic event batch.
    auto artifactFiles = BrooklynEvidenceGraphBuilder::DiscoverArtifactFiles(fixtureDir);

    std::vector<RawFileEvent> events;
    for (const auto& [nodeId, filenames] : artifactFiles) {
        for (const auto& filename : filenames) {
            // kUnknown, deliberately: a reconciliation scan has no
            // real information about HOW a file changed while nobody
            // was watching (was it created, modified, moved?) -- only
            // that its current state should be checked. Downstream
            // verification never branches on kind anyway (Milestone 10
            // proved this explicitly), so this is an honest
            // "I don't know" rather than an invented claim.
            events.push_back({fixtureDir / filename, RawEventKind::kUnknown});
        }
    }
    return events;
}

}  // namespace

NormalizedChangeSet Reconciler::Scan(const std::filesystem::path& fixtureDir,
                                      const std::filesystem::path& registryPath) {
    return ChangeEventNormalizer::Normalize(fixtureDir, registryPath, SynthesizeFullScanEvents(fixtureDir));
}

std::vector<RebuildReport> Reconciler::Reconcile(const std::filesystem::path& fixtureDir,
                                                  const std::filesystem::path& registryPath,
                                                  std::chrono::milliseconds lockTimeout) {
    return ChangeEventNormalizer::ProcessEvents(fixtureDir, registryPath, SynthesizeFullScanEvents(fixtureDir),
                                                 lockTimeout);
}

}  // namespace dominus::reality
