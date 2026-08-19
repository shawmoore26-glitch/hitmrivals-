// GRAPHICS/Renderer/SceneFromEntities.h
// The real "Entity/Component data reaches the renderer" path. DOMINUS
// already has a real entity substrate -- WORLD::EntityRegistry holding
// CORE::MetaBinObject entities, each a generic component bag (WORLD
// LAW 002: no special-cased Enemy/Tree/Character types, just Entity +
// Components). This file does not invent a second one. It reads real
// components off real entities and produces a real Scene -- nothing
// more.
//
// Two real components this file reads, both pre-existing:
//   - world::SpatialComponent (WORLD/Core/SpatialComponent.h) for
//     position. Honest limitation, stated plainly: SpatialComponent
//     carries x/y/optional-z only -- no rotation, no scale. There is
//     no "visual scale" component anywhere in DOMINUS today. Entities
//     built from SpatialComponent alone get rotation_deg=0,
//     scale_x=scale_y=1.0 -- a real, disclosed default, not a
//     fabricated one.
//   - character::MaterialGenomeComponent (CHARACTER/Rig/RigBinder.h)
//     for material_ref and material_wear_state, if the entity happens
//     to carry one (the SAME component RigBinder::Bind attaches when
//     loading a .dominus character). Absent if the entity has no
//     material component; never fabricated.
//
// An entity with no SpatialComponent contributes no SceneEntity --
// there is nothing real to place it at, and this file will not invent
// a default position.
//
// SORT_LAYER, disclosed plainly (found via a real test failure, not
// assumed): WORLD entities have no real "draw layer"/"z-order"
// property anywhere in DOMINUS today, the same honest gap
// SpatialComponent's missing rotation/scale represents. Rather than
// fabricate one, `Build()` assigns sort_layer by the POSITION of each
// id in the caller-supplied `entityIds` list -- the first id drawn
// first, and so on. This means the ORDER of `entityIds` is a real,
// meaningful input to this function, not an arbitrary iteration detail
// -- reversing it changes each entity's assigned layer, and therefore
// can change the real, deterministic Frame this produces. This is
// FrameCompiler's own sort (by sort_layer, then entity_id) still being
// fully deterministic and order-independent GIVEN a fixed sort_layer
// assignment -- it is this function's OWN, separate, disclosed choice
// to derive that assignment from list position when nothing more
// authoritative exists yet.
//
// Material Implementation Phase: when a real `genomeRegistry` is
// passed (optional, defaults to nullptr, preserving the exact
// pre-existing behavior for every prior caller that doesn't pass one),
// an entity's real MaterialGenomeComponent is compiled
// (GenomeCompiler::CompileMaterialGenome -- reuses the existing
// canonical hash, invents nothing new), REGISTERED into the SAME
// GenomeRegistry Phase 1 made authoritative (never a parallel
// registry), and RESOLVED via MaterialContract::Resolve (Phase 2) --
// the real, full-genome color, not just a hash of material_id. An
// entity with no MaterialGenomeComponent still gets a real, resolved
// color via MaterialContract::ResolveFallback(entity_id) -- which
// reuses the exact same underlying color function
// (GRAPHICS::ResolveMaterialColor) the pre-existing renderer fallback
// already used, so THAT specific case's color is unchanged even under
// the new path. Only entities with a real MaterialGenomeComponent see
// a different (more complete, canonical-hash-derived) color than
// before.
#pragma once

#include <string>
#include <vector>

#include "GRAPHICS/Renderer/Scene.h"
#include "REGISTRY/GenomeRegistry.h"
#include "WORLD/Core/EntityRegistry.h"

namespace dominus::graphics {

class SceneFromEntities {
public:
    // Builds a real Scene from a real EntityRegistry -- only entities
    // named in `entityIds` that actually carry a real SpatialComponent
    // are included, in the order given (FrameCompiler is responsible
    // for the graph's own deterministic ordering downstream; this
    // function does no reordering of its own).
    //
    // `genomeRegistry`: optional. nullptr (the default) preserves
    // exactly the pre-existing behavior -- material_resolved stays
    // false, renderers fall back to the pre-existing material_ref-hash
    // path. Passing a real GenomeRegistry enables the real,
    // registry-backed material resolution described above.
    static Scene Build(const world::EntityRegistry& registry, const std::vector<std::string>& entityIds,
                        registry::GenomeRegistry* genomeRegistry = nullptr);
};

}  // namespace dominus::graphics
