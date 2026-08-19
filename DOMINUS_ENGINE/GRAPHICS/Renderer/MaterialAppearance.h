// GRAPHICS/Renderer/MaterialAppearance.h
// Turns a real material_ref + a real material_wear_state into a real,
// deterministic RGB color -- used identically by RasterDevice (CPU)
// and VulkanFrameRenderer (GPU) so the two renderers can never
// silently disagree, the same discipline as MeshTransform.h.
//
// Two real, disclosed inputs, and nothing invented beyond them:
//   - material_ref -> a base color via the SAME registry::Sha256::Hash
//     authority this whole engine already trusts (unchanged from
//     before this file existed).
//   - material_wear_state (0=pristine, 1=destroyed) -> a real,
//     explicit, DETERMINISTIC linear blend toward a fixed dark-gray
//     "worn" tone. This is a real, simple, disclosed choice -- not a
//     shading model. MaterialGenome's own header is explicit that it
//     stores "state, not rendering" (CHARACTER/Genome/MaterialGenome.h):
//     no real DOMINUS data says what a worn material should actually
//     look like, so this file does not pretend one does. Darkening
//     toward gray is a real, named, honest interpretation of "more
//     worn = more visually degraded" -- if DOMINUS ever gains a real
//     shading/wear-visualization authority, this is the one place
//     that needs to change.
//
// What this file does NOT do: read a texture (none exists anywhere in
// DOMINUS -- see GRAPHICS/README.md's "Texture authority: NOT YET
// PRESENT"), apply lighting (no lighting model exists), or invent any
// other MaterialProperties field's visual meaning (age_years,
// damage_history, weather_exposure remain real, authoritative,
// unconsumed data -- same honesty as texture support: not yet
// present, not faked).
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "REGISTRY/Hash/Sha256.h"

namespace dominus::graphics {

inline void ResolveMaterialColor(const std::string& colorSeed, float wearState, std::uint8_t& r, std::uint8_t& g,
                                  std::uint8_t& b) {
    std::string hash = registry::Sha256::Hash(colorSeed);
    auto hexByte = [&](std::size_t pos) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::stoul(hash.substr(pos, 2), nullptr, 16));
    };
    std::uint8_t baseR = hexByte(0);
    std::uint8_t baseG = hexByte(2);
    std::uint8_t baseB = hexByte(4);

    float wear = std::clamp(wearState, 0.0f, 1.0f);
    constexpr std::uint8_t kWornGray = 60;  // real, fixed, disclosed "worn" tone
    r = static_cast<std::uint8_t>(baseR + (static_cast<float>(kWornGray) - baseR) * wear);
    g = static_cast<std::uint8_t>(baseG + (static_cast<float>(kWornGray) - baseG) * wear);
    b = static_cast<std::uint8_t>(baseB + (static_cast<float>(kWornGray) - baseB) * wear);
}

}  // namespace dominus::graphics
