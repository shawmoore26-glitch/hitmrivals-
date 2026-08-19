#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "GRAPHICS/Renderer/Frame.h"
#include "GRAPHICS/Renderer/GPUResourceAuthority.h"
#include "GRAPHICS/Renderer/MaterialContract.h"

struct GLFWwindow;

namespace dominus::graphics {

// First production GPU slice for DOMINUS.
//
// This renderer consumes the engine's canonical Frame directly. Each
// DrawCommand becomes two GPU triangles (a rectangle) using its real
// camera-space transform and a deterministic color derived from the same
// registry Sha256 authority used by RasterDevice. The rectangle is not a
// placeholder CPU rasterization: vertex processing and triangle coverage are
// performed by Vulkan on the selected physical GPU.
//
// Scope of this first slice is intentionally bounded:
//   Frame -> CPU vertex staging -> Vulkan vertex buffer -> GPU rasterization
//   -> swapchain image -> presentation.
// It does not claim resolved mesh/texture/material assets yet.
//
// --- Offscreen headless path (this phase) --------------------------------
// A second, genuinely separate rendering path, per explicit direction:
// the windowed/GLFW path above remains the presentation path; it is
// NEVER required to prove the renderer works. InitializeHeadless/
// RenderOffscreen create their own instance/device/pipeline (no GLFW,
// no VkSurfaceKHR, no swapchain) targeting a real, DEVICE_LOCAL,
// COLOR_ATTACHMENT + TRANSFER_SRC VkImage, with a real GPU->CPU
// readback via a HOST_VISIBLE staging buffer. This is the automated
// correctness authority -- see GRAPHICS/Vulkan/README.md for the full
// scope and TOOLS/Editor/dominus_gpu_scene_test.cpp for how it's
// exercised against real DOMINUS Scene/Entity data.
//
// The offscreen path also uses real INDEXED drawing (4 unique vertices
// + 6 indices per rectangle) -- a genuine capability the windowed path
// above does not yet have (it still draws 6 raw vertices per
// rectangle); the windowed path is left completely unmodified rather
// than risk regressing its already-proven behavior for an unrelated
// improvement.
class VulkanFrameRenderer {
public:
    VulkanFrameRenderer() = default;
    ~VulkanFrameRenderer();

    VulkanFrameRenderer(const VulkanFrameRenderer&) = delete;
    VulkanFrameRenderer& operator=(const VulkanFrameRenderer&) = delete;

    // Creates a real GLFW window, Vulkan instance, physical device, logical
    // device, swapchain, render pass, graphics pipeline and command resources.
    // Returns false with a human-readable error when Vulkan/GLFW is absent or
    // the requested surface cannot be created.
    bool Initialize(int width, int height, const std::string& title, std::string& error);

    // Renders one complete canonical Frame through the actual GPU and presents
    // it. The call waits for the submitted frame to finish before returning;
    // this first slice favors deterministic lifecycle correctness over frame
    // pacing complexity. Returns false on a lost/out-of-date surface or GPU
    // submission failure.
    bool Render(const Frame& frame, std::string& error);

    bool ShouldClose() const;
    void PollEvents();
    void Shutdown();

    bool initialized() const { return initialized_; }
    std::uint32_t framebuffer_width() const { return framebuffer_width_; }
    std::uint32_t framebuffer_height() const { return framebuffer_height_; }
    std::string device_name() const;

    // --- Offscreen headless path -------------------------------------------
    // Creates a real Vulkan instance and device with NO GLFW involvement and
    // NO VkSurfaceKHR -- only a graphics-capable queue is required, never a
    // presentation-capable one. Returns false with a real, specific error
    // (never a fabricated success) if no Vulkan-capable physical device is
    // available in this process's environment at all -- the caller is
    // expected to report that as NOT_EXECUTED, not PASS.
    bool InitializeHeadless(std::string& error);

