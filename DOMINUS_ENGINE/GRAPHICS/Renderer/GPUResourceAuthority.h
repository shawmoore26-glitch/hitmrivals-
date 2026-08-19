// GRAPHICS/Renderer/GPUResourceAuthority.h
// GPU Material Resource phase: the "exactly one owner per identity"
// invariant Phase 3's GPUResourceLifetime documented but never
// enforced across multiple instances. Pure C++, zero Vulkan includes,
// same discipline as GPUResourceLifetime.h -- it owns the mapping from
// identity to lifetime, nothing else.
//
// The real gap this closes: before this file, nothing stopped two
// different call sites from constructing two separate
// GPUResourceLifetime instances for the SAME GPUResourceIdentity --
// the "one owner" invariant was true only by not being violated, never
// enforced. AcquireOrCreate makes it structural: the first call for a
// given identity creates a real entry; every subsequent call for the
// same identity returns the SAME entry, mirroring
// GenomeRegistry::Register's idempotent, content-addressed pattern
// (same shape, not the same type -- GenomeRegistry's ImmutableArtifact
// is immutable content, GPUResourceLifetime is mutable lifecycle
// state; reusing GenomeRegistry literally would mean lying about
// mutability).
#pragma once

#include <string>
#include <unordered_map>

#include "GRAPHICS/Renderer/GPUResourceLifetime.h"

namespace dominus::graphics {

class GPUResourceAuthority {
public:
    // Idempotent: the first call for a given identity creates a real,
    // UNCREATED entry; every later call for the SAME identity returns
    // a reference to that SAME entry -- never a second one.
    GPUResourceLifetime& AcquireOrCreate(const GPUResourceIdentity& identity) {
        const std::string key = CompositeKey(identity);
        auto it = resources_.find(key);
        if (it != resources_.end()) return it->second;
        return resources_.emplace(key, GPUResourceLifetime(identity)).first->second;
    }

    // Real lookup, no creation -- nullptr if this identity was never
    // acquired.
    GPUResourceLifetime* Find(const GPUResourceIdentity& identity) {
        auto it = resources_.find(CompositeKey(identity));
        return it == resources_.end() ? nullptr : &it->second;
    }

    const GPUResourceLifetime* Find(const GPUResourceIdentity& identity) const {
        auto it = resources_.find(CompositeKey(identity));
        return it == resources_.end() ? nullptr : &it->second;
    }

    std::size_t Count() const { return resources_.size(); }

    // The real cross-resource question the GPU Resource Lifetime
    // investigation found evidence for (Phase 3): can a resource
    // outlive its authority? Answer: no owned resource may remain
    // non-DESTROYED when the authority itself is asked whether
    // teardown is safe -- checked explicitly here rather than only in
    // a destructor, so callers can act on it before real Vulkan
    // objects are involved.
    bool AllResourcesDestroyed() const {
        for (const auto& [key, lifetime] : resources_) {
            if (lifetime.State() != GPUResourceLifetimeState::kDestroyed) return false;
        }
        return true;
    }

    // Force-destroys every currently-tracked lifetime's STATE (not any
    // real GPU object -- the caller is responsible for having already
    // destroyed those). Used only at real renderer teardown, where the
    // caller has already confirmed device-idle and already destroyed
    // every real resource -- without this, a lifetime would keep
    // claiming kReady for a GPU object that no longer exists, a real
    // inconsistency between tracked state and reality. Silently skips
    // any lifetime already kDestroyed or still kUncreated -- this is a
    // real "make tracked state match reality" operation, not a
    // generic bypass of MarkDestroyed's own refusal rules.
    void ForceDestroyAllForRealTeardown(bool deviceIdleConfirmed) {
        for (auto& [key, lifetime] : resources_) {
            if (lifetime.State() == GPUResourceLifetimeState::kReady) {
                lifetime.MarkDestroyed(deviceIdleConfirmed);
            }
        }
    }

private:
    static std::string CompositeKey(const GPUResourceIdentity& identity) {
        std::string typeTag = (identity.type == GPUResourceType::kMaterialColor) ? "material_color:" : "unknown:";
        return typeTag + identity.source_hash;
    }

    std::unordered_map<std::string, GPUResourceLifetime> resources_;
};

}  // namespace dominus::graphics
