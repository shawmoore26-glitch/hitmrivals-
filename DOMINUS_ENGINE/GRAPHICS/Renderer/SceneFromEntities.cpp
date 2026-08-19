// GRAPHICS/Renderer/SceneFromEntities.cpp
#include "GRAPHICS/Renderer/SceneFromEntities.h"

#include "CHARACTER/Rig/RigBinder.h"
#include "GRAPHICS/Renderer/MaterialContract.h"
#include "REGISTRY/GenomeCompiler.h"
#include "WORLD/Core/SpatialComponent.h"

namespace dominus::graphics {

Scene SceneFromEntities::Build(const world::EntityRegistry& registry, const std::vector<std::string>& entityIds,
                                registry::GenomeRegistry* genomeRegistry) {
    Scene scene;
    int sortLayer = 0;
    for (const auto& id : entityIds) {
        const core::MetaBinObject* entity = registry.Find(id);
        if (!entity) continue;  // real refusal to invent an entity that isn't there

        const world::SpatialComponent* spatial = entity->GetComponent<world::SpatialComponent>();
        if (!spatial) continue;  // no real position data -- nothing honest to place

        SceneEntity sceneEntity;
        sceneEntity.entity_id = entity->Id();
        sceneEntity.world_transform.x = spatial->x;
        sceneEntity.world_transform.y = spatial->y;
        // Real, disclosed default: SpatialComponent has no rotation or
        // scale field anywhere in DOMINUS today (see this file's own
        // header comment) -- 0/1/1 is not a fabricated visual property,
        // it is the honest absence of one.
        sceneEntity.world_transform.rotation_deg = 0.0f;
        sceneEntity.world_transform.scale_x = 1.0f;
        sceneEntity.world_transform.scale_y = 1.0f;

        const auto* material = entity->GetComponent<character::MaterialGenomeComponent>();
        if (material) {
            sceneEntity.material_ref = material->genome.material_id;
            // A real, already-authoritative value -- see Scene.h's own
            // comment on SceneEntity::material_wear_state.
            sceneEntity.material_wear_state = material->genome.properties.wear_state;
        }
        // mesh_ref intentionally left "" -- no real mesh/asset reference
        // component exists on generic WORLD entities anywhere in
        // DOMINUS today.

        // Material Implementation Phase: real, registry-backed
        // resolution, only when a real GenomeRegistry is supplied.
        if (genomeRegistry) {
            MaterialVisualResolution resolution;
            if (material) {
                // Compile (reuses the existing canonical hash, invents
                // nothing new) and REGISTER into the SAME real
                // GenomeRegistry Phase 1 made authoritative -- never a
                // parallel registry.
                auto compileResult =
                    registry::GenomeCompiler::CompileMaterialGenome(sceneEntity.entity_id, material->genome,
                                                                      std::nullopt, 1, "");
                if (compileResult.ok) {
                    genomeRegistry->Register(*compileResult.artifact);
                    resolution = MaterialContract::Resolve(*compileResult.artifact);
                } else {
                    // A real compile failure (e.g. a genuinely invalid
                    // genome) degrades to the same real, named fallback
                    // a missing MaterialGenomeComponent already uses --
                    // never a fabricated color, never a silent crash.
                    resolution = MaterialContract::ResolveFallback(sceneEntity.entity_id);
                }
            } else {
                // No MaterialGenomeComponent at all -- the same real,
                // named fallback path, reusing the identical
                // color function the pre-existing renderer fallback
                // already used, so this specific case's color is
                // unchanged even under the new path.
                resolution = MaterialContract::ResolveFallback(sceneEntity.entity_id);
            }
            sceneEntity.material_resolved = true;
            sceneEntity.material_r = resolution.r;
            sceneEntity.material_g = resolution.g;
            sceneEntity.material_b = resolution.b;
        }

        sceneEntity.sort_layer = sortLayer++;
        scene.entities.push_back(std::move(sceneEntity));
    }
    return scene;
}

}  // namespace dominus::graphics
