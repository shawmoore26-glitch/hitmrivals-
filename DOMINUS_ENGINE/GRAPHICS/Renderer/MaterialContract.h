// GRAPHICS/Renderer/MaterialContract.h
// Material Resource Authority -- Phase 2: the deterministic
// relationship between a REGISTERED MaterialGenome and what a (not yet
// built) GPU resource layer would need, proven as real, testable data
// -- no VkImage, no VkSampler, no descriptor cache, no texture
// streaming, and this file is never called by RasterDevice or
// VulkanFrameRenderer (confirmed: neither renderer includes this
// header). Wiring it into a renderer is real, separate, later work
// (Phase 4's own scope).
//
// Investigated directly before writing this, not assumed: does
// MaterialGenome (or its sibling genomes, VisualGenome/
// VisualStyleGenome) contain texture references, image/asset ids,
// sampler information, UV data, or channel mappings anywhere?
// Confirmed by reading every one of those files, and by a
// repository-wide search for "uv", "sampler", "channel_map",
// "texture_ref", "image_ref", "albedo", "normal_map", etc. -- zero
// matches anywhere in real DOMINUS source. Every one of those genomes
// is deliberately, consistently "state, not rendering" -- VisualGenome.h
// says so in its own header comment ("GRAPHICS remains an empty
// placeholder"). This is a real, repository-wide architectural
// choice, not an oversight specific to MaterialGenome.
//
// What MaterialGenome DOES have, confirmed real and reused here: a
// real, canonical, already-registered identity (material_id +
// MaterialProperties, hashed via MaterialGenomeCompiler and now
// authoritative through GenomeRegistry -- the immediately-prior
// milestone). This file's whole job is proving what THAT identity
// deterministically resolves to today, honestly -- not what it will
// resolve to once a texture system exists.
#pragma once

#include <cstdint>
#include <string>

#include "REGISTRY/ImmutableArtifact.h"

namespace dominus::graphics {

// Every field here is real and honestly scoped -- no field claims
// data DOMINUS doesn't have.
struct MaterialVisualResolution {
    // Always false today -- see this file's header comment for the
    // real, repository-wide investigation behind that. Explicit and
    // tested so "no texture" is a stated fact, not a silent absence a
    // caller has to infer.
    bool has_texture = false;

    // A real, deterministic color -- derived from the REGISTERED
    // artifact's FULL canonical hash (material_id AND every
    // MaterialProperties field: age_years, wear_state, damage_history,
    // weather_exposure), reusing GRAPHICS::ResolveMaterialColor and
    // the one real Sha256 authority this engine already trusts. This
    // is a real improvement over the renderer's current material_ref-
    // only color seed: two MaterialGenomes sharing a material_id but
    // differing in any OTHER real property now resolve to genuinely
    // different colors here, where today's renderer path would not.
    std::uint8_t r = 0, g = 0, b = 0;

    // True iff this resolution came from a real, registered
    // GenomeKind::kMaterial artifact. False means the real, named
    // fallback path (ResolveFallback) was used instead -- never
    // silently indistinguishable from a real resolution.
    bool from_registered_artifact = false;

    // The real artifact hash this resolution was derived from -- empty
    // for the fallback case. Lets a caller trace a resolution back to
    // its real, registered source, the same traceability
    // GenomeRegistry::Find already provides for the artifact itself.
    std::string source_hash;
};

class MaterialContract {
public:
    // Real, deterministic: the same registered artifact always
    // produces the identical MaterialVisualResolution. Real, honest
    // type safety: an artifact that is not GenomeKind::kMaterial (a
    // real misuse -- passing a CombatGenome artifact here) is never
    // silently misinterpreted as material data; it degrades to the
    // real, named fallback path instead, keyed by the artifact's own
    // entity id.
    static MaterialVisualResolution Resolve(const registry::ImmutableArtifact& artifact);

    // The real, named fallback -- for when no MaterialGenome is
    // registered or available at all. Takes a real identity seed
    // (e.g. an entity_id) so the fallback stays deterministic and
    // traceable, never arbitrary. This is the same behavior the
    // renderer already falls into today when material_ref is empty
    // (colorSeed = entity_id) -- named and tested here explicitly
    // rather than left as an implicit renderer-side accident.
    static MaterialVisualResolution ResolveFallback(const std::string& identitySeed);
};

}  // namespace dominus::graphics
