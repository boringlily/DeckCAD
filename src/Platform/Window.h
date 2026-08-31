#pragma once
#include "Types.h"
#include <string>

struct SDL_Window;

namespace Platform {

/**
 * @brief Owns the SDL3 window and the event pump.
 * @note This is the only place that talks to SDL. Everything above it
 * consumes the small surface below; swapping the windowing layer stays a
 * local change.
 */
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// Creates the SDL window. Returns false and fills getLastError() on failure.
    bool createWindow(const std::string& title_ref, u32 width, u32 height);
    void destroyWindow();

    /// Pumps the SDL event queue, forwarding each event to Dear ImGui.
    /// Sets shouldClose() when the user closes the window.
    void pumpEvents();

    bool shouldClose() const { return should_close_; }
    void requestClose() { should_close_ = true; }

    /// Size in physical pixels (what the swapchain must match), not logical points.
    u32 getPixelWidth() const { return pixel_width_; }
    u32 getPixelHeight() const { return pixel_height_; }

    /// True when the window is minimized and rendering should be skipped.
    bool isMinimized() const;

    /// Ratio of physical pixels to logical points; 2.0 on a Retina display.
    f32 getDisplayScale() const;

    SDL_Window* getHandle() const { return window_ptr_; }

    /// Native handles, used to build the WebGPU surface.
    /// system is one of "cocoa", "win32", "wayland", "x11".
    const char* getNativeSystem() const;
    void* getNativeWindow() const;
    void* getNativeDisplay() const;
    void* getNativeSurface() const;

    const std::string& getLastError() const { return last_error_; }

private:
    void refreshPixelSize();

    SDL_Window* window_ptr_ { nullptr };
    bool should_close_ { false };
    bool sdl_initialized_ { false };
    u32 pixel_width_ { 0 };
    u32 pixel_height_ { 0 };
    std::string last_error_;
};

} // namespace Platform
