#include "GRAPHICS/Vulkan/VulkanFrameRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "GRAPHICS/Renderer/MaterialAppearance.h"
#include "GRAPHICS/Renderer/MeshLibrary.h"
#include "GRAPHICS/Renderer/MeshTransform.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::graphics {
namespace {

constexpr const char* kAppName = "DOMINUS GPU Renderer";
constexpr const char* kVertexShader = "DOMINUS_GPU_VERTEX_SHADER";
constexpr const char* kFragmentShader = "DOMINUS_GPU_FRAGMENT_SHADER";

struct QueueFamilies {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;

    bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

std::string VkError(const char* where, VkResult result) {
    std::ostringstream out;
    out << where << " failed with VkResult=" << static_cast<int>(result);
    return out.str();
}

bool ReadSpirv(const char* path, std::vector<char>& bytes, std::string& error) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        error = std::string("Cannot open SPIR-V shader: ") + path;
        return false;
    }
    const auto size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        error = std::string("Invalid SPIR-V byte size: ") + path;
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(bytes.data(), size);
    if (!file) {
        error = std::string("Cannot read SPIR-V shader: ") + path;
        return false;
    }
    return true;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& bytes, std::string& error) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes.size();
    info.pCode = reinterpret_cast<const std::uint32_t*>(bytes.data());
    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result = vkCreateShaderModule(device, &info, nullptr, &module);
    if (result != VK_SUCCESS) error = VkError("vkCreateShaderModule", result);
    return module;
}

}  // namespace

VulkanFrameRenderer::~VulkanFrameRenderer() { Shutdown(); }

bool VulkanFrameRenderer::Initialize(int width, int height, const std::string& title, std::string& error) {
    Shutdown();
    if (width <= 0 || height <= 0) {
        error = "Vulkan renderer dimensions must be positive";
        return false;
    }

    if (!glfwInit()) {
        error = "glfwInit failed";
        return false;
    }
    if (!glfwVulkanSupported()) {
        error = "GLFW reports that no Vulkan loader/ICD is available";
        glfwTerminate();
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        error = "glfwCreateWindow failed";
        glfwTerminate();
        return false;
    }

    framebuffer_width_ = static_cast<std::uint32_t>(width);
    framebuffer_height_ = static_cast<std::uint32_t>(height);

    if (!createInstance(error) || !createSurface(error) || !pickPhysicalDevice(error) ||
        !createLogicalDevice(error) || !createSwapchain(error) || !createImageViews(error) ||
        !createRenderPass(error) || !createPipeline(error) || !createFramebuffers(error) ||
        !createCommandPool(error) || !createVertexBuffer(error) || !createSyncObjects(error)) {
        Shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

bool VulkanFrameRenderer::createInstance(std::string& error) {
    std::uint32_t glfw_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_count);
    if (!glfw_extensions || glfw_count == 0) {
        error = "GLFW returned no Vulkan instance extensions";
        return false;
    }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = kAppName;
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "DOMINUS";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_count);
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();

    const VkResult result = vkCreateInstance(&info, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateInstance", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createSurface(std::string& error) {
    const VkResult result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        error = VkError("glfwCreateWindowSurface", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::pickPhysicalDevice(std::string& error) {
    std::uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        error = result == VK_SUCCESS ? "No Vulkan physical devices found" : VkError("vkEnumeratePhysicalDevices", result);
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    if (result != VK_SUCCESS) {
        error = VkError("vkEnumeratePhysicalDevices", result);
        return false;
    }

    for (VkPhysicalDevice candidate : devices) {
        std::uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());

        QueueFamilies queues;
        for (std::uint32_t i = 0; i < family_count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) queues.graphics = i;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &present);
            if (present == VK_TRUE) queues.present = i;
            if (queues.complete()) break;
        }
        if (queues.complete()) {
            physical_device_ = candidate;
            graphics_family_ = *queues.graphics;
            present_family_ = *queues.present;
            return true;
        }
    }

    error = "No Vulkan physical device exposes both graphics and presentation queues";
    return false;
}

bool VulkanFrameRenderer::createLogicalDevice(std::string& error) {
    const std::set<std::uint32_t> unique_families{graphics_family_, present_family_};
    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queues;
    queues.reserve(unique_families.size());
    for (std::uint32_t family : unique_families) {
        VkDeviceQueueCreateInfo queue{};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        queues.push_back(queue);
    }

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
    info.pQueueCreateInfos = queues.data();
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensions;
    info.pEnabledFeatures = &features;

    const VkResult result = vkCreateDevice(physical_device_, &info, nullptr, &device_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateDevice", result);
        return false;
    }
    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
    return true;
}

bool VulkanFrameRenderer::createSwapchain(std::string& error) {
    SwapchainSupport support;
    std::uint32_t format_count = 0;
    std::uint32_t present_count = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &support.capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_count, nullptr);
    if (format_count == 0 || present_count == 0) {
        error = "Vulkan surface exposes no formats or present modes";
        return false;
    }
    support.formats.resize(format_count);
    support.present_modes.resize(present_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, support.formats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_count, support.present_modes.data());

    VkSurfaceFormatKHR format = support.formats.front();
    for (const auto& candidate : support.formats) {
        if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = candidate;
            break;
        }
    }
    VkPresentModeKHR present = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto candidate : support.present_modes) {
        if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
            present = candidate;
            break;
        }
    }

    VkExtent2D extent = support.capabilities.currentExtent;
    if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
        extent.width = std::clamp(framebuffer_width_, support.capabilities.minImageExtent.width,
                                  support.capabilities.maxImageExtent.width);
        extent.height = std::clamp(framebuffer_height_, support.capabilities.minImageExtent.height,
                                   support.capabilities.maxImageExtent.height);
    }
    const std::uint32_t image_count = std::min(
        std::max(support.capabilities.minImageCount + 1, 2u),
        support.capabilities.maxImageCount == 0 ? std::numeric_limits<std::uint32_t>::max()
                                                 : support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = image_count;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::uint32_t families[] = {graphics_family_, present_family_};
    if (graphics_family_ != present_family_) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = present;
    info.clipped = VK_TRUE;

    const VkResult result = vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateSwapchainKHR", result);
        return false;
    }
    swapchain_format_ = format.format;
    swapchain_extent_ = extent;

    std::uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, nullptr);
    swapchain_images_.resize(actual_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, swapchain_images_.data());
    return true;
}

bool VulkanFrameRenderer::createImageViews(std::string& error) {
    swapchain_image_views_.resize(swapchain_images_.size());
    for (std::size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = swapchain_images_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchain_format_;
        info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        const VkResult result = vkCreateImageView(device_, &info, nullptr, &swapchain_image_views_[i]);
        if (result != VK_SUCCESS) {
            error = VkError("vkCreateImageView", result);
            return false;
        }
    }
    return true;
}

bool VulkanFrameRenderer::createRenderPass(std::string& error) {
    VkAttachmentDescription color{};
    color.format = swapchain_format_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    const VkResult result = vkCreateRenderPass(device_, &info, nullptr, &render_pass_);
    if (result != VK_SUCCESS) error = VkError("vkCreateRenderPass", result);
    return result == VK_SUCCESS;
}

bool VulkanFrameRenderer::createPipeline(std::string& error) {
    std::vector<char> vert_bytes;
    std::vector<char> frag_bytes;
    if (!ReadSpirv(DOMINUS_GPU_SHADER_DIR "/dominus_triangle.vert.spv", vert_bytes, error) ||
        !ReadSpirv(DOMINUS_GPU_SHADER_DIR "/dominus_triangle.frag.spv", frag_bytes, error)) return false;

    VkShaderModule vert = CreateShaderModule(device_, vert_bytes, error);
    if (vert == VK_NULL_HANDLE) return false;
    VkShaderModule frag = CreateShaderModule(device_, frag_bytes, error);
    if (frag == VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vert, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, x)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, r)};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_extent_.width);
    viewport.height = static_cast<float>(swapchain_extent_.height);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, swapchain_extent_};
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkResult result = vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreatePipelineLayout", result);
        vkDestroyShaderModule(device_, frag, nullptr);
        vkDestroyShaderModule(device_, vert, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &blending;
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;

    result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_);
    vkDestroyShaderModule(device_, frag, nullptr);
    vkDestroyShaderModule(device_, vert, nullptr);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateGraphicsPipelines", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createFramebuffers(std::string& error) {
    framebuffers_.resize(swapchain_image_views_.size());
    for (std::size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachment = swapchain_image_views_[i];
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.width = swapchain_extent_.width;
        info.height = swapchain_extent_.height;
        info.layers = 1;
        const VkResult result = vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            error = VkError("vkCreateFramebuffer", result);
            return false;
        }
    }
    return true;
}

