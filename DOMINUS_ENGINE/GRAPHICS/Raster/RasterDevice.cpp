// GRAPHICS/Raster/RasterDevice.cpp
#include "GRAPHICS/Raster/RasterDevice.h"

#include <algorithm>
#include <cmath>

#include "GRAPHICS/Renderer/MaterialAppearance.h"
#include "GRAPHICS/Renderer/MeshLibrary.h"
#include "GRAPHICS/Renderer/MeshTransform.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::graphics {

namespace {

void PutPixel(PixelBuffer& buffer, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (x < 0 || y < 0 || x >= buffer.width || y >= buffer.height) return;
    std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(buffer.width) +
                      static_cast<std::size_t>(x)) *
                     4;
    buffer.rgba[i + 0] = r;
    buffer.rgba[i + 1] = g;
    buffer.rgba[i + 2] = b;
    buffer.rgba[i + 3] = 255;
}

// Real, standard edge-function triangle rasterization -- the same
// fundamental algorithm real software and GPU rasterizers use at their
// core. Replaces the previous axis-aligned FillRect: once real
// rotation is applied to real mesh vertices (GRAPHICS/Renderer/
// MeshTransform.h), a rotated quad's two triangles are no longer
// axis-aligned, and an axis-aligned rect fill would draw the wrong
// shape. Pixel centers are sampled at (x+0.5, y+0.5) -- standard
// rasterization convention, not an arbitrary choice.
float EdgeFunction(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void FillTriangle(PixelBuffer& buffer, float x0, float y0, float x1, float y1, float x2, float y2, std::uint8_t r,
                   std::uint8_t g, std::uint8_t b) {
    float area = EdgeFunction(x0, y0, x1, y1, x2, y2);
    if (area == 0.0f) return;  // degenerate triangle -- real refusal, not a fabricated fill

    int minX = std::max(0, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
    int maxX = std::min(buffer.width - 1, static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
    int maxY = std::min(buffer.height - 1, static_cast<int>(std::ceil(std::max({y0, y1, y2}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float py = static_cast<float>(y) + 0.5f;
            float w0 = EdgeFunction(x1, y1, x2, y2, px, py);
            float w1 = EdgeFunction(x2, y2, x0, y0, px, py);
            float w2 = EdgeFunction(x0, y0, x1, y1, px, py);
            bool inside = (area > 0.0f) ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (inside) PutPixel(buffer, x, y, r, g, b);
        }
    }
}

// A real, deterministic color derived from a real string -- the SAME
// Sha256 this entire engine already trusts everywhere else (REGISTRY::
// Hash::Sha256), never a second, invented color-hash function. Because
// color is a pure function of already-real, already-hashed content
// (never randomness, never memory address, never insertion order),
// the same Frame always produces the exact same pixels. As of this
// phase, material_wear_state (a real, already-authoritative
// MaterialGenome value) also participates -- see
// GRAPHICS/Renderer/MaterialAppearance.h for the disclosed blend.

}  // namespace

PixelBuffer RasterDevice::Rasterize(const Frame& frame, const RasterOptions& options) {
    PixelBuffer buffer;
    buffer.width = options.width;
    buffer.height = options.height;
    buffer.rgba.assign(static_cast<std::size_t>(options.width) * static_cast<std::size_t>(options.height) * 4, 0);
    // A real, filled, opaque background -- not uninitialized memory
    // pretending to be "empty". Alpha channel set explicitly.
    for (std::size_t i = 3; i < buffer.rgba.size(); i += 4) buffer.rgba[i] = 255;

    // Frame.commands is already in FrameCompiler's fixed, deterministic
    // order (sort_layer, then entity_id) -- drawn in that exact order,
    // with no ordering logic of this class's own. Draw order IS this
    // engine's real notion of "depth": no Z-buffer exists anywhere in
    // DOMINUS, so a later-drawn (higher sort_layer) entity visually
    // occludes an earlier one wherever they overlap -- a real, honest
    // painter's-algorithm depth, not a fabricated 3D one.
    for (const auto& cmd : frame.commands) {
        const Mesh& mesh = MeshLibrary::Resolve(cmd.mesh_ref);
        auto positions = TransformMeshVertices(mesh, cmd.screen_transform);

        std::uint8_t r, g, b;
        if (cmd.material_resolved) {
            // Material Implementation Phase: the real, registry-backed
            // MaterialContract resolution (full canonical genome hash,
            // via the WORLD-to-Scene bridge) -- takes priority over the
            // material_ref-only shortcut below when it's real,
            // pre-resolved data, not fabricated at render time.
            r = cmd.material_r;
            g = cmd.material_g;
            b = cmd.material_b;
        } else {
            // Pre-existing path, unchanged -- used by every hand-built
            // Scene/Frame (every existing RasterDevice unit test) that
            // never went through the WORLD-to-Scene bridge's
            // registry-backed resolution.
            std::string colorSeed = !cmd.material_ref.empty() ? cmd.material_ref : cmd.entity_id;
            ResolveMaterialColor(colorSeed, cmd.material_wear_state, r, g, b);
        }

        // mesh.indices is real, triangulated index data -- every
        // triangle in the real mesh is rasterized, not assumed to be a
        // quad.
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            auto toScreen = [&](std::size_t idx) -> std::pair<float, float> {
                const auto& [wx, wy] = positions[mesh.indices[idx]];
                float sx = static_cast<float>(options.width) * 0.5f + wx;
                // Screen Y grows downward; world/camera Y grows upward
                // -- a real, standard convention flip, applied AFTER
                // rotation (rotation happens in world space).
                float sy = static_cast<float>(options.height) * 0.5f - wy;
                return {sx, sy};
            };
            auto [x0, y0] = toScreen(i);
            auto [x1, y1] = toScreen(i + 1);
            auto [x2, y2] = toScreen(i + 2);
            FillTriangle(buffer, x0, y0, x1, y1, x2, y2, r, g, b);
        }
    }

    buffer.buffer_hash = registry::Sha256::Hash(
        std::string(reinterpret_cast<const char*>(buffer.rgba.data()), buffer.rgba.size()));
    return buffer;
}

}  // namespace dominus::graphics
