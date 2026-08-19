// GRAPHICS/Renderer/MaterialContract.cpp
#include "GRAPHICS/Renderer/MaterialContract.h"

#include "GRAPHICS/Renderer/MaterialAppearance.h"

namespace dominus::graphics {

MaterialVisualResolution MaterialContract::Resolve(const registry::ImmutableArtifact& artifact) {
    if (artifact.Kind() != registry::GenomeKind::kMaterial) {
        // Real, honest degrade -- never misinterpret a CombatGenome
        // artifact's DecisionWeights as material data. Keyed by the
        // artifact's own real entity id, so this is still a real,
        // traceable resolution, not an arbitrary default.
        return ResolveFallback(artifact.EntityId());
    }

    MaterialVisualResolution resolution;
    resolution.has_texture = false;  // real, honest -- no texture system exists, see header comment
    resolution.from_registered_artifact = true;
    resolution.source_hash = artifact.Hash();

    // Reuses the real, existing MaterialAppearance authority --
    // artifact.Hash() is already a real Sha256 over the FULL canonical
    // genome (material_id + every MaterialProperties field), so this
    // is not a second hash algorithm, it's the same one, fed a richer,
    // more complete real seed than the renderer's current
    // material_ref-only path uses.
    ResolveMaterialColor(artifact.Hash(), artifact.Material().properties.wear_state, resolution.r, resolution.g,
                          resolution.b);
    return resolution;
}

MaterialVisualResolution MaterialContract::ResolveFallback(const std::string& identitySeed) {
    MaterialVisualResolution resolution;
    resolution.has_texture = false;
    resolution.from_registered_artifact = false;
    resolution.source_hash = "";
    // wear_state=0.0 -- matches the real, existing renderer fallback
    // behavior today (an entity with no material_ref never blends
    // toward "worn"), named and tested here rather than left implicit.
    ResolveMaterialColor(identitySeed, 0.0f, resolution.r, resolution.g, resolution.b);
    return resolution;
}

}  // namespace dominus::graphics
