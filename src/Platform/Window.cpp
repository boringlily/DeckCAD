#include "Window.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>

#include <cstdint>

namespace Platform {

Window::~Window()
{
    destroyWindow();
}

bool Window::createWindow(const std::string& title_ref, u32 width, u32 height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        last_error_ = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }
    sdl_initialized_ = true;

    // HIGH_PIXEL_DENSITY: backing store at full device resolution, matching
    // what the WebGPU surface expects
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window_ptr_ = SDL_CreateWindow(title_ref.c_str(), static_cast<int>(width), static_cast<int>(height), flags);
    if (!window_ptr_) {
        last_error_ = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        return false;
    }

    SDL_SetWindowMinimumSize(window_ptr_, static_cast<int>(width), static_cast<int>(height));
    refreshPixelSize();
    return true;
}

void Window::destroyWindow()
{
    if (window_ptr_) {
        SDL_DestroyWindow(window_ptr_);
        window_ptr_ = nullptr;
    }
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
}

void Window::refreshPixelSize()
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_ptr_, &w, &h);
    pixel_width_ = static_cast<u32>(w < 0 ? 0 : w);
    pixel_height_ = static_cast<u32>(h < 0 ? 0 : h);
}

void Window::pumpEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
        case SDL_EVENT_QUIT:
            should_close_ = true;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event.window.windowID == SDL_GetWindowID(window_ptr_)) {
                should_close_ = true;
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
            refreshPixelSize();
            break;
        default:
            break;
        }
    }
}

bool Window::isMinimized() const
{
    if (!window_ptr_) {
        return true;
    }
    return (SDL_GetWindowFlags(window_ptr_) & SDL_WINDOW_MINIMIZED) != 0;
}

f32 Window::getDisplayScale() const
{
    if (!window_ptr_) {
        return 1.0f;
    }
    f32 scale = SDL_GetWindowPixelDensity(window_ptr_);
    return scale > 0.0f ? scale : 1.0f;
}

// --- Native handle plumbing for WebGPU surface creation ----------------------

const char* Window::getNativeSystem() const
{
#if defined(SDL_PLATFORM_MACOS)
    return "cocoa";
#elif defined(SDL_PLATFORM_WIN32)
    return "win32";
#elif defined(SDL_PLATFORM_LINUX)
    // Linux can run under either compositor; decided at runtime from
    // the driver SDL actually selected
    {
        const char* driver_ptr = SDL_GetCurrentVideoDriver();
        if (driver_ptr && SDL_strcmp(driver_ptr, "wayland") == 0) {
            return "wayland";
        }
        return "x11";
    }
#else
    return "";
#endif
}

void* Window::getNativeWindow() const
{
    if (!window_ptr_) {
        return nullptr;
    }
    SDL_PropertiesID props = SDL_GetWindowProperties(window_ptr_);
#if defined(SDL_PLATFORM_MACOS)
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(SDL_PLATFORM_WIN32)
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(SDL_PLATFORM_LINUX)
    if (SDL_strcmp(getNativeSystem(), "wayland") == 0) {
        return nullptr; // Wayland identifies the window by its wl_surface instead.
    }
    // X11 window ids are integers, but the surface helper takes them as a pointer-width value.
    return reinterpret_cast<void*>(
        static_cast<uintptr_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
#else
    (void)props;
    return nullptr;
#endif
}

void* Window::getNativeDisplay() const
{
#if defined(SDL_PLATFORM_LINUX)
    if (!window_ptr_) {
        return nullptr;
    }
    SDL_PropertiesID props = SDL_GetWindowProperties(window_ptr_);
    if (SDL_strcmp(getNativeSystem(), "wayland") == 0) {
        return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    }
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
#else
    return nullptr;
#endif
}

void* Window::getNativeSurface() const
{
#if defined(SDL_PLATFORM_LINUX)
    if (!window_ptr_) {
        return nullptr;
    }
    SDL_PropertiesID props = SDL_GetWindowProperties(window_ptr_);
    if (SDL_strcmp(getNativeSystem(), "wayland") == 0) {
        return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
    return nullptr;
#else
    return nullptr;
#endif
}

} // namespace Platform