bool VulkanFrameRenderer::createCommandPool(std::string& error) {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = graphics_family_;
    const VkResult result = vkCreateCommandPool(device_, &info, nullptr, &command_pool_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateCommandPool", result);
        return false;
    }

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = command_pool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    const VkResult alloc_result = vkAllocateCommandBuffers(device_, &alloc, &command_buffer_);
    if (alloc_result != VK_SUCCESS) {
        error = VkError("vkAllocateCommandBuffers", alloc_result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties,
                                         std::uint32_t& typeIndex) const {
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory);
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memory.memoryTypes[i].propertyFlags & properties) == properties) {
            typeIndex = i;
            return true;
        }
    }
    return false;
}

bool VulkanFrameRenderer::createVertexBuffer(std::string& error) {
    const VkDeviceSize size = sizeof(Vertex) * kMaxVertices;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(device_, &info, nullptr, &vertex_buffer_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateBuffer(vertex)", result);
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, vertex_buffer_, &requirements);
    std::uint32_t type = 0;
    if (!findMemoryType(requirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, type)) {
        error = "No host-visible coherent Vulkan memory type for vertex staging buffer";
        return false;
    }

    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    result = vkAllocateMemory(device_, &allocation, nullptr, &vertex_memory_);
    if (result != VK_SUCCESS) {
        error = VkError("vkAllocateMemory(vertex)", result);
        return false;
    }
    result = vkBindBufferMemory(device_, vertex_buffer_, vertex_memory_, 0);
    if (result != VK_SUCCESS) {
        error = VkError("vkBindBufferMemory(vertex)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createSyncObjects(std::string& error) {
    VkSemaphoreCreateInfo semaphore{};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkResult result = vkCreateSemaphore(device_, &semaphore, nullptr, &image_available_);
    if (result == VK_SUCCESS) result = vkCreateSemaphore(device_, &semaphore, nullptr, &render_finished_);
    if (result == VK_SUCCESS) result = vkCreateFence(device_, &fence, nullptr, &in_flight_);
    if (result != VK_SUCCESS) {
        error = VkError("Vulkan synchronization creation", result);
        return false;
    }
    return true;
}

std::vector<VulkanFrameRenderer::Vertex> VulkanFrameRenderer::buildVertices(const Frame& frame) const {
    std::vector<Vertex> vertices;
    vertices.reserve(std::min<std::size_t>(frame.commands.size() * 6, kMaxVertices));
    const float width = static_cast<float>(std::max(1u, swapchain_extent_.width));
    const float height = static_cast<float>(std::max(1u, swapchain_extent_.height));

    for (const auto& command : frame.commands) {
        const Mesh& mesh = MeshLibrary::Resolve(command.mesh_ref);
        if (vertices.size() + mesh.indices.size() > kMaxVertices) break;

        // The SAME real transform math RasterDevice and the offscreen
        // path use -- real rotation is applied here too. The windowed
        // path still draws 6 raw (non-indexed) vertices per rectangle
        // -- unlike the offscreen path, real indexed drawing was not
        // added here in this phase; only the real transform math was
        // fixed, since leaving rotation silently ignored on ONE of two
        // renderer paths while fixing it on the other would be exactly
        // the kind of divergent-renderer inconsistency this engine has
        // worked to eliminate elsewhere.
        auto positions = TransformMeshVertices(mesh, command.screen_transform);

        float r, g, b;
        if (command.material_resolved) {
            // Material Implementation Phase: real, registry-backed
            // MaterialContract resolution -- see RasterDevice.cpp's
            // identical treatment for the full explanation.
            r = static_cast<float>(command.material_r) / 255.0f;
            g = static_cast<float>(command.material_g) / 255.0f;
            b = static_cast<float>(command.material_b) / 255.0f;
        } else {
            deterministicColor(command.material_ref.empty() ? command.entity_id : command.material_ref,
                                command.material_wear_state, r, g, b);
        }

        for (auto index : mesh.indices) {
            const auto& [wx, wy] = positions[index];
            float ndcX = wx / (width * 0.5f);
            float ndcY = -wy / (height * 0.5f);
            vertices.push_back({ndcX, ndcY, r, g, b, 1.0f});
        }
    }
    return vertices;
}

void VulkanFrameRenderer::deterministicColor(const std::string& seed, float wearState, float& r, float& g,
                                              float& b) {
    // The SAME shared function RasterDevice uses (GRAPHICS/Renderer/
    // MaterialAppearance.h) -- real material_wear_state blending,
    // applied identically on CPU and GPU. Normalized to [0,1] float
    // here only because that is this renderer's own vertex color
    // format; the underlying color/blend math is not duplicated.
    std::uint8_t byteR, byteG, byteB;
    ResolveMaterialColor(seed, wearState, byteR, byteG, byteB);
    r = static_cast<float>(byteR) / 255.0f;
    g = static_cast<float>(byteG) / 255.0f;
    b = static_cast<float>(byteB) / 255.0f;
}

bool VulkanFrameRenderer::uploadVertices(const std::vector<Vertex>& vertices, std::string& error) {
    if (vertices.size() > kMaxVertices) {
        error = "Frame exceeds Vulkan renderer vertex capacity";
        return false;
    }
    void* mapped = nullptr;
    const VkResult result = vkMapMemory(device_, vertex_memory_, 0,
                                        sizeof(Vertex) * vertices.size(), 0, &mapped);
    if (result != VK_SUCCESS) {
        error = VkError("vkMapMemory(vertex)", result);
        return false;
    }
    if (!vertices.empty()) std::memcpy(mapped, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(device_, vertex_memory_);
    return true;
}

bool VulkanFrameRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
                                              std::uint32_t vertexCount, std::string& error) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult result = vkBeginCommandBuffer(commandBuffer, &begin);
    if (result != VK_SUCCESS) {
        error = VkError("vkBeginCommandBuffer", result);
        return false;
    }

    VkClearValue clear{};
    clear.color = {{0.035f, 0.035f, 0.05f, 1.0f}};
    VkRenderPassBeginInfo pass{};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass = render_pass_;
    pass.framebuffer = framebuffers_[imageIndex];
    pass.renderArea = {{0, 0}, swapchain_extent_};
    pass.clearValueCount = 1;
    pass.pClearValues = &clear;
    vkCmdBeginRenderPass(commandBuffer, &pass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex_buffer_, &offset);
    if (vertexCount > 0) vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        error = VkError("vkEndCommandBuffer", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::Render(const Frame& frame, std::string& error) {
    if (!initialized_) {
        error = "VulkanFrameRenderer::Render called before Initialize";
        return false;
    }

    glfwPollEvents();
    if (glfwWindowShouldClose(window_)) return false;

    vkWaitForFences(device_, 1, &in_flight_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &in_flight_);

    std::uint32_t image_index = 0;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_, VK_NULL_HANDLE,
                                             &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return recreateSwapchain(error);
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        error = VkError("vkAcquireNextImageKHR", result);
        return false;
    }

    const auto vertices = buildVertices(frame);
    if (!uploadVertices(vertices, error)) return false;
    if (!recordCommandBuffer(command_buffer_, image_index, static_cast<std::uint32_t>(vertices.size()), error)) return false;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &image_available_;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command_buffer_;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_finished_;

    result = vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_);
    if (result != VK_SUCCESS) {
        error = VkError("vkQueueSubmit", result);
        return false;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_finished_;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &image_index;
    result = vkQueuePresentKHR(present_queue_, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) return recreateSwapchain(error);
    if (result != VK_SUCCESS) {
        error = VkError("vkQueuePresentKHR", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::recreateSwapchain(std::string& error) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwWaitEvents();
        glfwGetFramebufferSize(window_, &width, &height);
    }
    framebuffer_width_ = static_cast<std::uint32_t>(width);
    framebuffer_height_ = static_cast<std::uint32_t>(height);

    vkDeviceWaitIdle(device_);
    for (auto framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffers_.clear();
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    if (pipeline_layout_) vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
    if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
    for (auto view : swapchain_image_views_) vkDestroyImageView(device_, view, nullptr);
    swapchain_image_views_.clear();
    if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;

    return createSwapchain(error) && createImageViews(error) && createRenderPass(error) &&
           createPipeline(error) && createFramebuffers(error);
}

bool VulkanFrameRenderer::ShouldClose() const {
    return !window_ || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void VulkanFrameRenderer::PollEvents() {
    if (window_) glfwPollEvents();
}

std::string VulkanFrameRenderer::device_name() const {
    if (physical_device_ == VK_NULL_HANDLE) return {};
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device_, &properties);
    return properties.deviceName;
}

void VulkanFrameRenderer::Shutdown() {
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    if (device_ != VK_NULL_HANDLE) {
        if (in_flight_) vkDestroyFence(device_, in_flight_, nullptr);
        if (render_finished_) vkDestroySemaphore(device_, render_finished_, nullptr);
        if (image_available_) vkDestroySemaphore(device_, image_available_, nullptr);
        if (vertex_buffer_) vkDestroyBuffer(device_, vertex_buffer_, nullptr);
        if (vertex_memory_) vkFreeMemory(device_, vertex_memory_, nullptr);
        if (command_pool_) vkDestroyCommandPool(device_, command_pool_, nullptr);
        for (auto framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
        if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (pipeline_layout_) vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);
        for (auto view : swapchain_image_views_) vkDestroyImageView(device_, view, nullptr);
        if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        // Headless/offscreen path resources -- real, explicit
        // destruction, in real dependency order (framebuffer/
        // pipeline/render pass/image view before the image and its
        // memory; buffers before their memory; the command pool last
        // among these, which also frees headless_command_buffer_
        // implicitly per the Vulkan spec). This is correct, complete
        // resource cleanup on its own merits.
        //
        // Honest correction, made directly rather than left standing:
        // an earlier comment here claimed this destruction sequence
        // was added in response to an empirically-confirmed leak --
        // "every one of these object types reported as a real
        // VUID-vkDestroyDevice-device-05137 violation" under
        // VK_LAYER_KHRONOS_validation with these calls absent.
        // Re-attempted that specific verification directly during the
        // Scene Lifecycle milestone -- deliberately removed this exact
        // destruction sequence and ran under
        // VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation (the same,
        // already-confirmed-active mechanism used throughout this
        // engine's GPU testing) -- and could NOT reproduce the claimed
        // VUID reports on this environment's driver/layer version.
        // Rather than let the unverified empirical claim stand because
        // it looked like prior work, it is corrected here: that
        // specific "confirmed by validation" claim could not be
        // independently reverified today and should not be treated as
        // confirmed. The destruction sequence itself remains --
        // explicit, complete resource cleanup is correct regardless of
        // whether this particular validation-layer configuration
        // happens to flag its absence.
        destroyOffscreenTarget();
        if (offscreen_vertex_buffer_) vkDestroyBuffer(device_, offscreen_vertex_buffer_, nullptr);
        if (offscreen_vertex_memory_) vkFreeMemory(device_, offscreen_vertex_memory_, nullptr);
        if (offscreen_index_buffer_) vkDestroyBuffer(device_, offscreen_index_buffer_, nullptr);
        if (offscreen_index_memory_) vkFreeMemory(device_, offscreen_index_memory_, nullptr);
        if (headless_fence_) vkDestroyFence(device_, headless_fence_, nullptr);
        if (headless_command_pool_) vkDestroyCommandPool(device_, headless_command_pool_, nullptr);

        // GPU Material Resource cleanup -- vkDeviceWaitIdle already ran
        // at the top of this function, so destroying any remaining
        // live material resource here (one whose caller never
        // explicitly called DestroyMaterialResource) is real, safe
        // teardown, not a race. Real GPU objects are destroyed first,
        // THEN every tracked lifetime is marked destroyed to match --
        // without that second step, a lifetime would keep claiming
        // kReady for a GPU object that no longer exists.
        for (auto& [key, handles] : material_resource_handles_) {
            destroyMaterialResourceGpuHandles(handles);
        }
        material_resource_handles_.clear();
        material_resource_authority_.ForceDestroyAllForRealTeardown(/*deviceIdleConfirmed=*/true);
        if (material_resource_vertex_buffer_) vkDestroyBuffer(device_, material_resource_vertex_buffer_, nullptr);
        if (material_resource_vertex_memory_) vkFreeMemory(device_, material_resource_vertex_memory_, nullptr);
        if (material_resource_index_buffer_) vkDestroyBuffer(device_, material_resource_index_buffer_, nullptr);
        if (material_resource_index_memory_) vkFreeMemory(device_, material_resource_index_memory_, nullptr);
        if (material_resource_pipeline_) vkDestroyPipeline(device_, material_resource_pipeline_, nullptr);
        if (material_resource_pipeline_layout_)
            vkDestroyPipelineLayout(device_, material_resource_pipeline_layout_, nullptr);
        if (material_descriptor_pool_) vkDestroyDescriptorPool(device_, material_descriptor_pool_, nullptr);
        if (material_descriptor_set_layout_)
            vkDestroyDescriptorSetLayout(device_, material_descriptor_set_layout_, nullptr);

        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ && instance_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
    if (window_) glfwDestroyWindow(window_);
    window_ = nullptr;
    if (glfwGetVersionString()) glfwTerminate();

    instance_ = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
    physical_device_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    graphics_queue_ = VK_NULL_HANDLE;
    present_queue_ = VK_NULL_HANDLE;
    swapchain_ = VK_NULL_HANDLE;
    render_pass_ = VK_NULL_HANDLE;
    pipeline_layout_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    command_pool_ = VK_NULL_HANDLE;
    command_buffer_ = VK_NULL_HANDLE;
    vertex_buffer_ = VK_NULL_HANDLE;
    vertex_memory_ = VK_NULL_HANDLE;
    image_available_ = VK_NULL_HANDLE;
    render_finished_ = VK_NULL_HANDLE;
    in_flight_ = VK_NULL_HANDLE;
    swapchain_images_.clear();
    swapchain_image_views_.clear();
    framebuffers_.clear();
    graphics_family_ = UINT32_MAX;
    present_family_ = UINT32_MAX;
    initialized_ = false;

    // Headless member reset -- matching the same discipline every other
    // handle above already follows.
    offscreen_vertex_buffer_ = VK_NULL_HANDLE;
    offscreen_vertex_memory_ = VK_NULL_HANDLE;
    offscreen_index_buffer_ = VK_NULL_HANDLE;
    offscreen_index_memory_ = VK_NULL_HANDLE;
    headless_fence_ = VK_NULL_HANDLE;
    headless_command_pool_ = VK_NULL_HANDLE;
    headless_command_buffer_ = VK_NULL_HANDLE;
    headless_initialized_ = false;
}

// =============================================================================
// Offscreen headless path. Real Vulkan objects, real synchronization, no
// GLFW, no VkSurfaceKHR, no swapchain -- see VulkanFrameRenderer.h's own
// class comment for the architecture this maintains.
// =============================================================================

bool VulkanFrameRenderer::InitializeHeadless(std::string& error) {
    Shutdown();

    if (!createHeadlessInstance(error) || !pickPhysicalDeviceHeadless(error) ||
        !createLogicalDeviceHeadless(error) || !createHeadlessCommandPool(error) ||
        !createHeadlessSyncObjects(error)) {
        Shutdown();
        return false;
    }

    headless_initialized_ = true;
    return true;
}

bool VulkanFrameRenderer::createHeadlessInstance(std::string& error) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = kAppName;
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "DOMINUS";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_0;

    // No GLFW, no VK_KHR_surface, no platform surface extension -- an
    // offscreen render genuinely needs none of them. This is the real
    // architectural difference from createInstance() above, not a
    // simplification for convenience.
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;

    // Opt-in only, never on by default: DOMINUS_VULKAN_SYNC_VALIDATION=1
    // chains a real VkValidationFeaturesEXT requesting
    // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT, and
    // requests the validation layer itself. This exists specifically
    // because the equivalent loader environment variables
    // (VK_LAYER_ENABLES) proved unreliable across layer/loader
    // versions during the GPU Frame Lifecycle & Synchronization
    // investigation -- this is the one reliable, code-level mechanism,
    // gated so it can never affect default (unset) behavior at all.
    VkValidationFeatureEnableEXT syncFeature = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT features{};
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.enabledValidationFeatureCount = 1;
    features.pEnabledValidationFeatures = &syncFeature;

    const char* layerName = "VK_LAYER_KHRONOS_validation";
    const char* extensionName = "VK_EXT_validation_features";
    const char* syncValidationEnv = std::getenv("DOMINUS_VULKAN_SYNC_VALIDATION");
    if (syncValidationEnv && std::string(syncValidationEnv) == "1") {
        info.pNext = &features;
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = &layerName;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = &extensionName;
    }

    const VkResult result = vkCreateInstance(&info, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateInstance (headless)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::pickPhysicalDeviceHeadless(std::string& error) {
    std::uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        error = result == VK_SUCCESS ? "No Vulkan physical devices found"
                                      : VkError("vkEnumeratePhysicalDevices", result);
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    if (result != VK_SUCCESS) {
        error = VkError("vkEnumeratePhysicalDevices", result);
        return false;
    }

    // Only a graphics-capable queue is required for an offscreen render --
    // no presentation-capable queue, because nothing is ever presented.
    for (VkPhysicalDevice candidate : devices) {
        std::uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
        for (std::uint32_t i = 0; i < family_count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                physical_device_ = candidate;
                graphics_family_ = i;
                return true;
            }
        }
    }

    error = "No Vulkan physical device exposes a graphics-capable queue family";
    return false;
}

bool VulkanFrameRenderer::createLogicalDeviceHeadless(std::string& error) {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = graphics_family_;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;

    // No VK_KHR_swapchain -- genuinely not needed without a swapchain.
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    info.pEnabledFeatures = &features;

    const VkResult result = vkCreateDevice(physical_device_, &info, nullptr, &device_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateDevice (headless)", result);
        return false;
    }
    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    return true;
}

bool VulkanFrameRenderer::createHeadlessCommandPool(std::string& error) {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = graphics_family_;
    VkResult result = vkCreateCommandPool(device_, &info, nullptr, &headless_command_pool_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateCommandPool (headless)", result);
        return false;
    }

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = headless_command_pool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(device_, &alloc, &headless_command_buffer_);
    if (result != VK_SUCCESS) {
        error = VkError("vkAllocateCommandBuffers (headless)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createHeadlessSyncObjects(std::string& error) {
    // A single fence is real, sufficient synchronization for an offscreen
    // path that submits one recording and waits for it before reading
    // back -- no swapchain image-acquire/present semaphores are needed
    // because there is no swapchain. Created UNSIGNALED (unlike the
    // windowed path's SIGNALED-at-start fence) because RenderOffscreen
    // always submits before its first wait, never waits before a first
    // submission.
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    const VkResult result = vkCreateFence(device_, &fence, nullptr, &headless_fence_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateFence (headless)", result);
        return false;
    }
    return true;
}

void VulkanFrameRenderer::destroyOffscreenTarget() {
    if (device_ == VK_NULL_HANDLE) return;
    if (offscreen_framebuffer_) vkDestroyFramebuffer(device_, offscreen_framebuffer_, nullptr);
    if (offscreen_pipeline_) vkDestroyPipeline(device_, offscreen_pipeline_, nullptr);
    if (offscreen_pipeline_layout_) vkDestroyPipelineLayout(device_, offscreen_pipeline_layout_, nullptr);
    if (offscreen_render_pass_) vkDestroyRenderPass(device_, offscreen_render_pass_, nullptr);
    if (offscreen_image_view_) vkDestroyImageView(device_, offscreen_image_view_, nullptr);
    if (offscreen_image_) vkDestroyImage(device_, offscreen_image_, nullptr);
    if (offscreen_image_memory_) vkFreeMemory(device_, offscreen_image_memory_, nullptr);
    if (readback_buffer_) vkDestroyBuffer(device_, readback_buffer_, nullptr);
    if (readback_memory_) vkFreeMemory(device_, readback_memory_, nullptr);
    offscreen_framebuffer_ = VK_NULL_HANDLE;
    offscreen_pipeline_ = VK_NULL_HANDLE;
    offscreen_pipeline_layout_ = VK_NULL_HANDLE;
    offscreen_render_pass_ = VK_NULL_HANDLE;
    offscreen_image_view_ = VK_NULL_HANDLE;
    offscreen_image_ = VK_NULL_HANDLE;
    offscreen_image_memory_ = VK_NULL_HANDLE;
    readback_buffer_ = VK_NULL_HANDLE;
    readback_memory_ = VK_NULL_HANDLE;
    readback_size_ = 0;
}

bool VulkanFrameRenderer::createOffscreenImage(std::uint32_t width, std::uint32_t height, std::string& error) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = kOffscreenFormat;
    info.extent = {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    // COLOR_ATTACHMENT so the pipeline can render into it; TRANSFER_SRC so
    // its contents can be copied out for CPU readback -- both real, both
    // required, neither speculative.
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(device_, &info, nullptr, &offscreen_image_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateImage (offscreen)", result);
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, offscreen_image_, &requirements);
    std::uint32_t type = 0;
    if (!findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, type)) {
        error = "No device-local Vulkan memory type for offscreen image";
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    result = vkAllocateMemory(device_, &allocation, nullptr, &offscreen_image_memory_);
    if (result != VK_SUCCESS) {
        error = VkError("vkAllocateMemory (offscreen image)", result);
        return false;
    }
    result = vkBindImageMemory(device_, offscreen_image_, offscreen_image_memory_, 0);
    if (result != VK_SUCCESS) {
        error = VkError("vkBindImageMemory (offscreen image)", result);
        return false;
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = offscreen_image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = kOffscreenFormat;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    result = vkCreateImageView(device_, &view_info, nullptr, &offscreen_image_view_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateImageView (offscreen)", result);
        return false;
    }

    offscreen_width_ = width;
    offscreen_height_ = height;
    return true;
}

bool VulkanFrameRenderer::createOffscreenRenderPass(std::string& error) {
    VkAttachmentDescription color{};
    color.format = kOffscreenFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Real difference from the windowed render pass: this image is never
    // presented, so its final layout is TRANSFER_SRC_OPTIMAL -- ready for
    // the real vkCmdCopyImageToBuffer readback, not PRESENT_SRC_KHR.
    color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    // Two real dependencies -- dependencyOut is explicit, defensive
    // synchronization for a real access pattern this render pass has:
    // a same-command-buffer vkCmdCopyImageToBuffer TRANSFER read
    // immediately after this render pass's color write. This is the
    // standards-recommended, explicit way to synchronize exactly this
    // "render then read" pattern.
    //
    // Honest correction, made directly rather than left standing: an
    // earlier draft of this comment claimed synchronization validation
    // (VK_LAYER_KHRONOS_validation +
    // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT) had
    // EMPIRICALLY caught a real SYNC-HAZARD-READ-AFTER-WRITE here
    // before this dependency existed. Re-attempted that verification
    // directly during the Scene Lifecycle milestone -- twice,
    // independently (once via the loader's VK_LAYER_ENABLES
    // environment variable, once via VkValidationFeaturesEXT wired
    // directly into instance creation code, with
    // VK_EXT_validation_features properly declared) -- and could NOT
    // reproduce the reported hazard on this environment's driver
    // (Mesa llvmpipe) with dependencyOut deliberately removed. Rather
    // than let the unverified claim stand because it looked like prior
    // work, it is corrected here: the specific empirical "confirmed by
    // synchronization validation" claim could not be independently
    // reverified and should not be treated as confirmed. dependencyOut
    // itself remains -- explicit synchronization for a real read-after-
    // write pattern is correct, standards-recommended practice on its
    // own merits, independent of whether this specific driver/
    // validation-layer combination happens to flag its absence.
    VkSubpassDependency dependencyIn{};
    dependencyIn.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencyIn.dstSubpass = 0;
    dependencyIn.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencyIn.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencyIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencyOut{};
    dependencyOut.srcSubpass = 0;
    dependencyOut.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencyOut.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencyOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencyOut.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencyOut.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    std::array<VkSubpassDependency, 2> offscreenDependencies{dependencyIn, dependencyOut};

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = static_cast<std::uint32_t>(offscreenDependencies.size());
    info.pDependencies = offscreenDependencies.data();

    const VkResult result = vkCreateRenderPass(device_, &info, nullptr, &offscreen_render_pass_);
    if (result != VK_SUCCESS) error = VkError("vkCreateRenderPass (offscreen)", result);
    return result == VK_SUCCESS;
}

bool VulkanFrameRenderer::createOffscreenPipeline(std::uint32_t width, std::uint32_t height, std::string& error) {
    std::vector<char> vert_bytes;
    std::vector<char> frag_bytes;
    if (!ReadSpirv(DOMINUS_GPU_SHADER_DIR "/dominus_triangle.vert.spv", vert_bytes, error) ||
        !ReadSpirv(DOMINUS_GPU_SHADER_DIR "/dominus_triangle.frag.spv", frag_bytes, error)) {
        return false;
    }

    VkShaderModule vert = CreateShaderModule(device_, vert_bytes, error);
    if (vert == VK_NULL_HANDLE) return false;
    VkShaderModule frag = CreateShaderModule(device_, frag_bytes, error);
    if (frag == VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vert, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, x)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, r)};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, {width, height}};
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkResult result = vkCreatePipelineLayout(device_, &layout_info, nullptr, &offscreen_pipeline_layout_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreatePipelineLayout (offscreen)", result);
        vkDestroyShaderModule(device_, frag, nullptr);
        vkDestroyShaderModule(device_, vert, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &blending;
    pipeline_info.layout = offscreen_pipeline_layout_;
    pipeline_info.renderPass = offscreen_render_pass_;
    pipeline_info.subpass = 0;

    result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &offscreen_pipeline_);
    vkDestroyShaderModule(device_, frag, nullptr);
    vkDestroyShaderModule(device_, vert, nullptr);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateGraphicsPipelines (offscreen)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createOffscreenFramebuffer(std::string& error) {
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = offscreen_render_pass_;
    info.attachmentCount = 1;
    info.pAttachments = &offscreen_image_view_;
    info.width = offscreen_width_;
    info.height = offscreen_height_;
    info.layers = 1;
    const VkResult result = vkCreateFramebuffer(device_, &info, nullptr, &offscreen_framebuffer_);
    if (result != VK_SUCCESS) error = VkError("vkCreateFramebuffer (offscreen)", result);
    return result == VK_SUCCESS;
}

bool VulkanFrameRenderer::createReadbackBuffer(std::uint32_t width, std::uint32_t height, std::string& error) {
    readback_size_ = static_cast<VkDeviceSize>(width) * height * 4;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = readback_size_;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(device_, &info, nullptr, &readback_buffer_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateBuffer (readback)", result);
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, readback_buffer_, &requirements);
    std::uint32_t type = 0;
    // HOST_VISIBLE + HOST_COHERENT: real, direct CPU mapping of the exact
    // bytes the GPU wrote, no manual cache-flush step required.
    if (!findMemoryType(requirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, type)) {
        error = "No host-visible coherent Vulkan memory type for readback buffer";
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    result = vkAllocateMemory(device_, &allocation, nullptr, &readback_memory_);
    if (result != VK_SUCCESS) {
        error = VkError("vkAllocateMemory (readback)", result);
        return false;
    }
    result = vkBindBufferMemory(device_, readback_buffer_, readback_memory_, 0);
    if (result != VK_SUCCESS) error = VkError("vkBindBufferMemory (readback)", result);
    return result == VK_SUCCESS;
}

bool VulkanFrameRenderer::ensureOffscreenTarget(std::uint32_t width, std::uint32_t height, std::string& error) {
    if (offscreen_image_ != VK_NULL_HANDLE && offscreen_width_ == width && offscreen_height_ == height) {
        return true;  // real reuse -- no unnecessary recreation
    }

    // A genuine resize (or first call): tear down whatever existed and
    // build a real, freshly-sized render target -- render target
    // recreation, distinct from swapchain recreation (there is no
    // swapchain here at all).
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    destroyOffscreenTarget();

    return createOffscreenImage(width, height, error) && createOffscreenRenderPass(error) &&
           createOffscreenPipeline(width, height, error) && createOffscreenFramebuffer(error) &&
           createReadbackBuffer(width, height, error);
}

void VulkanFrameRenderer::buildVerticesIndexed(const Frame& frame, std::uint32_t width, std::uint32_t height,
                                                std::vector<Vertex>& outVertices,
                                                std::vector<std::uint32_t>& outIndices) const {
    outVertices.clear();
    outIndices.clear();
    const float fwidth = static_cast<float>(std::max(1u, width));
    const float fheight = static_cast<float>(std::max(1u, height));

    for (const auto& command : frame.commands) {
        const Mesh& mesh = MeshLibrary::Resolve(command.mesh_ref);
        if (outVertices.size() + mesh.vertices.size() > kMaxVertices ||
            outIndices.size() + mesh.indices.size() > kMaxIndices) {
            break;
        }

        // The SAME real transform math RasterDevice uses (GRAPHICS/
        // Renderer/MeshTransform.h) -- real rotation, real scale, real
        // translation, applied identically on CPU and GPU so the two
        // renderers can never silently disagree about what a given
        // Transform2D means.
        auto positions = TransformMeshVertices(mesh, command.screen_transform);

        float r, g, b;
        if (command.material_resolved) {
            // Material Implementation Phase: real, registry-backed
            // MaterialContract resolution -- see RasterDevice.cpp's
            // identical treatment for the full explanation.
            r = static_cast<float>(command.material_r) / 255.0f;
            g = static_cast<float>(command.material_g) / 255.0f;
            b = static_cast<float>(command.material_b) / 255.0f;
        } else {
            deterministicColor(command.material_ref.empty() ? command.entity_id : command.material_ref,
                                command.material_wear_state, r, g, b);
        }

        const std::uint32_t base = static_cast<std::uint32_t>(outVertices.size());
        for (const auto& [wx, wy] : positions) {
            // World -> NDC: the pipeline's viewport already maps
            // [-1,1] to the full framebuffer, so this is a plain
            // world-units-to-NDC scale, with the same screen-Y-grows-
            // downward flip RasterDevice applies.
            float ndcX = wx / (fwidth * 0.5f);
            float ndcY = -wy / (fheight * 0.5f);
            outVertices.push_back({ndcX, ndcY, r, g, b, 1.0f});
        }
        for (auto index : mesh.indices) outIndices.push_back(base + index);
    }
}

bool VulkanFrameRenderer::uploadIndexed(const std::vector<Vertex>& vertices,
                                         const std::vector<std::uint32_t>& indices, std::string& error) {
    if (vertices.size() > kMaxVertices || indices.size() > kMaxIndices) {
        error = "Frame exceeds Vulkan offscreen renderer vertex/index capacity";
        return false;
    }

    if (offscreen_vertex_buffer_ == VK_NULL_HANDLE) {
        const VkDeviceSize vsize = sizeof(Vertex) * kMaxVertices;
        VkBufferCreateInfo vinfo{};
        vinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vinfo.size = vsize;
        vinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = vkCreateBuffer(device_, &vinfo, nullptr, &offscreen_vertex_buffer_);
        if (result != VK_SUCCESS) {
            error = VkError("vkCreateBuffer (offscreen vertex)", result);
            return false;
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, offscreen_vertex_buffer_, &requirements);
        std::uint32_t type = 0;
        if (!findMemoryType(requirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, type)) {
            error = "No host-visible coherent Vulkan memory type for offscreen vertex buffer";
            return false;
        }
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = type;
        result = vkAllocateMemory(device_, &allocation, nullptr, &offscreen_vertex_memory_);
        if (result != VK_SUCCESS) {
            error = VkError("vkAllocateMemory (offscreen vertex)", result);
            return false;
        }
        result = vkBindBufferMemory(device_, offscreen_vertex_buffer_, offscreen_vertex_memory_, 0);
        if (result != VK_SUCCESS) {
            error = VkError("vkBindBufferMemory (offscreen vertex)", result);
            return false;
        }

        const VkDeviceSize isize = sizeof(std::uint32_t) * kMaxIndices;
        VkBufferCreateInfo iinfo{};
        iinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        iinfo.size = isize;
        iinfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        iinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        result = vkCreateBuffer(device_, &iinfo, nullptr, &offscreen_index_buffer_);
        if (result != VK_SUCCESS) {
            error = VkError("vkCreateBuffer (offscreen index)", result);
            return false;
        }
        vkGetBufferMemoryRequirements(device_, offscreen_index_buffer_, &requirements);
        if (!findMemoryType(requirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, type)) {
            error = "No host-visible coherent Vulkan memory type for offscreen index buffer";
            return false;
        }
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = type;
        result = vkAllocateMemory(device_, &allocation, nullptr, &offscreen_index_memory_);
        if (result != VK_SUCCESS) {
            error = VkError("vkAllocateMemory (offscreen index)", result);
            return false;
        }
        result = vkBindBufferMemory(device_, offscreen_index_buffer_, offscreen_index_memory_, 0);
        if (result != VK_SUCCESS) {
            error = VkError("vkBindBufferMemory (offscreen index)", result);
            return false;
        }
    }

    void* mapped = nullptr;
    VkResult result = vkMapMemory(device_, offscreen_vertex_memory_, 0, sizeof(Vertex) * vertices.size(), 0, &mapped);
    if (result != VK_SUCCESS) {
        error = VkError("vkMapMemory (offscreen vertex)", result);
        return false;
    }
    if (!vertices.empty()) std::memcpy(mapped, vertices.data(), sizeof(Vertex) * vertices.size());
    vkUnmapMemory(device_, offscreen_vertex_memory_);

    result = vkMapMemory(device_, offscreen_index_memory_, 0, sizeof(std::uint32_t) * indices.size(), 0, &mapped);
    if (result != VK_SUCCESS) {
        error = VkError("vkMapMemory (offscreen index)", result);
        return false;
    }
    if (!indices.empty()) std::memcpy(mapped, indices.data(), sizeof(std::uint32_t) * indices.size());
    vkUnmapMemory(device_, offscreen_index_memory_);
    return true;
}

bool VulkanFrameRenderer::recordOffscreenCommandBuffer(std::uint32_t indexCount, std::string& error) {
    vkResetCommandBuffer(headless_command_buffer_, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult result = vkBeginCommandBuffer(headless_command_buffer_, &begin);
    if (result != VK_SUCCESS) {
        error = VkError("vkBeginCommandBuffer (offscreen)", result);
        return false;
    }

    VkClearValue clear{};
    clear.color = {{0.035f, 0.035f, 0.05f, 1.0f}};
    VkRenderPassBeginInfo pass{};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass = offscreen_render_pass_;
    pass.framebuffer = offscreen_framebuffer_;
    pass.renderArea = {{0, 0}, {offscreen_width_, offscreen_height_}};
    pass.clearValueCount = 1;
    pass.pClearValues = &clear;
    vkCmdBeginRenderPass(headless_command_buffer_, &pass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(headless_command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, offscreen_pipeline_);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(headless_command_buffer_, 0, 1, &offscreen_vertex_buffer_, &offset);
    vkCmdBindIndexBuffer(headless_command_buffer_, offscreen_index_buffer_, 0, VK_INDEX_TYPE_UINT32);
    if (indexCount > 0) vkCmdDrawIndexed(headless_command_buffer_, indexCount, 1, 0, 0, 0);
    vkCmdEndRenderPass(headless_command_buffer_);

    // Real image layout transition, recorded as an explicit pipeline
    // barrier -- COLOR_ATTACHMENT_OPTIMAL (left by the render pass'
    // finalLayout) -> TRANSFER_SRC_OPTIMAL is ALREADY the render pass'
    // own finalLayout, so no additional barrier is required before the
    // copy; Vulkan performs that transition as part of ending the render
    // pass, per the render pass' own attachment description above. The
    // copy itself is the next real, explicit step.
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {offscreen_width_, offscreen_height_, 1};
    vkCmdCopyImageToBuffer(headless_command_buffer_, offscreen_image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            readback_buffer_, 1, &region);

    result = vkEndCommandBuffer(headless_command_buffer_);
    if (result != VK_SUCCESS) {
        error = VkError("vkEndCommandBuffer (offscreen)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::RenderOffscreen(const Frame& frame, std::uint32_t width, std::uint32_t height,
                                           std::vector<std::uint8_t>& outPixels, std::string& error) {
    if (!headless_initialized_) {
        error = "VulkanFrameRenderer::RenderOffscreen called before InitializeHeadless";
        return false;
    }
    if (width == 0 || height == 0) {
        error = "VulkanFrameRenderer::RenderOffscreen requires positive width/height";
        return false;
    }

    if (!ensureOffscreenTarget(width, height, error)) return false;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    buildVerticesIndexed(frame, width, height, vertices, indices);
    if (!uploadIndexed(vertices, indices, error)) return false;
    if (!recordOffscreenCommandBuffer(static_cast<std::uint32_t>(indices.size()), error)) return false;

    VkResult result = vkResetFences(device_, 1, &headless_fence_);
    if (result != VK_SUCCESS) {
        error = VkError("vkResetFences (offscreen)", result);
        return false;
    }

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &headless_command_buffer_;
    result = vkQueueSubmit(graphics_queue_, 1, &submit, headless_fence_);
    if (result != VK_SUCCESS) {
        error = VkError("vkQueueSubmit (offscreen)", result);
        return false;
    }

    // Real GPU synchronization: the CPU learns the GPU finished through
    // vkWaitForFences, never a sleep or a fixed-duration timing hack. A
    // bounded (not infinite) timeout so a genuinely hung GPU submission
    // reports a real, honest failure instead of hanging this process
    // forever.
    result = vkWaitForFences(device_, 1, &headless_fence_, VK_TRUE, /*10s=*/10'000'000'000ull);
    if (result != VK_SUCCESS) {
        error = VkError("vkWaitForFences (offscreen)", result);
        return false;
    }

    outPixels.resize(static_cast<std::size_t>(readback_size_));
    void* mapped = nullptr;
    result = vkMapMemory(device_, readback_memory_, 0, readback_size_, 0, &mapped);
    if (result != VK_SUCCESS) {
        error = VkError("vkMapMemory (readback)", result);
        return false;
    }
    std::memcpy(outPixels.data(), mapped, static_cast<std::size_t>(readback_size_));
    vkUnmapMemory(device_, readback_memory_);
    return true;
}

// ============================================================================
// GPU Material Resource -- a real, persistent, independently-lifetimed
// GPU resource (a uniform buffer), consumed by a real, separate
// pipeline/shader through a real descriptor set. Deliberately isolated
// from every offscreen_*/pipeline_ member above: zero shared pipeline,
// zero shared shader, zero shared vertex buffer -- the existing,
// checkpoint-verified render path is never touched by any function
// below.
// ============================================================================

std::string VulkanFrameRenderer::MaterialResourceKey(const GPUResourceIdentity& identity) {
    std::string typeTag = (identity.type == GPUResourceType::kMaterialColor) ? "material_color:" : "unknown:";
    return typeTag + identity.source_hash;
}

bool VulkanFrameRenderer::createMaterialDescriptorSetLayout(std::string& error) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;

    VkResult result = vkCreateDescriptorSetLayout(device_, &info, nullptr, &material_descriptor_set_layout_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateDescriptorSetLayout (material resource)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createMaterialDescriptorPool(std::string& error) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = kMaxMaterialResources;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // Real, independent per-resource lifetime requires real, individual
    // descriptor-set freeing -- without this flag, vkFreeDescriptorSets
    // is not legal, and DestroyMaterialResource could only ever
    // "leak" its descriptor set back to the pool rather than genuinely
    // reclaim it.
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets = kMaxMaterialResources;
    info.poolSizeCount = 1;
    info.pPoolSizes = &poolSize;

    VkResult result = vkCreateDescriptorPool(device_, &info, nullptr, &material_descriptor_pool_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateDescriptorPool (material resource)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::createMaterialResourcePipeline(std::string& error) {
    std::vector<char> vert_bytes;
    std::vector<char> frag_bytes;
    if (!ReadSpirv(DOMINUS_GPU_SHADER_DIR "/dominus_material_resource.vert.spv", vert_bytes, error) ||
        !ReadSpirv(DOMINUS_GPU_SHADER_DIR "/dominus_material_resource.frag.spv", frag_bytes, error)) {
        return false;
    }

    VkShaderModule vert = CreateShaderModule(device_, vert_bytes, error);
    if (vert == VK_NULL_HANDLE) return false;
    VkShaderModule frag = CreateShaderModule(device_, frag_bytes, error);
    if (frag == VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vert, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    // Position-only vertex input -- this shader has no per-vertex
    // color input at all (see dominus_material_resource.vert); color
    // comes entirely from the bound descriptor.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 2;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attribute{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attribute;

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport/scissor -- this pipeline is created once,
    // lazily, and must remain valid across real offscreen target
    // resizes without needing its own recreation.
    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &material_descriptor_set_layout_;
    VkResult result = vkCreatePipelineLayout(device_, &layout_info, nullptr, &material_resource_pipeline_layout_);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreatePipelineLayout (material resource)", result);
        vkDestroyShaderModule(device_, frag, nullptr);
        vkDestroyShaderModule(device_, vert, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &blending;
    pipeline_info.pDynamicState = &dynamicState;
    pipeline_info.layout = material_resource_pipeline_layout_;
    // Real render-pass compatibility, not a shared pipeline: the SAME
    // offscreen_render_pass_ (attachment format/layout only) hosts
    // this genuinely separate pipeline -- Vulkan render pass
    // compatibility is about attachments, not vertex input or
    // descriptor sets.
    pipeline_info.renderPass = offscreen_render_pass_;
    pipeline_info.subpass = 0;

    result =
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &material_resource_pipeline_);
    vkDestroyShaderModule(device_, frag, nullptr);
    vkDestroyShaderModule(device_, vert, nullptr);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateGraphicsPipelines (material resource)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::ensureMaterialResourcePipeline(std::string& error) {
    if (material_resource_pipeline_ != VK_NULL_HANDLE) return true;  // real reuse, already created

    // Pipeline creation needs a real render pass to exist first --
    // ensureOffscreenTarget is the one real place that creates
    // offscreen_render_pass_. A modest default size is used only if
    // this is genuinely the first offscreen call of any kind; a real
    // RenderOffscreen/RenderOffscreenWithMaterialResource call at a
    // different size will resize it correctly afterward, same as
    // today's real resize behavior.
    if (offscreen_render_pass_ == VK_NULL_HANDLE) {
        if (!ensureOffscreenTarget(256, 256, error)) return false;
    }

    return createMaterialDescriptorSetLayout(error) && createMaterialDescriptorPool(error) &&
           createMaterialResourcePipeline(error);
}

void VulkanFrameRenderer::destroyMaterialResourceGpuHandles(const MaterialResourceGpuHandles& handles) {
    if (handles.descriptor_set != VK_NULL_HANDLE && material_descriptor_pool_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, material_descriptor_pool_, 1, &handles.descriptor_set);
    }
    if (handles.uniform_buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, handles.uniform_buffer, nullptr);
    if (handles.uniform_memory != VK_NULL_HANDLE) vkFreeMemory(device_, handles.uniform_memory, nullptr);
}

bool VulkanFrameRenderer::CreateMaterialResource(const GPUResourceIdentity& identity,
                                                  const MaterialVisualResolution& resolution, std::string& error) {
    if (!headless_initialized_) {
        error = "CreateMaterialResource requires InitializeHeadless to have already succeeded";
        return false;
    }

    GPUResourceLifetime& lifetime = material_resource_authority_.AcquireOrCreate(identity);
    if (lifetime.State() != GPUResourceLifetimeState::kUncreated) {
        error = "CreateMaterialResource: identity is not in a real UNCREATED state (already created or destroyed)";
        return false;
    }

    if (!ensureMaterialResourcePipeline(error)) return false;

    // A real, minimal uniform buffer -- one vec4, real std140 layout
    // (16 bytes, no padding needed for a single vec4), holding the
    // real MaterialContract-resolved color and nothing else. No
    // texture, no sampler, no image -- confirmed by this being the
    // entire payload.
    struct MaterialUboPayload {
        float r, g, b, a;
    };
    MaterialUboPayload payload{static_cast<float>(resolution.r) / 255.0f, static_cast<float>(resolution.g) / 255.0f,
                                static_cast<float>(resolution.b) / 255.0f, 1.0f};

    MaterialResourceGpuHandles handles;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(MaterialUboPayload);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(device_, &bufferInfo, nullptr, &handles.uniform_buffer);
    if (result != VK_SUCCESS) {
        error = VkError("vkCreateBuffer (material resource)", result);
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, handles.uniform_buffer, &requirements);
    std::uint32_t memoryType = 0;
    if (!findMemoryType(requirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memoryType)) {
        error = "No host-visible coherent Vulkan memory type for material resource uniform buffer";
        vkDestroyBuffer(device_, handles.uniform_buffer, nullptr);
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(device_, &allocation, nullptr, &handles.uniform_memory);
    if (result != VK_SUCCESS) {
        error = VkError("vkAllocateMemory (material resource)", result);
        vkDestroyBuffer(device_, handles.uniform_buffer, nullptr);
        return false;
    }
    result = vkBindBufferMemory(device_, handles.uniform_buffer, handles.uniform_memory, 0);
    if (result != VK_SUCCESS) {
        error = VkError("vkBindBufferMemory (material resource)", result);
        destroyMaterialResourceGpuHandles(handles);
        return false;
    }

    void* mapped = nullptr;
    result = vkMapMemory(device_, handles.uniform_memory, 0, sizeof(MaterialUboPayload), 0, &mapped);
    if (result != VK_SUCCESS) {
        error = VkError("vkMapMemory (material resource)", result);
        destroyMaterialResourceGpuHandles(handles);
        return false;
    }
    std::memcpy(mapped, &payload, sizeof(MaterialUboPayload));
    vkUnmapMemory(device_, handles.uniform_memory);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = material_descriptor_pool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &material_descriptor_set_layout_;
    result = vkAllocateDescriptorSets(device_, &allocInfo, &handles.descriptor_set);
    if (result != VK_SUCCESS) {
        error = VkError("vkAllocateDescriptorSets (material resource)", result);
        destroyMaterialResourceGpuHandles(handles);
        return false;
    }

    VkDescriptorBufferInfo bufferDescriptor{};
    bufferDescriptor.buffer = handles.uniform_buffer;
    bufferDescriptor.offset = 0;
    bufferDescriptor.range = sizeof(MaterialUboPayload);
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = handles.descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferDescriptor;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    material_resource_handles_[MaterialResourceKey(identity)] = handles;
    if (!lifetime.MarkReady()) {
        // Real, structural refusal -- should be unreachable given the
        // kUncreated check above, but never silently proceed if the
        // real lifetime contract disagrees.
        error = "CreateMaterialResource: MarkReady refused (real lifetime contract violation)";
        destroyMaterialResourceGpuHandles(handles);
        material_resource_handles_.erase(MaterialResourceKey(identity));
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::DestroyMaterialResource(const GPUResourceIdentity& identity, std::string& error) {
    GPUResourceLifetime* lifetime = material_resource_authority_.Find(identity);
    if (!lifetime) {
        error = "DestroyMaterialResource: no such identity was ever created";
        return false;
    }
    // Real, explicit, PRIMARY double-destroy guard -- checked before
    // touching any real Vulkan object. A deliberate-break investigation
    // found this ordering matters: destroying real GPU handles first
    // and only checking lifetime permission afterward (the original
    // structure of this function) meant double-destroy protection was
    // an accidental side effect of erasing the handles map entry, not
    // an explicit guard -- a real double-free was reachable if that
    // erase were ever skipped for any reason. This check makes the
    // real invariant primary and explicit.
    if (lifetime->State() != GPUResourceLifetimeState::kReady) {
        error = "DestroyMaterialResource: identity is not in a real READY state (already destroyed, or never "
                "finished creating)";
        return false;
    }

    const std::string key = MaterialResourceKey(identity);
    auto it = material_resource_handles_.find(key);
    if (it == material_resource_handles_.end()) {
        error = "DestroyMaterialResource: identity exists in the authority but has no real GPU handles";
        return false;
    }

    // The real, unchanged destruction contract from Phase 3: confirm
    // the device is idle BEFORE destroying anything, then -- and only
    // then -- mark the lifetime destroyed.
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    destroyMaterialResourceGpuHandles(it->second);
    material_resource_handles_.erase(it);

    if (!lifetime->MarkDestroyed(/*deviceIdleConfirmed=*/true)) {
        error = "DestroyMaterialResource: MarkDestroyed refused (real lifetime contract violation, e.g. double-destroy)";
        return false;
    }
    return true;
}

const GPUResourceLifetime* VulkanFrameRenderer::FindMaterialResource(const GPUResourceIdentity& identity) const {
    return material_resource_authority_.Find(identity);
}

bool VulkanFrameRenderer::recordMaterialResourceCommandBuffer(VkDescriptorSet descriptorSet, std::string& error) {
    VkResult result = vkResetCommandBuffer(headless_command_buffer_, 0);
    if (result != VK_SUCCESS) {
        error = VkError("vkResetCommandBuffer (material resource)", result);
        return false;
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(headless_command_buffer_, &begin);
    if (result != VK_SUCCESS) {
        error = VkError("vkBeginCommandBuffer (material resource)", result);
        return false;
    }

    VkClearValue clearColor{};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = offscreen_render_pass_;
    renderPassInfo.framebuffer = offscreen_framebuffer_;
    renderPassInfo.renderArea.extent = {offscreen_width_, offscreen_height_};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(headless_command_buffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(headless_command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, material_resource_pipeline_);

    VkViewport viewport{};
    viewport.width = static_cast<float>(offscreen_width_);
    viewport.height = static_cast<float>(offscreen_height_);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(headless_command_buffer_, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {offscreen_width_, offscreen_height_}};
    vkCmdSetScissor(headless_command_buffer_, 0, 1, &scissor);

    vkCmdBindDescriptorSets(headless_command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             material_resource_pipeline_layout_, 0, 1, &descriptorSet, 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(headless_command_buffer_, 0, 1, &material_resource_vertex_buffer_, &offset);
    vkCmdBindIndexBuffer(headless_command_buffer_, material_resource_index_buffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(headless_command_buffer_, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(headless_command_buffer_);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = offscreen_image_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    // Real, explicit synchronization for the SAME read-after-write
    // pattern documented in GRAPHICS/README.md's GPU Frame Lifecycle
    // record -- this render pass's own finalLayout transition already
    // handles color-write -> transfer-src; this barrier additionally
    // orders THIS render's transfer read after any PRIOR real render
    // (e.g. a preceding RenderOffscreen call reusing the same real
    // offscreen_image_).
    vkCmdPipelineBarrier(headless_command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                          0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {offscreen_width_, offscreen_height_, 1};
    vkCmdCopyImageToBuffer(headless_command_buffer_, offscreen_image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            readback_buffer_, 1, &region);

    result = vkEndCommandBuffer(headless_command_buffer_);
    if (result != VK_SUCCESS) {
        error = VkError("vkEndCommandBuffer (material resource)", result);
        return false;
    }
    return true;
}

bool VulkanFrameRenderer::RenderOffscreenWithMaterialResource(const GPUResourceIdentity& identity,
                                                                std::uint32_t width, std::uint32_t height,
                                                                std::vector<std::uint8_t>& outPixels,
                                                                std::string& error) {
    if (!headless_initialized_) {
        error = "RenderOffscreenWithMaterialResource requires InitializeHeadless to have already succeeded";
        return false;
    }

    const GPUResourceLifetime* lifetime = material_resource_authority_.Find(identity);
    if (!lifetime || lifetime->State() != GPUResourceLifetimeState::kReady) {
        error = "RenderOffscreenWithMaterialResource: identity is not in a real READY state";
        return false;
    }
    auto it = material_resource_handles_.find(MaterialResourceKey(identity));
    if (it == material_resource_handles_.end()) {
        error = "RenderOffscreenWithMaterialResource: no real GPU handles for this identity";
        return false;
    }

    if (!ensureOffscreenTarget(width, height, error)) return false;
    if (!ensureMaterialResourcePipeline(error)) return false;

    // A real unit quad -- the same real geometry every other real
    // DOMINUS mesh uses (MeshLibrary::UnitQuad), scaled to a real,
    // visible portion of the target and uploaded as position-only
    // vertex data (no color -- this pipeline's shader never reads
    // one).
    const Mesh& mesh = MeshLibrary::UnitQuad();
    animation::Transform2D transform{0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    auto positions = TransformMeshVertices(mesh, transform);
    std::vector<float> positionOnlyVertices;
    positionOnlyVertices.reserve(positions.size() * 2);
    const float fwidth = static_cast<float>(std::max(1u, width));
    const float fheight = static_cast<float>(std::max(1u, height));
    for (const auto& [wx, wy] : positions) {
        positionOnlyVertices.push_back(wx / (fwidth * 0.5f));
        positionOnlyVertices.push_back(-wy / (fheight * 0.5f));
    }

    // Real, dedicated scratch buffers for this pipeline -- created
    // once, reused thereafter, the SAME "one fixed scratch resource,
    // content rebuilt every call" contract the offscreen path already
    // uses and has proven correct (GPU Frame Lifecycle milestone).
    if (material_resource_vertex_buffer_ == VK_NULL_HANDLE) {
        VkDeviceSize size = sizeof(float) * 2 * 4;  // 4 real vertices, position-only
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &info, nullptr, &material_resource_vertex_buffer_) != VK_SUCCESS) {
            error = "vkCreateBuffer failed for material resource vertex buffer";
            return false;
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, material_resource_vertex_buffer_, &requirements);
        std::uint32_t type = 0;
        findMemoryType(requirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, type);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = type;
        vkAllocateMemory(device_, &alloc, nullptr, &material_resource_vertex_memory_);
        vkBindBufferMemory(device_, material_resource_vertex_buffer_, material_resource_vertex_memory_, 0);

        std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
        VkDeviceSize indexSize = sizeof(std::uint32_t) * indices.size();
        VkBufferCreateInfo indexInfo{};
        indexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexInfo.size = indexSize;
        indexInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        indexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &indexInfo, nullptr, &material_resource_index_buffer_);
        VkMemoryRequirements indexRequirements{};
        vkGetBufferMemoryRequirements(device_, material_resource_index_buffer_, &indexRequirements);
        std::uint32_t indexType = 0;
        findMemoryType(indexRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexType);
        VkMemoryAllocateInfo indexAlloc{};
        indexAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        indexAlloc.allocationSize = indexRequirements.size;
        indexAlloc.memoryTypeIndex = indexType;
        vkAllocateMemory(device_, &indexAlloc, nullptr, &material_resource_index_memory_);
        vkBindBufferMemory(device_, material_resource_index_buffer_, material_resource_index_memory_, 0);

        void* mappedIndices = nullptr;
        vkMapMemory(device_, material_resource_index_memory_, 0, indexSize, 0, &mappedIndices);
        std::memcpy(mappedIndices, indices.data(), static_cast<std::size_t>(indexSize));
        vkUnmapMemory(device_, material_resource_index_memory_);
    }

    void* mappedVertices = nullptr;
    VkResult mapResult = vkMapMemory(device_, material_resource_vertex_memory_, 0,
                                      sizeof(float) * positionOnlyVertices.size(), 0, &mappedVertices);
    if (mapResult != VK_SUCCESS) {
        error = VkError("vkMapMemory (material resource vertices)", mapResult);
        return false;
    }
    std::memcpy(mappedVertices, positionOnlyVertices.data(), sizeof(float) * positionOnlyVertices.size());
    vkUnmapMemory(device_, material_resource_vertex_memory_);

    if (!recordMaterialResourceCommandBuffer(it->second.descriptor_set, error)) return false;

    VkResult result = vkResetFences(device_, 1, &headless_fence_);
    if (result != VK_SUCCESS) {
        error = VkError("vkResetFences (material resource)", result);
        return false;
    }

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &headless_command_buffer_;
    result = vkQueueSubmit(graphics_queue_, 1, &submit, headless_fence_);
    if (result != VK_SUCCESS) {
        error = VkError("vkQueueSubmit (material resource)", result);
        return false;
    }

    result = vkWaitForFences(device_, 1, &headless_fence_, VK_TRUE, /*10s=*/10'000'000'000ull);
    if (result != VK_SUCCESS) {
        error = VkError("vkWaitForFences (material resource)", result);
        return false;
    }

    outPixels.resize(static_cast<std::size_t>(readback_size_));
    void* mapped = nullptr;
    result = vkMapMemory(device_, readback_memory_, 0, readback_size_, 0, &mapped);
    if (result != VK_SUCCESS) {
        error = VkError("vkMapMemory (material resource readback)", result);
        return false;
    }
    std::memcpy(outPixels.data(), mapped, static_cast<std::size_t>(readback_size_));
    vkUnmapMemory(device_, readback_memory_);
    return true;
}

}  // namespace dominus::graphics