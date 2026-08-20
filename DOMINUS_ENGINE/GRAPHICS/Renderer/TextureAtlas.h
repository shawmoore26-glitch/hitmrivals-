// GRAPHICS/Renderer/TextureAtlas.h
// Texture Capability phase (Track H Phase 5A) -- the minimum real
// texture resource DOMINUS has ever had. GRAPHICS/Renderer/Mesh.h's own
// header comment named this exact, honestly-disclosed gap when it was
// written: "no texture/image asset representation anywhere for a UV to
// sample... When a real shading or texture system exists, those fields
// belong here; not before." This is that real system's data type.
//
// A TextureAtlas is real, already-decoded RGBA8 pixel data -- produced
// by GRAPHICS::PngDecoder from a real PNG file (see
// GRAPHICS/Raster/PngDecoder.h), never a placeholder or a procedurally
// generated color. width*height*4 bytes, row-major, top-to-bottom --
// the same convention GRAPHICS::PixelBuffer's own `rgba` already uses
// and PNG's own scanline order already produces, so no row-flip is
// needed anywhere between decode and sample.
//
// Deliberately NOT a GPU resource: no VkImage, no VkImageView, no
// VkSampler anywhere in this file. This is the CPU-side, renderer-
// agnostic resource a `GRAPHICS::Frame` carries (see Frame.h's own
// `atlases` field) -- exactly the same shape TextureAtlas gives
// RasterDevice today; a future GPU-side sampler/descriptor pipeline
// (out of scope for this phase -- see this repo's
// HITM_RENDER_INPUT_LOOP_AUDIT.md section A and this phase's own
// report for why) would upload this same real pixel data to a real
// VkImage, not replace or duplicate it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dominus::graphics {

struct TextureAtlas {
    std::string atlas_id;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;  // real bytes, width*height*4, RGBA8
};

}  // namespace dominus::graphics
