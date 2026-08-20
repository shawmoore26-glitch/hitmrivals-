// GRAPHICS/Renderer/Scene.h
// A Scene is a flat list of entities with real world transforms and
// real (but UNRESOLVED) material/mesh references -- honest about what
// this engine actually has: no loaded mesh data, no loaded texture
// data, no skin/vertex data anywhere (VISUALFORGE's own
// RendererPackageAcceptanceHarness already established this). A
// SceneEntity's material_ref/mesh_ref are real strings that trace back
// to real genome data (a MaterialGenome's material_id, a
// RawRefComponent's stored path) -- never fabricated, but also never
// claimed to be "loaded" or "resolved" into pixels here.
//
// Material Implementation Phase: MaterialGenome -> renderable material.
// SceneEntity gained real, RESOLVED material color fields
// (material_resolved/material_r/g/b), populated ONLY by
// SceneFromEntities (the one real bridge that has access to a real
// GenomeRegistry to compile/register/resolve against -- see
// SceneFromEntities.h). Hand-built Scenes (every existing RasterDevice
// and VulkanFrameRenderer unit test) leave material_resolved at its
// default false and are completely unaffected -- the renderer falls
// back to the pre-existing material_ref-hash path for them, unchanged.
// This is deliberate: it preserves 100% of existing test behavior
// while making the REAL entity pipeline (WORLD -> SceneFromEntities)
// flow through the full, authoritative GenomeRegistry -> MaterialContract
// chain instead of the renderer's own shortcut.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ANIMATION/SkeletonSystem/Transform2D.h"

namespace dominus::graphics {

struct SceneEntity {
    std::string entity_id;
    animation::Transform2D world_transform;
    std::string material_ref;  // real reference, e.g. a MaterialGenome's material_id -- "" if none attached
    std::string mesh_ref;      // real reference, e.g. a .dominus "mesh" ref path -- "" if none attached
    int sort_layer = 0;        // real, explicit draw order -- ties broken by entity_id (see FrameCompiler)
    // A real, existing, already-authoritative material PARAMETER --
    // MaterialGenome::MaterialProperties::wear_state (0=pristine,
    // 1=destroyed), already canonically hashed by
    // REGISTRY::MaterialGenomeCompiler and already part of
    // VISUALFORGE's material_genome_hash -- this field is not a new
    // concept, it is finally letting a renderer read a real value that
    // has been authoritative everywhere else in DOMINUS all along.
    // "" material_ref means this stays at its default (0.0), same as
    // any other unattached material field.
    float material_wear_state = 0.0f;

    // Real, pre-resolved MaterialContract output (Material
    // Implementation Phase) -- set only by SceneFromEntities. When
    // false, renderers use the pre-existing material_ref-hash
    // fallback path, unchanged. has_texture is always false today
    // (see MaterialContract.h) -- no texture field exists here because
    // there is nothing real for it to carry yet.
    bool material_resolved = false;
    std::uint8_t material_r = 0;
    std::uint8_t material_g = 0;
    std::uint8_t material_b = 0;

    // HITM Sprite Bridge phase (Track H Phase 5B). When `textured` is
    // false (the default -- every SceneEntity built before this phase
    // existed), this entity compiles to a DrawCommand exactly as it
    // always has. See GRAPHICS/Renderer/Frame.h's own `DrawCommand`
    // fields of the same name for the full explanation -- these mirror
    // them exactly, one field for one field, so FrameCompiler::Compile
    // can copy them straight across with no reinterpretation. Populated
    // by CHARACTER::hitm::BuildHitmSceneEntities (CHARACTER/HitmBridge/
    // HitmSceneBridge.h) from a real HitmPartDraw's own `frame_x/y/w/h`
    // -- never by SceneFromEntities, which has no texture data to give.
    bool textured = false;
    std::string atlas_id;
    int atlas_src_x = 0;
    int atlas_src_y = 0;
    int atlas_src_w = 0;
    int atlas_src_h = 0;
};

struct Scene {
    std::vector<SceneEntity> entities;
};

}  // namespace dominus::graphics
