// tests/graphics/test_render_boundary.cpp
// The hard architectural invariant, enforced automatically rather than
// trusted to stay true: WORLD::EntityRegistry / CORE::MetaBinObject /
// GRAPHICS::Scene must never be referenced inside the renderer
// implementations. Frame is the only channel.
//
//   WORLD::EntityRegistry
//         |
//   GRAPHICS::Scene / FrameCompiler
//         |
//   GRAPHICS::Frame        <- the ONLY channel past this point
//         |
//   RasterDevice | VulkanFrameRenderer
//
// This is a real, direct check of the actual source files -- not an
// assertion that happens to be true today. If a future change makes
// either renderer reach past Frame back into EntityRegistry/
// MetaBinObject/Scene, this test fails immediately, the same day that
// change is made, rather than the boundary quietly eroding until
// someone notices.
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path RepoRoot() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("."),
        std::filesystem::path(".."),
        std::filesystem::path("../.."),
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c / "GRAPHICS" / "Renderer" / "Frame.h")) return c;
    }
    throw std::runtime_error("could not locate repository root from the test working directory");
}

std::string ReadWhole(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("could not read " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The real forbidden strings -- exact type names, not vague keywords,
// so this doesn't false-positive on unrelated prose (e.g. this file's
// own header comment mentions "EntityRegistry" in English, which is
// exactly why this check only inspects the RENDERER files, never
// itself).
const std::vector<std::string>& ForbiddenReferences() {
    static const std::vector<std::string> forbidden = {
        "EntityRegistry",
        "MetaBinObject",
        "dominus::graphics::Scene",
        "SceneEntity",
        "WORLD/Core/EntityRegistry.h",
        "CORE/MetaBin/MetaBinObject.h",
        "GRAPHICS/Renderer/Scene.h",
        "GRAPHICS/Renderer/SceneFromEntities.h",
    };
    return forbidden;
}

void CheckFileClean(const std::filesystem::path& path, std::vector<std::string>& violations) {
    std::string content = ReadWhole(path);
    for (const auto& forbidden : ForbiddenReferences()) {
        if (content.find(forbidden) != std::string::npos) {
            violations.push_back(path.string() + " references '" + forbidden + "'");
        }
    }
}

}  // namespace

DOMINUS_TEST(RenderBoundary_RasterDevice_NeverReferencesEntityRegistryOrScene) {
    auto root = RepoRoot();
    std::vector<std::string> violations;
    CheckFileClean(root / "GRAPHICS" / "Raster" / "RasterDevice.h", violations);
    CheckFileClean(root / "GRAPHICS" / "Raster" / "RasterDevice.cpp", violations);
    for (const auto& v : violations) {
        std::cerr << "RENDER BOUNDARY VIOLATION: " << v << "\n";
    }
    DOMINUS_EXPECT(violations.empty());
}

DOMINUS_TEST(RenderBoundary_VulkanFrameRenderer_NeverReferencesEntityRegistryOrScene) {
    auto root = RepoRoot();
    std::vector<std::string> violations;
    CheckFileClean(root / "GRAPHICS" / "Vulkan" / "VulkanFrameRenderer.h", violations);
    CheckFileClean(root / "GRAPHICS" / "Vulkan" / "VulkanFrameRenderer.cpp", violations);
    for (const auto& v : violations) {
        std::cerr << "RENDER BOUNDARY VIOLATION: " << v << "\n";
    }
    DOMINUS_EXPECT(violations.empty());
}

DOMINUS_TEST(RenderBoundary_SceneFromEntities_IsTheOneDeclaredBridge_AndIsNotUsedByEitherRenderer) {
    // SceneFromEntities is explicitly ALLOWED to reference
    // EntityRegistry/MetaBinObject -- that is its one, documented job.
    // What must remain true is that neither renderer calls it.
    auto root = RepoRoot();
    std::string bridge = ReadWhole(root / "GRAPHICS" / "Renderer" / "SceneFromEntities.h");
    DOMINUS_EXPECT(bridge.find("EntityRegistry") != std::string::npos);  // confirms this IS the real bridge

    std::string rasterDeviceCpp = ReadWhole(root / "GRAPHICS" / "Raster" / "RasterDevice.cpp");
    std::string vulkanCpp = ReadWhole(root / "GRAPHICS" / "Vulkan" / "VulkanFrameRenderer.cpp");
    DOMINUS_EXPECT(rasterDeviceCpp.find("SceneFromEntities") == std::string::npos);
    DOMINUS_EXPECT(vulkanCpp.find("SceneFromEntities") == std::string::npos);
}
