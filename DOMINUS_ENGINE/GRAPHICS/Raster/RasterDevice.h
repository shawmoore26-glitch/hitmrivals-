// GRAPHICS/Raster/RasterDevice.h
// The first real, actual pixel output this engine has ever produced.
// GRAPHICS/README.md named this exact gap: "No rasterizer... There is
// no GPU context, no image buffer, no Vulkan/DirectX/Metal/OpenGL
// anywhere in this engine." This file closes that gap honestly, not
// by overclaiming what it draws.
//
// What this is NOT: a mesh renderer. `DrawCommand.mesh_ref`/
// `material_ref` are real strings this engine has never resolved into
// real vertex data, texture data, or a material/shading model (see
// GRAPHICS/README.md's own "No resolved assets" section, and
// VISUALFORGE/README.md's parallel scoping). There is no real
// geometry anywhere for a rasterizer to draw. Pretending otherwise --
// drawing a placeholder box and calling it "the character" -- would be
// exactly the kind of overclaim this engine has refused for its whole
// history.
//
// What this IS: a real, deterministic, content-derived visualization
// of a Frame's actual logical content. Per DrawCommand, the only real
// information available is its screen_transform (position/rotation/
// scale) and its identity (entity_id, material_ref). RasterDevice
// draws exactly that -- one filled rectangle per command, sized by the
// real transform, colored by a real Sha256 of the real material_ref
// (or entity_id) -- proving the Frame -> pixels path is real,
// reproducible, and order-correct (Frame.commands is already in
// FrameCompiler's fixed deterministic order; this class adds no
// ordering logic of its own), without claiming to render a resolved
// character mesh.
//
// This does NOT unlock VISUALFORGE's `ACTIVE` acceptance state --
// that gate is specifically about resolved, rendered character
// assets, which still do not exist. See GRAPHICS/README.md and
// VISUALFORGE/README.md for why this is a real, separate, later step.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GRAPHICS/Renderer/Frame.h"

namespace dominus::graphics {

struct RasterOptions {
    int width = 256;
    int height = 256;
};

struct PixelBuffer {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;  // real bytes, width*height*4, RGBA8
    std::string buffer_hash;         // real Sha256 over the actual pixel bytes -- not a placeholder
};

class RasterDevice {
public:
    // Real, pure, deterministic: the same Frame always produces the
    // exact same PixelBuffer.buffer_hash. Never touches the registry,
    // never allocates GPU resources (there are none) -- a plain CPU
    // buffer fill.
    static PixelBuffer Rasterize(const Frame& frame, const RasterOptions& options = RasterOptions{});
};

}  // namespace dominus::graphics
