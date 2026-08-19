// GRAPHICS/Renderer/Frame.h
// The hard architectural invariant this engine now enforces:
//
//   WORLD::EntityRegistry
//         |
//   GRAPHICS::Scene / FrameCompiler
//         |
//   GRAPHICS::Frame          <- the ONLY channel past this point
//         |
//   RasterDevice | VulkanFrameRenderer
//
// Neither renderer knows EntityRegistry exists. Confirmed directly,
// not assumed: `GRAPHICS/Raster/RasterDevice.{h,cpp}` and
// `GRAPHICS/Vulkan/VulkanFrameRenderer.{h,cpp}` contain zero
// references to `EntityRegistry`, `MetaBinObject`, `Scene`, or
// `SceneEntity` anywhere -- their entire public surface (`Rasterize`,
// `Render`, `RenderOffscreen`) takes a `const Frame&` and nothing
// else. `GRAPHICS::SceneFromEntities` is the one, explicit,
// documented bridge on the OTHER side of this boundary (WORLD ->
// Scene); it is not part of the renderer, and the renderer never
// calls it. The scene is authoritative world state; a Frame is an
// immutable rendering snapshot; the renderer only ever consumes the
// snapshot.
//
// A Frame is the complete, deterministic rendering description: a
// real, hash-addressed camera, viewport, and ORDERED list of draw
// commands -- not pixels. Two real rasterizers exist and consume this
// exact same type (GRAPHICS/Raster/RasterDevice.h for CPU,
// GRAPHICS/Vulkan/VulkanFrameRenderer.h for a real GPU) -- "logical
// determinism" (this file, and FrameCompiler's own hash) and "raster
// determinism" (each renderer's own golden-image proof) are different,
// both real, guarantees; conflating them would be exactly the kind of
// overclaim this engine has refused for 20+ phases.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ANIMATION/SkeletonSystem/Transform2D.h"
#include "GRAPHICS/Renderer/Camera.h"

namespace dominus::graphics {

struct DrawCommand {
    std::string entity_id;
    animation::Transform2D screen_transform;  // already camera-transformed, see FrameCompiler
    std::string material_ref;
    std::string mesh_ref;
    int sort_layer = 0;
    float material_wear_state = 0.0f;  // real MaterialGenome::MaterialProperties::wear_state, see Scene.h
    // Real, pre-resolved MaterialContract output -- see Scene.h's
    // SceneEntity fields of the same name for the full explanation.
    bool material_resolved = false;
    std::uint8_t material_r = 0;
    std::uint8_t material_g = 0;
    std::uint8_t material_b = 0;
};

// Descriptive, not prescriptive: the render-target dimensions a Frame
// was compiled with in mind. In this engine's 2D architecture,
// viewport does NOT affect any DrawCommand's screen_transform --
// proven directly by the resize test (RasterOptions/RenderOffscreen's
// width/height are independent renderer-time parameters, and a single
// compiled Frame legitimately renders correctly at multiple target
// sizes; see GRAPHICS/README.md's resize-test record). This field
// exists so a Frame is a complete, self-describing snapshot -- you can
// inspect one and know what it was compiled for -- without binding a
// renderer to that size. {0,0} means "unspecified".
struct Viewport {
    int width = 0;
    int height = 0;
};

// The complete, deterministic rendering snapshot: DOMINUS Scene ->
// FrameCompiler -> Frame -> [Vulkan | RasterDevice]. Neither renderer
// reads WORLD::EntityRegistry, CORE::MetaBinObject, or Scene/SceneEntity
// directly (confirmed: zero such references exist in GRAPHICS/Raster/
// RasterDevice.{h,cpp} or GRAPHICS/Vulkan/VulkanFrameRenderer.{h,cpp})
// -- this struct is the ONLY channel. `camera` is a real copy of the
// Camera this Frame was compiled against (not just its baked-in
// effect on each command's screen_transform) -- so a Frame carries its
// own provenance, not just its result.
struct Frame {
    Camera camera;
    Viewport viewport;
    std::vector<DrawCommand> commands;  // fixed, deterministic order -- see FrameCompiler
    std::string frame_hash;             // real content hash of the whole frame, including camera and viewport
};

}  // namespace dominus::graphics
