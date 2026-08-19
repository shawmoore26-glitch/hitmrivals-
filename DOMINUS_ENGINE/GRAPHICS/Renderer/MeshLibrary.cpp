// GRAPHICS/Renderer/MeshLibrary.cpp
#include "GRAPHICS/Renderer/MeshLibrary.h"

namespace dominus::graphics {

const Mesh& MeshLibrary::UnitQuad() {
    // -0.5..0.5 on both axes -- a real, centered, unit-extent quad,
    // wound counter-clockwise (matching VulkanFrameRenderer's existing
    // VK_FRONT_FACE_COUNTER_CLOCKWISE convention), 4 unique vertices +
    // 6 indices describing two triangles that share an edge.
    static const Mesh quad{
        "dominus.unit_quad",
        {
            {-0.5f, 0.5f},   // 0: top-left
            {0.5f, 0.5f},    // 1: top-right
            {0.5f, -0.5f},   // 2: bottom-right
            {-0.5f, -0.5f},  // 3: bottom-left
        },
        {0, 1, 2, 0, 2, 3},
    };
    return quad;
}

const Mesh& MeshLibrary::Resolve(const std::string& /*meshRef*/) {
    // Every real mesh_ref resolves to the one real mesh DOMINUS has
    // today -- see this file's header comment for exactly why. Not a
    // fallback masking a missing asset: there is no asset to be
    // missing yet.
    return UnitQuad();
}

}  // namespace dominus::graphics
