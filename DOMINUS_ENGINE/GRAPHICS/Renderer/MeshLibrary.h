// GRAPHICS/Renderer/MeshLibrary.h
// Resolves a real mesh_ref string to a real Mesh -- NOT an asset-
// loading pipeline. DOMINUS has no mesh files anywhere to load; there
// is exactly one real, built-in Mesh today (a unit quad), and every
// mesh_ref -- recognized or not, empty or not -- resolves to it. This
// is the same shape every DrawCommand has implicitly used since the
// Raster phase (GRAPHICS/Raster/RasterDevice.cpp's own rectangle),
// now promoted to a real, named, reusable geometry asset instead of
// being re-derived ad hoc inside each renderer's own vertex-building
// code -- one real mesh, two real consumers (RasterDevice and
// VulkanFrameRenderer), never two divergent shapes.
//
// The lookup exists (rather than the renderers just hardcoding the
// quad directly) so mesh_ref genuinely participates in resolution --
// and so that when a real mesh-asset system exists, it has an obvious
// place to plug in without changing any renderer.
#pragma once

#include <string>

#include "GRAPHICS/Renderer/Mesh.h"

namespace dominus::graphics {

class MeshLibrary {
public:
    static const Mesh& UnitQuad();
    static const Mesh& Resolve(const std::string& meshRef);
};

}  // namespace dominus::graphics