    // Renders one real Frame to a real offscreen VkImage sized
    // width x height, waits for real GPU completion via a real VkFence
    // (never a sleep), and reads the rendered pixels back to `outPixels`
    // as tightly packed RGBA8 (width*height*4 bytes, row-major,
    // top-to-bottom -- the same convention GRAPHICS::RasterDevice
    // already uses). If the offscreen target's size differs from the
    // last call (or this is the first call), the render target,
    // render pass, pipeline, and framebuffer are genuinely recreated
    // at the new size -- a real "render target recreation" distinct
    // from swapchain recreation. Returns false on any real Vulkan
    // failure, with a specific error string; never returns true with
    // stale or partial pixel data.
    bool RenderOffscreen(const Frame& frame, std::uint32_t width, std::uint32_t height,
                          std::vector<std::uint8_t>& outPixels, std::string& error);

    bool headless_initialized() const { return headless_initialized_; }

    // --- GPU Material Resource -----------------------------------------
    // A real, persistent, independently-lifetimed GPU resource -- a
    // uniform buffer holding a MaterialContract-resolved color,
    // consumed by a real fragment shader through a real descriptor
    // set. Deliberately a SEPARATE pipeline/shader/vertex buffer from
    // the offscreen path above (dominus_material_resource.vert/frag,
    // not dominus_triangle.vert/frag) -- zero risk to that path's
    // already-proven, checkpoint-verified pixel output; confirmed by
    // this class never modifying offscreen_pipeline_/pipeline_ or
    // their shaders anywhere. Requires InitializeHeadless to have
    // already succeeded. Reuses the real GPUResourceLifetime contract
    // (Phase 3) and GPUResourceAuthority (this phase) for real
    // ownership/lifetime enforcement -- MarkReady/MarkDestroyed are
    // never bypassed.
    bool CreateMaterialResource(const GPUResourceIdentity& identity, const MaterialVisualResolution& resolution,
                                 std::string& error);
    // Real destruction contract, unchanged from Phase 3: calls
    // vkDeviceWaitIdle before destroying anything, THEN calls
    // MarkDestroyed(true) -- never the reverse, never skipped.
    bool DestroyMaterialResource(const GPUResourceIdentity& identity, std::string& error);
    const GPUResourceLifetime* FindMaterialResource(const GPUResourceIdentity& identity) const;

    // Renders a single real quad using the material resource's real,
    // bound descriptor set -- the color in outPixels comes from the
    // GPU-side uniform buffer via the shader, not from any CPU-side
    // vertex color. Requires the identity to already be in a real
    // kReady state (see CreateMaterialResource); refuses otherwise.
    bool RenderOffscreenWithMaterialResource(const GPUResourceIdentity& identity, std::uint32_t width,
                                              std::uint32_t height, std::vector<std::uint8_t>& outPixels,
                                              std::string& error);

private:
    struct Vertex {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;
    };

    static constexpr std::size_t kMaxVertices = 6 * 4096;
    static constexpr std::size_t kMaxIndices = 6 * 4096;

    bool createInstance(std::string& error);
    bool createSurface(std::string& error);
    bool pickPhysicalDevice(std::string& error);
    bool createLogicalDevice(std::string& error);
    bool createSwapchain(std::string& error);
    bool createImageViews(std::string& error);
    bool createRenderPass(std::string& error);
    bool createPipeline(std::string& error);
    bool createFramebuffers(std::string& error);
    bool createCommandPool(std::string& error);
    bool createVertexBuffer(std::string& error);
    bool createSyncObjects(std::string& error);

