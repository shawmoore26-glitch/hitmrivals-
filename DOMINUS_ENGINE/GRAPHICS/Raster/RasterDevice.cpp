// GRAPHICS/Raster/RasterDevice.cpp
#include "GRAPHICS/Raster/RasterDevice.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

#include "GRAPHICS/Renderer/MaterialAppearance.h"
#include "GRAPHICS/Renderer/MeshLibrary.h"
#include "GRAPHICS/Renderer/MeshTransform.h"
#include "GRAPHICS/Renderer/TextureAtlas.h"
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

// Texture Capability phase (Track H Phase 5A). Writes `src` blended
// over whatever is already at (x,y) using `src`'s own real alpha byte
// -- standard "over" alpha compositing (out = src*a + dst*(1-a)),
// computed per channel. Matches PutPixel's own convention of always
// leaving the destination buffer fully opaque (alpha=255) -- this
// engine has no real transparent FINAL output anywhere (see
// RasterDevice.h's own "real, filled, opaque background" claim); only
// intermediate compositing uses the source's real alpha.
void BlendPixel(PixelBuffer& buffer, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (x < 0 || y < 0 || x >= buffer.width || y >= buffer.height) return;
    std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(buffer.width) +
                      static_cast<std::size_t>(x)) *
                     4;
    if (a == 255) {
        buffer.rgba[i + 0] = r;
        buffer.rgba[i + 1] = g;
        buffer.rgba[i + 2] = b;
        buffer.rgba[i + 3] = 255;
        return;
    }
    if (a == 0) return;  // fully transparent source pixel -- real no-op, not a fabricated write
    float srcA = static_cast<float>(a) / 255.0f;
    float dstA = 1.0f - srcA;
    buffer.rgba[i + 0] = static_cast<std::uint8_t>(static_cast<float>(r) * srcA + static_cast<float>(buffer.rgba[i + 0]) * dstA);
    buffer.rgba[i + 1] = static_cast<std::uint8_t>(static_cast<float>(g) * srcA + static_cast<float>(buffer.rgba[i + 1]) * dstA);
    buffer.rgba[i + 2] = static_cast<std::uint8_t>(static_cast<float>(b) * srcA + static_cast<float>(buffer.rgba[i + 2]) * dstA);
    buffer.rgba[i + 3] = 255;
}

// Real per-vertex atlas-pixel coordinates (u,v), interpolated the same
// real, exact way position already is -- barycentric weights from the
// SAME edge functions FillTriangle already computes (this is an affine
// 2D transform, so linear barycentric interpolation of UV is exact,
// not an approximation; no perspective divide is needed or applied).
// Real, disclosed sampling choice: NEAREST-neighbor, not bilinear --
// simpler, fully deterministic (no filtering-kernel edge cases to
// reason about), and consistent with this class's existing "real,
// simple, disclosed choice, not a shading model" philosophy
// (MaterialAppearance.h's own words). `atlas` is never null here --
// the caller (Rasterize) has already resolved and checked it.
void FillTexturedTriangle(PixelBuffer& buffer, float x0, float y0, float u0, float v0, float x1, float y1, float u1,
                           float v1, float x2, float y2, float u2, float v2, const TextureAtlas& atlas) {
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
            if (!inside) continue;

            float b0 = w0 / area;
            float b1 = w1 / area;
            float b2 = w2 / area;
            float u = b0 * u0 + b1 * u1 + b2 * u2;
            float v = b0 * v0 + b1 * v1 + b2 * v2;

            int sx = std::clamp(static_cast<int>(u), 0, atlas.width - 1);
            int sy = std::clamp(static_cast<int>(v), 0, atlas.height - 1);
            std::size_t si = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(atlas.width) +
                               static_cast<std::size_t>(sx)) *
                              4;
            BlendPixel(buffer, x, y, atlas.rgba[si + 0], atlas.rgba[si + 1], atlas.rgba[si + 2], atlas.rgba[si + 3]);
        }
    }
}

const TextureAtlas* FindAtlas(const Frame& frame, const std::string& atlasId) {
    for (const auto& atlas : frame.atlases) {
        if (atlas.atlas_id == atlasId) return &atlas;
    }
    return nullptr;
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

        if (cmd.textured) {
            const TextureAtlas* atlas = FindAtlas(frame, cmd.atlas_id);
            // Real, disclosed choice: a textured command whose atlas_id
            // does not name any atlas this Frame actually carries draws
            // NOTHING (skipped entirely), rather than silently falling
            // back to the flat-color path -- that fallback would mask a
            // real caller bug (a Frame built with `textured=true` but an
            // inconsistent/missing atlas), the same "real refusal, not a
            // fabricated fill" discipline FillTriangle's own degenerate-
            // triangle check already uses above.
            if (!atlas) continue;

            // MeshLibrary::Resolve always returns the one real UnitQuad
            // mesh (see MeshLibrary.h's own header comment) -- 4
            // vertices, local space -0.5..0.5, wound
            // top-left/top-right/bottom-right/bottom-left. This is the
            // one real, fixed corner-to-pixel-rect mapping for that
            // mesh: local +y (up) is the atlas rect's TOP row (v=src_y),
            // matching a PNG's own real top-to-bottom scanline order
            // (TextureAtlas.h) and RasterDevice's own screen Y-flip
            // convention below.
            const float u0 = static_cast<float>(cmd.atlas_src_x);
            const float v0 = static_cast<float>(cmd.atlas_src_y);
            const float u1 = static_cast<float>(cmd.atlas_src_x + cmd.atlas_src_w);
            const float v1 = static_cast<float>(cmd.atlas_src_y + cmd.atlas_src_h);
            std::vector<std::pair<float, float>> uvs = {
                {u0, v0},  // 0: top-left
                {u1, v0},  // 1: top-right
                {u1, v1},  // 2: bottom-right
                {u0, v1},  // 3: bottom-left
            };

            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                auto toScreen = [&](std::size_t idx) -> std::tuple<float, float, float, float> {
                    std::uint32_t vi = mesh.indices[idx];
                    const auto& [wx, wy] = positions[vi];
                    float sx = static_cast<float>(options.width) * 0.5f + wx;
                    float sy = static_cast<float>(options.height) * 0.5f - wy;
                    const auto& [uu, vv] = uvs[vi];
                    return {sx, sy, uu, vv};
                };
                auto [x0, y0, uu0, vv0] = toScreen(i);
                auto [x1, y1, uu1, vv1] = toScreen(i + 1);
                auto [x2, y2, uu2, vv2] = toScreen(i + 2);
                FillTexturedTriangle(buffer, x0, y0, uu0, vv0, x1, y1, uu1, vv1, x2, y2, uu2, vv2, *atlas);
            }
            continue;
        }

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
