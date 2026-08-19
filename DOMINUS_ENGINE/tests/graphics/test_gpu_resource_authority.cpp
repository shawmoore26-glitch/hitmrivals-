// tests/graphics/test_gpu_resource_authority.cpp
// GPU Material Resource phase: GPUResourceAuthority is the one owner
// of the identity -> lifetime mapping. Pure C++, no Vulkan -- proving
// the ownership/idempotence contract before any real GPU resource is
// built against it.
#include "GRAPHICS/Renderer/GPUResourceAuthority.h"
#include "tests/TestFramework.h"

using dominus::graphics::GPUResourceAuthority;
using dominus::graphics::GPUResourceIdentity;
using dominus::graphics::GPUResourceLifetimeState;
using dominus::graphics::GPUResourceType;

DOMINUS_TEST(GPUResourceAuthority_AcquireOrCreate_IsIdempotent_SameIdentityReturnsSameEntry) {
    GPUResourceAuthority authority;
    GPUResourceIdentity identity{GPUResourceType::kMaterialColor, "hash_a"};

    auto& first = authority.AcquireOrCreate(identity);
    first.MarkReady();
    auto& second = authority.AcquireOrCreate(identity);

    // The REAL, structural proof of "exactly one owner": mutating
    // through one reference is visible through the other, because
    // they are the SAME entry, not two independent ones.
    DOMINUS_EXPECT(second.State() == GPUResourceLifetimeState::kReady);
    DOMINUS_EXPECT(authority.Count() == 1);
}

DOMINUS_TEST(GPUResourceAuthority_DifferentIdentities_ProduceDifferentEntries) {
    GPUResourceAuthority authority;
    GPUResourceIdentity identityA{GPUResourceType::kMaterialColor, "hash_a"};
    GPUResourceIdentity identityB{GPUResourceType::kMaterialColor, "hash_b"};

    authority.AcquireOrCreate(identityA);
    authority.AcquireOrCreate(identityB);
    DOMINUS_EXPECT(authority.Count() == 2);
}

DOMINUS_TEST(GPUResourceAuthority_Find_ReturnsNullptrForUnknownIdentity) {
    GPUResourceAuthority authority;
    GPUResourceIdentity known{GPUResourceType::kMaterialColor, "hash_known"};
    GPUResourceIdentity unknown{GPUResourceType::kMaterialColor, "hash_unknown"};
    authority.AcquireOrCreate(known);

    DOMINUS_EXPECT(authority.Find(known) != nullptr);
    DOMINUS_EXPECT(authority.Find(unknown) == nullptr);
}

DOMINUS_TEST(GPUResourceAuthority_AllResourcesDestroyed_TrueWhenEmpty) {
    GPUResourceAuthority authority;
    DOMINUS_EXPECT(authority.AllResourcesDestroyed());
}

DOMINUS_TEST(GPUResourceAuthority_AllResourcesDestroyed_FalseWhileAnyResourceIsLive) {
    GPUResourceAuthority authority;
    GPUResourceIdentity identity{GPUResourceType::kMaterialColor, "hash_live"};
    auto& lifetime = authority.AcquireOrCreate(identity);
    lifetime.MarkReady();

    DOMINUS_EXPECT(!authority.AllResourcesDestroyed());

    lifetime.MarkDestroyed(/*deviceIdleConfirmed=*/true);
    DOMINUS_EXPECT(authority.AllResourcesDestroyed());
}

DOMINUS_TEST(GPUResourceAuthority_AllResourcesDestroyed_FalseIfAnySingleResourceRemainsLive) {
    GPUResourceAuthority authority;
    GPUResourceIdentity identityA{GPUResourceType::kMaterialColor, "hash_a2"};
    GPUResourceIdentity identityB{GPUResourceType::kMaterialColor, "hash_b2"};
    auto& lifetimeA = authority.AcquireOrCreate(identityA);
    auto& lifetimeB = authority.AcquireOrCreate(identityB);
    lifetimeA.MarkReady();
    lifetimeB.MarkReady();
    lifetimeA.MarkDestroyed(true);

    // A destroyed, B still live -- the check must catch the ONE
    // remaining live resource, not just "at least one was destroyed".
    DOMINUS_EXPECT(!authority.AllResourcesDestroyed());
    lifetimeB.MarkDestroyed(true);
    DOMINUS_EXPECT(authority.AllResourcesDestroyed());
}
