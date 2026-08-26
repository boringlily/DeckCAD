#pragma once
#include "GpuContext.h"
#include "Theme.h"
#include "Types.h"
#include <string>

struct SDL_Window;
struct ImFont;

namespace Ui {

/// Owns the Dear ImGui context and its SDL3 + WebGPU backends.
class Context {
public:
    ~Context();

    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    bool initializeResources(SDL_Window* window_ptr, const Gpu::Context& gpu_ref, f32 display_scale, std::string& out_error_ref);
    void shutdownResources();

    /// Starts a new ImGui frame.
    void beginFrame();

    /// Ends the frame and records ImGui's draw commands into the given pass_ref.
    void renderFrame(const wgpu::RenderPassEncoder& pass_ref);

    /// Applies the palette in Ui::gui_theme to the ImGui style. Call after
    /// changing the theme at runtime.
    void applyTheme();

    ImFont* getRegularFont() const { return font_regular_ptr_; }
    ImFont* getTitleFont() const { return font_title_ptr_; }
    f32 getDisplayScale() const { return display_scale_; }

private:
    bool initialized_ { false };
    bool sdl_backend_ready_ { false };
    bool wgpu_backend_ready_ { false };
    f32 display_scale_ { 1.0f };
    ImFont* font_regular_ptr_ { nullptr };
    ImFont* font_title_ptr_ { nullptr };
};

} // namespace Ui
