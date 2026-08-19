// GRAPHICS/Renderer/Mesh.h
// The smallest real geometry representation DOMINUS has ever had.
//
// Confirmed by a fresh, full-repository search before writing this
// file (see GRAPHICS/README.md's geometry-milestone record): no
// Vertex/Mesh type, no vertex list, no index list exists anywhere in
// DOMINUS outside VulkanFrameRenderer's own internal, GPU-layout-only
// Vertex struct. VISUALFORGE::AssetSpecification's "mesh" is a
// REQUIREMENT category string ("this character needs a body mesh"),
// never actual geometry data.
//
// This file does not invent a MeshComponent to satisfy the renderer.
// It defines the minimum real thing: a named, local-space (untransformed,
// unit-scale) vertex/index list. Positions only -- explicitly no
// normals, no UVs. Neither would be real data: there is no lighting
// model anywhere in DOMINUS for a normal to feed, and no texture/image
// asset representation anywhere for a UV to sample -- adding either
// field would be exactly the kind of fake completeness this engine has
// refused for its whole history. When a real shading or texture system
// exists, those fields belong here; not before.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dominus::graphics {

struct Vertex2D {
    float x = 0.0f;
    float y = 0.0f;
};

struct Mesh {
    std::string mesh_id;
    std::vector<Vertex2D> vertices;   // local/object space, untransformed
    std::vector<std::uint32_t> indices;
};

}  // namespace dominus::graphics
