// GRAPHICS/Renderer/GPUResourceLifetime.h
// GPU Resource Lifetime -- Phase 3: the resource lifetime CONTRACT,
// proven as pure, testable data/logic. No VkImage, no VkSampler, no
// descriptor cache, no real GPU resource anywhere in this file --
// confirmed by its own lack of any Vulkan include.
//
// The one gap investigation found (see GRAPHICS/README.md's Phase 3
// record for the full investigation): DOMINUS's existing GPU resource
// destruction contract -- "call vkDeviceWaitIdle (or, mid-render,
// vkWaitForFences) before any destroy/recreate call" -- is real and
// already followed at every actual destroy/recreate call site in
// VulkanFrameRenderer (Shutdown(), ensureOffscreenTarget()'s
// recreation path). But it is enforced only by manual code discipline
// -- there is no explicit, checkable guard that would catch a FUTURE
// call site that destroys a resource without first satisfying that
// contract. This type makes the rule explicit and testable,
// independent of any real Vulkan call -- it does not wrap, replace,
// or add to the existing vkDeviceWaitIdle/vkWaitForFences mechanisms,
// which remain the real synchronization primitives.
//
// States derived from DOMINUS's ACTUAL renderer, not a generic
// example: VulkanFrameRenderer is fully synchronous per call (proven
// in the GPU Frame Lifecycle milestone -- RenderOffscreen never
// returns until vkWaitForFences confirms completion), so the CPU
// never observes an "in flight" resource as a state distinct from
// "ready" -- by the time control returns to any caller, a resource is
// either fully idle-and-usable or being torn down. A RETIRE_PENDING-
// style intermediate state would only become real and necessary if
// DOMINUS ever introduces overlapping/asynchronous GPU work (multiple
// frames in flight) -- confirmed absent today. Three real states are
// what the evidence supports:
//
//   UNCREATED -> READY -> DESTROYED
//
// with the one real, load-bearing rule: READY -> DESTROYED is legal
// only when the caller has confirmed the real device-idle (or
// equivalent full-completion) boundary has been crossed.
#pragma once

#include <string>

namespace dominus::graphics {

// Only one real, currently-relevant resource type exists to name --
// the deterministic color a registered MaterialGenome resolves to
// (GRAPHICS::MaterialContract, Phase 2). Extend this enum only when a
// real, evidenced need exists (e.g. a real texture system) -- not
// speculatively.
enum class GPUResourceType { kMaterialColor };

// Resource type + source identity + relevant configuration (Section 4
// of the investigation). Source identity reuses MaterialContract's
// real, existing artifact hash directly -- never a pointer, never an
// allocation address, never an invented UUID. "Relevant configuration"
// has no real content yet (no texture format/resolution exists to
// vary) -- honestly omitted rather than padded with a placeholder
// field nothing would consume.
struct GPUResourceIdentity {
    GPUResourceType type;
    std::string source_hash;

    bool operator==(const GPUResourceIdentity& other) const {
        return type == other.type && source_hash == other.source_hash;
    }
};

enum class GPUResourceLifetimeState { kUncreated, kReady, kDestroyed };

// A pure, in-memory state machine -- no GPU resource behind it. Proves
// the destruction contract is enforceable in code, not just true by
// convention.
class GPUResourceLifetime {
public:
    explicit GPUResourceLifetime(GPUResourceIdentity identity) : identity_(std::move(identity)) {}

    const GPUResourceIdentity& Identity() const { return identity_; }
    GPUResourceLifetimeState State() const { return state_; }

    // UNCREATED -> READY only. Returns false (refuses) for any other
    // starting state -- a resource cannot become "ready" twice, and
    // cannot skip past DESTROYED back to READY.
    bool MarkReady() {
        if (state_ != GPUResourceLifetimeState::kUncreated) return false;
        state_ = GPUResourceLifetimeState::kReady;
        return true;
    }

    // READY -> DESTROYED, legal only when deviceIdleConfirmed is true
    // -- the real contract every existing destroy/recreate call site
    // in VulkanFrameRenderer already follows. Also refuses double
    // destruction (state_ == kDestroyed) and destruction of a resource
    // that was never marked ready (state_ == kUncreated) -- both real,
    // distinct illegal transitions, not just one collapsed check.
    bool MarkDestroyed(bool deviceIdleConfirmed) {
        if (state_ != GPUResourceLifetimeState::kReady) return false;
        if (!deviceIdleConfirmed) return false;
        state_ = GPUResourceLifetimeState::kDestroyed;
        return true;
    }

private:
    GPUResourceIdentity identity_;
    GPUResourceLifetimeState state_ = GPUResourceLifetimeState::kUncreated;
};

}  // namespace dominus::graphics
