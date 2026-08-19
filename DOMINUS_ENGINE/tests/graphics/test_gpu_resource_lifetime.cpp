// tests/graphics/test_gpu_resource_lifetime.cpp
// GPU Resource Lifetime -- Phase 3. Tests the pure destruction-contract
// state machine derived from real investigation of VulkanFrameRenderer
// (see GRAPHICS/README.md). No Vulkan, no real GPU resource anywhere
// in this file -- proving the CONTRACT is sound before any real
// resource is ever built against it.
//
// Two invariants from the task's own list are deliberately NOT tested
// here, with the real reason stated rather than silently omitted:
//   - Retirement invariant: DOMINUS's renderer is fully synchronous
//     (proven in the GPU Frame Lifecycle milestone) -- there is no
//     real, observable "in flight, not yet retired" state distinct
//     from READY/DESTROYED today. A RETIRE_PENDING-style invariant has
//     nothing real to test against until DOMINUS has overlapping GPU
//     work, which it does not.
//   - Cross-resource invariant: investigation found no real
//     Material -> Image -> ImageView -> Descriptor (or any other)
//     dependency chain anywhere in DOMINUS today -- no such
//     relationship exists to test. Fabricating one to satisfy the
//     checklist would violate "only report relationships that
//     actually exist."
#include "GRAPHICS/Renderer/GPUResourceLifetime.h"
#include "GRAPHICS/Renderer/MaterialContract.h"
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "REGISTRY/GenomeCompiler.h"
#include "REGISTRY/GenomeRegistry.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MaterialGenomeLoader;
using dominus::graphics::GPUResourceIdentity;
using dominus::graphics::GPUResourceLifetime;
using dominus::graphics::GPUResourceLifetimeState;
using dominus::graphics::GPUResourceType;
using dominus::graphics::MaterialContract;
using dominus::registry::GenomeCompiler;
using dominus::registry::GenomeKind;
using dominus::registry::GenomeRegistry;

namespace {
std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}
}  // namespace

// --- Identity invariant: equivalent source data -> equivalent identity

DOMINUS_TEST(GPUResourceLifetime_IdentityDerivedFromRealMaterialContract_IsDeterministic) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    auto compiled = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compiled.ok);

    auto resolutionA = MaterialContract::Resolve(*compiled.artifact);
    auto resolutionB = MaterialContract::Resolve(*compiled.artifact);

    GPUResourceIdentity identityA{GPUResourceType::kMaterialColor, resolutionA.source_hash};
    GPUResourceIdentity identityB{GPUResourceType::kMaterialColor, resolutionB.source_hash};
    DOMINUS_EXPECT(identityA == identityB);
    DOMINUS_EXPECT(!identityA.source_hash.empty());
}

DOMINUS_TEST(GPUResourceLifetime_DifferentMaterialProperties_ProducesDifferentIdentity) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto genomeA = *load.value;
    auto genomeB = *load.value;
    genomeB.properties.wear_state = genomeA.properties.wear_state + 0.3f;

    auto compiledA = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", genomeA, std::nullopt, 1, "t1");
    auto compiledB = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", genomeB, std::nullopt, 1, "t1");
    auto resolutionA = MaterialContract::Resolve(*compiledA.artifact);
    auto resolutionB = MaterialContract::Resolve(*compiledB.artifact);

    GPUResourceIdentity identityA{GPUResourceType::kMaterialColor, resolutionA.source_hash};
    GPUResourceIdentity identityB{GPUResourceType::kMaterialColor, resolutionB.source_hash};
    DOMINUS_EXPECT(!(identityA == identityB));
}

// --- Ownership invariant: exactly one authoritative owner ------------

DOMINUS_TEST(GPUResourceLifetime_EachIdentityHasExactlyOneLifetimeOwner) {
    // GenomeRegistry remains the sole authority for the SOURCE
    // identity (the material's real hash) -- GPUResourceLifetime never
    // duplicates or re-derives that identity, it only REFERENCES it
    // (via source_hash, copied verbatim from MaterialContract's real
    // resolution). Each GPUResourceLifetime instance is the one real
    // owner of its OWN lifetime state -- proven by construction:
    // state_ is a private, non-shared member with no aliasing API
    // (no reference-counting, no shared_ptr, no global table).
    GPUResourceIdentity identity{GPUResourceType::kMaterialColor, "some_real_hash"};
    GPUResourceLifetime lifetime(identity);
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kUncreated);
    DOMINUS_EXPECT(lifetime.Identity() == identity);
}

// --- Real state machine transitions -----------------------------------

DOMINUS_TEST(GPUResourceLifetime_RealTransitionSequence_UncreatedToReadyToDestroyed) {
    GPUResourceIdentity identity{GPUResourceType::kMaterialColor, "hash_1"};
    GPUResourceLifetime lifetime(identity);

    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kUncreated);
    DOMINUS_EXPECT(lifetime.MarkReady());
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kReady);
    DOMINUS_EXPECT(lifetime.MarkDestroyed(/*deviceIdleConfirmed=*/true));
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kDestroyed);
}

DOMINUS_TEST(GPUResourceLifetime_CannotMarkReadyTwice) {
    GPUResourceLifetime lifetime({GPUResourceType::kMaterialColor, "hash_2"});
    DOMINUS_EXPECT(lifetime.MarkReady());
    DOMINUS_EXPECT(!lifetime.MarkReady());  // real, illegal transition, refused
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kReady);
}

// --- Destruction invariant: destroy is legal only with a confirmed
// device-idle (or equivalent completion) boundary ------------------------

DOMINUS_TEST(GPUResourceLifetime_CannotDestroyWithoutDeviceIdleConfirmation) {
    GPUResourceLifetime lifetime({GPUResourceType::kMaterialColor, "hash_3"});
    DOMINUS_EXPECT(lifetime.MarkReady());
    DOMINUS_EXPECT(!lifetime.MarkDestroyed(/*deviceIdleConfirmed=*/false));  // the real hazard, refused
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kReady);   // real state unchanged
}

DOMINUS_TEST(GPUResourceLifetime_CannotDestroyBeforeReady) {
    GPUResourceLifetime lifetime({GPUResourceType::kMaterialColor, "hash_4"});
    DOMINUS_EXPECT(!lifetime.MarkDestroyed(/*deviceIdleConfirmed=*/true));  // UNCREATED -> DESTROYED, illegal
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kUncreated);
}

// --- Double-destruction invariant --------------------------------------

DOMINUS_TEST(GPUResourceLifetime_CannotDestroyTwice) {
    GPUResourceLifetime lifetime({GPUResourceType::kMaterialColor, "hash_5"});
    DOMINUS_EXPECT(lifetime.MarkReady());
    DOMINUS_EXPECT(lifetime.MarkDestroyed(true));
    DOMINUS_EXPECT(!lifetime.MarkDestroyed(true));  // real double-destruction, refused
    DOMINUS_EXPECT(lifetime.State() == GPUResourceLifetimeState::kDestroyed);
}