    bool recreateSwapchain(std::string& error);
    bool recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
                             std::uint32_t vertexCount, std::string& error);
    bool uploadVertices(const std::vector<Vertex>& vertices, std::string& error);
    bool findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties,
                        std::uint32_t& typeIndex) const;

    std::vector<Vertex> buildVertices(const Frame& frame) const;
    static void deterministicColor(const std::string& seed, float wearState, float& r, float& g, float& b);

    // --- Offscreen-path-only helpers ---------------------------------------
    bool createHeadlessInstance(std::string& error);
    bool pickPhysicalDeviceHeadless(std::string& error);
    bool createLogicalDeviceHeadless(std::string& error);
    bool createHeadlessCommandPool(std::string& error);
    bool createHeadlessSyncObjects(std::string& error);

    bool ensureOffscreenTarget(std::uint32_t width, std::uint32_t height, std::string& error);
    void destroyOffscreenTarget();
    bool createOffscreenImage(std::uint32_t width, std::uint32_t height, std::string& error);
    bool createOffscreenRenderPass(std::string& error);
    bool createOffscreenPipeline(std::uint32_t width, std::uint32_t height, std::string& error);
    bool createOffscreenFramebuffer(std::string& error);
    bool createReadbackBuffer(std::uint32_t width, std::uint32_t height, std::string& error);

    void buildVerticesIndexed(const Frame& frame, std::uint32_t width, std::uint32_t height,
                               std::vector<Vertex>& outVertices, std::vector<std::uint32_t>& outIndices) const;
    bool uploadIndexed(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices,
                        std::string& error);
    bool recordOffscreenCommandBuffer(std::uint32_t indexCount, std::string& error);

    // --- GPU Material Resource helpers --------------------------------
    struct MaterialResourceGpuHandles {
        VkBuffer uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory uniform_memory = VK_NULL_HANDLE;
        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    };

    static std::string MaterialResourceKey(const GPUResourceIdentity& identity);
    bool ensureMaterialResourcePipeline(std::string& error);
    bool createMaterialDescriptorSetLayout(std::string& error);
    bool createMaterialDescriptorPool(std::string& error);
    bool createMaterialResourcePipeline(std::string& error);
    bool recordMaterialResourceCommandBuffer(VkDescriptorSet descriptorSet, std::string& error);
    void destroyMaterialResourceGpuHandles(const MaterialResourceGpuHandles& handles);

    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    std::uint32_t graphics_family_ = UINT32_MAX;
    std::uint32_t present_family_ = UINT32_MAX;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_{};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;

    VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory_ = VK_NULL_HANDLE;

    VkSemaphore image_available_ = VK_NULL_HANDLE;
    VkSemaphore render_finished_ = VK_NULL_HANDLE;
    VkFence in_flight_ = VK_NULL_HANDLE;

    std::uint32_t framebuffer_width_ = 0;
    std::uint32_t framebuffer_height_ = 0;
    bool initialized_ = false;

    // --- Offscreen-path-only state ------------------------------------------
    bool headless_initialized_ = false;
    static constexpr VkFormat kOffscreenFormat = VK_FORMAT_R8G8B8A8_UNORM;

    VkImage offscreen_image_ = VK_NULL_HANDLE;
    VkDeviceMemory offscreen_image_memory_ = VK_NULL_HANDLE;
    VkImageView offscreen_image_view_ = VK_NULL_HANDLE;
    VkRenderPass offscreen_render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout offscreen_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline offscreen_pipeline_ = VK_NULL_HANDLE;
    VkFramebuffer offscreen_framebuffer_ = VK_NULL_HANDLE;
    std::uint32_t offscreen_width_ = 0;
    std::uint32_t offscreen_height_ = 0;

    VkBuffer offscreen_vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory offscreen_vertex_memory_ = VK_NULL_HANDLE;
    VkBuffer offscreen_index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory offscreen_index_memory_ = VK_NULL_HANDLE;

    VkBuffer readback_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory readback_memory_ = VK_NULL_HANDLE;
    VkDeviceSize readback_size_ = 0;

    VkCommandPool headless_command_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer headless_command_buffer_ = VK_NULL_HANDLE;
    VkFence headless_fence_ = VK_NULL_HANDLE;

    // --- GPU Material Resource state ------------------------------------
    // Real ownership: this instance's material_resource_authority_ is
    // the ONE owner of every material resource's lifecycle state
    // (Phase 3's GPUResourceLifetime, this phase's
    // GPUResourceAuthority) -- never duplicated, never bypassed.
    GPUResourceAuthority material_resource_authority_;
    std::unordered_map<std::string, MaterialResourceGpuHandles> material_resource_handles_;

    VkDescriptorSetLayout material_descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool material_descriptor_pool_ = VK_NULL_HANDLE;
    VkPipelineLayout material_resource_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline material_resource_pipeline_ = VK_NULL_HANDLE;
    VkBuffer material_resource_vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory material_resource_vertex_memory_ = VK_NULL_HANDLE;
    VkBuffer material_resource_index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory material_resource_index_memory_ = VK_NULL_HANDLE;
    static constexpr std::uint32_t kMaxMaterialResources = 64;
};

}  // namespace dominus::graphics
