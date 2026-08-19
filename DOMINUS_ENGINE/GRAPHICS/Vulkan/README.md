# DOMINUS Vulkan GPU Renderer — Phase 1

This is the first real GPU rendering backend in DOMINUS.

## Real execution path

```text
Scene + Camera
      ↓
FrameCompiler
      ↓
canonical Frame + frame_hash
      ↓
VulkanFrameRenderer
      ↓
real CPU vertex staging buffer
      ↓
VkCommandBuffer
      ↓
vertex shader (SPIR-V)
      ↓
GPU triangle assembly + rasterization
      ↓
fragment shader (SPIR-V)
      ↓
swapchain image
      ↓
real presentation
```

Every `DrawCommand` becomes a real GPU-rendered rectangle made from two triangles. Position and scale come from the canonical `Frame`; color comes from the engine's existing SHA-256 authority. This is intentionally the first GPU slice, not a claim that mesh/texture/material asset resolution is complete.

## Build requirements

The GPU target requires:

- Vulkan headers + loader
- GLFW 3
- `glslc` from a Vulkan SDK (used to compile the checked-in GLSL sources into SPIR-V at build time)

CMake exposes `DOMINUS_ENABLE_VULKAN`. It defaults to `OFF` when dependencies are unavailable, so the established CPU test suite remains buildable. When explicitly enabled, missing Vulkan/GLFW/glslc is a configuration error rather than a fake success.
