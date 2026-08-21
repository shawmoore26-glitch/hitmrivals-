// GRAPHICS/Raster/X11WindowPresenter.cpp
#include "GRAPHICS/Raster/X11WindowPresenter.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace dominus::graphics {

X11WindowPresenter::~X11WindowPresenter() { Shutdown(); }

bool X11WindowPresenter::Initialize(int width, int height, const std::string& title, std::string& error,
                                      const std::string& displayName) {
    if (initialized_) {
        error = "X11WindowPresenter: already initialized";
        return false;
    }
    if (width <= 0 || height <= 0) {
        error = "X11WindowPresenter: width/height must be positive";
        return false;
    }

    ::Display* display = XOpenDisplay(displayName.empty() ? nullptr : displayName.c_str());
    if (!display) {
        error = "X11WindowPresenter: XOpenDisplay failed -- no real X server reachable (check DISPLAY)";
        return false;
    }

    int screen = DefaultScreen(display);
    Visual* visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    // Real, disclosed scope limit -- see this file's own header comment.
    if (depth != 24 || visual->red_mask != 0x00FF0000UL || visual->green_mask != 0x0000FF00UL ||
        visual->blue_mask != 0x000000FFUL || ImageByteOrder(display) != LSBFirst) {
        error = "X11WindowPresenter: unsupported real X11 visual (this class supports exactly one real, "
                "verified combination: 24-bit depth, red/green/blue masks 0xFF0000/0xFF00/0xFF, LSBFirst)";
        XCloseDisplay(display);
        return false;
    }

    ::Window root = RootWindow(display, screen);
    ::Window window = XCreateSimpleWindow(display, root, 0, 0, static_cast<unsigned>(width),
                                            static_cast<unsigned>(height), 1, BlackPixel(display, screen),
                                            WhitePixel(display, screen));
    XStoreName(display, window, title.c_str());
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDelete, 1);
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);
    XFlush(display);

    ::GC gc = XCreateGC(display, window, 0, nullptr);

    display_ = display;
    window_ = window;
    gc_ = gc;
    wmDeleteMessage_ = wmDelete;
    width_ = width;
    height_ = height;
    initialized_ = true;
    shouldClose_ = false;
    return true;
}

void X11WindowPresenter::PollEvents() {
    if (!initialized_) return;
    while (XPending(display_) > 0) {
        XEvent event;
        XNextEvent(display_, &event);
        if (event.type == ClientMessage &&
            static_cast<unsigned long>(event.xclient.data.l[0]) == wmDeleteMessage_) {
            shouldClose_ = true;
        }
    }
}

bool X11WindowPresenter::Present(const PixelBuffer& buffer, std::string& error) {
    if (!initialized_) {
        error = "X11WindowPresenter: not initialized";
        return false;
    }
    if (buffer.width != width_ || buffer.height != height_) {
        error = "X11WindowPresenter: PixelBuffer dimensions do not match the real window size";
        return false;
    }

    // Real conversion: PixelBuffer's RGBA8 -> this window's real, already-
    // verified 32bpp/LSBFirst/0xFF0000-0xFF00-0xFF TrueColor layout, i.e.
    // per pixel memory bytes [B, G, R, unused].
    std::vector<char> converted(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_); ++i) {
        converted[i * 4 + 0] = static_cast<char>(buffer.rgba[i * 4 + 2]);  // B
        converted[i * 4 + 1] = static_cast<char>(buffer.rgba[i * 4 + 1]);  // G
        converted[i * 4 + 2] = static_cast<char>(buffer.rgba[i * 4 + 0]);  // R
        converted[i * 4 + 3] = 0;
    }

    Visual* visual = DefaultVisual(display_, DefaultScreen(display_));
    XImage* image = XCreateImage(display_, visual, 24, ZPixmap, 0, converted.data(), static_cast<unsigned>(width_),
                                   static_cast<unsigned>(height_), 32, 0);
    if (!image) {
        error = "X11WindowPresenter: XCreateImage failed";
        return false;
    }
    XPutImage(display_, window_, gc_, image, 0, 0, 0, 0, static_cast<unsigned>(width_),
              static_cast<unsigned>(height_));
    // `image->data` points at `converted`'s own stack-local buffer, not
    // Xlib-owned memory -- clear it before XDestroyImage so Xlib frees
    // only the real XImage struct it allocated, never our vector's data.
    image->data = nullptr;
    XDestroyImage(image);
    XFlush(display_);
    return true;
}

bool X11WindowPresenter::ReadBackForVerification(std::vector<std::uint8_t>& outRgba, std::string& error) const {
    if (!initialized_) {
        error = "X11WindowPresenter: not initialized";
        return false;
    }
    XImage* image = XGetImage(display_, window_, 0, 0, static_cast<unsigned>(width_), static_cast<unsigned>(height_),
                                AllPlanes, ZPixmap);
    if (!image) {
        error = "X11WindowPresenter: XGetImage failed";
        return false;
    }
    outRgba.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4, 0);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            // XGetPixel is format-agnostic -- it uses the real image's
            // own reported masks internally, an independent read path
            // from Present()'s own conversion assumptions.
            unsigned long pixel = XGetPixel(image, x, y);
            std::uint8_t r = static_cast<std::uint8_t>((pixel & 0x00FF0000UL) >> 16);
            std::uint8_t g = static_cast<std::uint8_t>((pixel & 0x0000FF00UL) >> 8);
            std::uint8_t b = static_cast<std::uint8_t>(pixel & 0x000000FFUL);
            std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                                 static_cast<std::size_t>(x)) *
                                4;
            outRgba[idx + 0] = r;
            outRgba[idx + 1] = g;
            outRgba[idx + 2] = b;
            outRgba[idx + 3] = 255;
        }
    }
    XDestroyImage(image);
    return true;
}

void X11WindowPresenter::Shutdown() {
    if (!initialized_) return;
    if (gc_) XFreeGC(display_, gc_);
    if (window_) XDestroyWindow(display_, window_);
    if (display_) XCloseDisplay(display_);
    display_ = nullptr;
    window_ = 0;
    gc_ = nullptr;
    initialized_ = false;
}

}  // namespace dominus::graphics
