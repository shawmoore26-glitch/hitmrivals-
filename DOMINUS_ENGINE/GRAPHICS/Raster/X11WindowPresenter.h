// GRAPHICS/Raster/X11WindowPresenter.h
// Track H Phase 5D -- a real, minimal, additive windowed presenter for
// the CPU raster path. Deliberately NOT Vulkan/GLFW: DOMINUS_ENABLE_VULKAN
// requires a Vulkan SDK (vulkan.h, glslc) and a real Vulkan-capable
// device/ICD, neither of which exists in every environment this engine
// might build in (see GRAPHICS/README.md's own "Deliberately not built
// this phase" note from Phase 5A) -- and, separately, real texture/
// sampler support has only ever been added to RasterDevice (Phase 5A),
// never to VulkanFrameRenderer, so a real Vulkan window could not show
// real HITM sprite pixels today regardless of GPU/SDK availability. This
// file exists so "a window launches, real Brooklyn/Rocket pixels are
// shown" does not depend on either of those separately-scoped gaps: it
// blits a real, already-rasterized RasterDevice::PixelBuffer (the one
// renderer that DOES have real texture support) straight into a real X11
// window via `XPutImage` -- the standard, minimal way to show a CPU pixel
// buffer in a real window without any GPU API at all.
//
// Gated behind DOMINUS_ENABLE_X11_PRESENTER (default OFF, same
// off-by-default/explicit-opt-in discipline as DOMINUS_ENABLE_VULKAN) --
// X11 development headers are not guaranteed present in every build
// environment either. Vulkan itself is completely untouched by this
// file: no VulkanFrameRenderer.h/.cpp include, no shader, no
// DOMINUS_ENABLE_VULKAN reference anywhere here.
//
// Real, disclosed scope limit, same "real, disclosed scope limits, not
// fabricated completeness" discipline as GRAPHICS/Raster/PngDecoder.h:
// this class supports exactly the one real, common X11 TrueColor visual
// this engine has actually verified pixel-for-pixel (24-bit depth, 32
// bits per pixel, red_mask=0xFF0000/green_mask=0xFF00/blue_mask=0xFF,
// LSBFirst byte order -- confirmed against a real Xvfb server; see
// HITM_APPLICATION_LOOP_REPORT.md for the verification) -- and refuses,
// with a specific real error, any other depth/mask/byte-order
// combination rather than silently drawing corrupted colors.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GRAPHICS/Raster/RasterDevice.h"

// Forward declarations only -- no <X11/Xlib.h> include here, so this
// header stays includable (though the class is unusable without linking
// libX11) without pulling X11's own extremely broad global-namespace
// macros into every translation unit that merely wants the type.
struct _XDisplay;
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef struct _XGC* GC;

namespace dominus::graphics {

class X11WindowPresenter {
public:
    X11WindowPresenter() = default;
    ~X11WindowPresenter();

    X11WindowPresenter(const X11WindowPresenter&) = delete;
    X11WindowPresenter& operator=(const X11WindowPresenter&) = delete;

    // Opens the real X11 display named by the `DISPLAY` environment
    // variable (or `displayName`, if non-empty), creates and maps a
    // real window of `width`x`height`. Returns false with a real,
    // specific error -- never a fabricated success -- if no X server is
    // reachable (no `DISPLAY` set, nothing listening) or the real
    // default visual is not the one, real, supported TrueColor format
    // this class handles (see this file's own header comment). The
    // caller is expected to treat that as NOT_EXECUTED in an
    // environment with no real display, same discipline
    // VulkanFrameRenderer::InitializeHeadless already established for
    // "no real GPU available."
    bool Initialize(int width, int height, const std::string& title, std::string& error,
                     const std::string& displayName = "");

    // Processes pending real X events; sets ShouldClose() to true on a
    // real WM_DELETE_WINDOW client message (the window's real close
    // button) -- the same real signal a windowed Vulkan present loop
    // would react to via its own ShouldClose().
    void PollEvents();
    bool ShouldClose() const { return shouldClose_; }

    // Converts `buffer`'s real RGBA8 pixels into the real window's own
    // native pixel format and blits them via a real `XPutImage` call,
    // then flushes so the real X server actually processes it before
    // this returns. `buffer.width`/`buffer.height` must exactly match
    // the window's own real dimensions (from `Initialize`) -- this
    // class does not scale.
    bool Present(const PixelBuffer& buffer, std::string& error);

    void Shutdown();
    bool initialized() const { return initialized_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // The real, live Xlib `Display*` this presenter opened -- exposed so
    // a real live keyboard/gamepad reader (e.g. `XQueryKeymap`) can bind
    // to the SAME real X server connection this window is on, rather
    // than opening a second, redundant one. Returns nullptr before a
    // successful Initialize().
    Display* RawDisplay() const { return display_; }

    // Real, closed-loop verification seam: reads the window's ACTUAL,
    // real, server-side current pixel content back via `XGetImage`,
    // converted back to RGBA8 in the same convention `PixelBuffer`
    // already uses. Exists so a test/verification run can prove the
    // pixels a real window is actually displaying match what
    // `RasterDevice` produced -- not just that `Present` returned true.
    bool ReadBackForVerification(std::vector<std::uint8_t>& outRgba, std::string& error) const;

private:
    Display* display_ = nullptr;
    Window window_ = 0;
    GC gc_ = nullptr;
    unsigned long wmDeleteMessage_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
    bool shouldClose_ = false;
};

}  // namespace dominus::graphics
