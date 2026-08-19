#include <iostream>
#include <string>

#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "GRAPHICS/Renderer/Scene.h"
#include "GRAPHICS/Vulkan/VulkanFrameRenderer.h"

int main() {
    using dominus::animation::Transform2D;
    using dominus::graphics::Camera;
    using dominus::graphics::FrameCompiler;
    using dominus::graphics::Scene;
    using dominus::graphics::VulkanFrameRenderer;

    VulkanFrameRenderer renderer;
    std::string error;
    if (!renderer.Initialize(1280, 720, "DOMINUS — Real GPU Renderer", error)) {
        std::cerr << "GPU renderer initialization failed: " << error << '\n';
        return 2;
    }

    std::cout << "GPU: " << renderer.device_name() << '\n';

    // Real engine data: these are canonical SceneEntity records that pass
    // through the same FrameCompiler used by GRAPHICS acceptance tests.
    Scene scene;
    scene.entities.push_back({"brooklyn", Transform2D{-220.0f, 40.0f, 0.0f, 5.0f, 7.0f},
                              "MAT-BROOKLYN-JACKET-001", "brooklyn.mesh", 0});
    scene.entities.push_back({"rocket", Transform2D{180.0f, 10.0f, 0.0f, 6.0f, 6.0f},
                              "MAT-ROCKET-FUR-001", "rocket.mesh", 1});
    scene.entities.push_back({"static", Transform2D{0.0f, -170.0f, 0.0f, 3.0f, 3.0f},
                              "MAT-STATIC-SCREEN-001", "static.mesh", 2});

    Camera camera;
    while (!renderer.ShouldClose()) {
        const auto frame = FrameCompiler::Compile(scene, camera);
        if (!renderer.Render(frame, error)) {
            if (!renderer.ShouldClose()) std::cerr << "GPU frame render failed: " << error << '\n';
            return renderer.ShouldClose() ? 0 : 3;
        }
    }

    renderer.Shutdown();
    return 0;
}
