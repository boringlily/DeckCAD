#include "App.h"
#include "Assets.h"
#include "GpuContext.h"
#include "Icons.h"
#include "MsdfAtlas.h"
#include "UiContext.h"
#include "Viewport.h"
#include "Window.h"

#ifdef DECKCAD_HOT_RELOAD_ENABLED
#include "HotReloadCore.h"
#endif

#include <SDL3/SDL.h>
#include <cstdio>
#include <string>

namespace MainInternal {

constexpr u32 WINDOW_WIDTH = 1440;
constexpr u32 WINDOW_HEIGHT = 744;
constexpr const char* WINDOW_TITLE = "DeckCAD";

/// Icon edge length in logical points.
constexpr u32 ICON_LOGICAL_SIZE = 20;

void ReportError(const char* stage_ptr, const std::string& message_ref)
{
    std::fprintf(stderr, "[DeckCAD] %s: %s\n", stage_ptr, message_ref.c_str());
}

} // namespace MainInternal
using namespace MainInternal;

int main(int, char**)
{
    Platform::Window window;
    if (!window.createWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        ReportError("window", window.getLastError());
        return 1;
    }

    Gpu::ContextDescriptor gpu_descriptor {};
    gpu_descriptor.native_system_ptr = window.getNativeSystem();
    gpu_descriptor.native_window_ptr = window.getNativeWindow();
    gpu_descriptor.native_display_ptr = window.getNativeDisplay();
    gpu_descriptor.native_surface_ptr = window.getNativeSurface();
    gpu_descriptor.width = window.getPixelWidth();
    gpu_descriptor.height = window.getPixelHeight();
    gpu_descriptor.vsync = true;

    Gpu::Context gpu;
    if (!gpu.initializeResources(gpu_descriptor)) {
        ReportError("webgpu", gpu.getLastError());
        return 1;
    }

    const f32 display_scale = window.getDisplayScale();

    Ui::Context ui;
    std::string error;
    if (!ui.initializeResources(window.getHandle(), gpu, display_scale, error)) {
        ReportError("imgui", error);
        return 1;
    }

    Ui::IconSet icons;
    if (!icons.initializeResources(gpu, display_scale, ICON_LOGICAL_SIZE, error)) {
        // Icons are cosmetic; the app remains usable without them.
        ReportError("icons", error);
    }

    // Bake the viewport label font. A failure here only costs in-viewport text.
    Text::MsdfAtlas atlas;
    Text::AtlasConfiguration atlas_configuration {};
    const std::string font_path = Platform::Assets::Resolve("fonts/Nunito/static/Nunito-SemiBold.ttf");
    if (!atlas.buildAtlas(font_path, atlas_configuration, error)) {
        ReportError("text atlas", error);
    }

    Viewport::Viewport viewport;
    if (!viewport.initializeResources(gpu, atlas, error)) {
        ReportError("viewport", error);
        return 1;
    }

    Core::AppState app;

#ifdef DECKCAD_HOT_RELOAD_ENABLED
    HotReloadCore hot_reload_core;
    const char* base_path_ptr = SDL_GetBasePath();
    if (!hot_reload_core.openLibrary(base_path_ptr ? base_path_ptr : ".")) {
        ReportError("hot reload", "failed to load deckcad_core");
        return 1;
    }
    // Open a scene immediately; the viewport needs one to show on first launch.
    hot_reload_core.appInit(app);
#else
    // Open a scene immediately; the viewport needs one to show on first launch.
    app.newScene();
#endif

    u64 previous_counter = SDL_GetPerformanceCounter();
    const f64 counter_frequency = static_cast<f64>(SDL_GetPerformanceFrequency());

    while (!window.shouldClose()) {
        window.pumpEvents();

        const u64 current_counter = SDL_GetPerformanceCounter();
        const f32 delta_time = static_cast<f32>(static_cast<f64>(current_counter - previous_counter) / counter_frequency);
        previous_counter = current_counter;

        // Nothing to present while minimized, and the surface may be zero-sized.
        if (window.isMinimized()) {
            SDL_Delay(16);
            continue;
        }

        gpu.resizeTarget(window.getPixelWidth(), window.getPixelHeight());
        gpu.tickDevice();

        if (!gpu.beginFrame()) {
            continue; // surface was reconfigured; try again next frame
        }

        ui.beginFrame();

        Core::FrameContext frame {
            .app_ref = app,
            .gpu_ref = gpu,
            .icons_ref = icons,
            .viewport_ref = viewport,
            .delta_time = delta_time,
            .display_scale = display_scale,
        };
        // Builds the UI and records the offscreen viewport pass into the frame's
        // encoder, ahead of the ImGui pass below that samples its result.
#ifdef DECKCAD_HOT_RELOAD_ENABLED
        hot_reload_core.reloadIfChanged(app);
        hot_reload_core.buildFrame(frame);
#else
        Core::BuildFrame(frame);
#endif

        wgpu::RenderPassColorAttachment color_attachment {};
        color_attachment.view = gpu.getBackbufferView();
        color_attachment.loadOp = wgpu::LoadOp::Clear;
        color_attachment.storeOp = wgpu::StoreOp::Store;
        const DeckMath::Vector4 clear = Ui::gui_theme.background_dark.toVector4();
        color_attachment.clearValue = { clear.x, clear.y, clear.z, 1.0f };

        wgpu::RenderPassDescriptor pass_descriptor {};
        pass_descriptor.label = "UI Pass";
        pass_descriptor.colorAttachmentCount = 1;
        pass_descriptor.colorAttachments = &color_attachment;

        wgpu::RenderPassEncoder pass = gpu.getEncoder().BeginRenderPass(&pass_descriptor);
        ui.renderFrame(pass);
        pass.End();

        gpu.endFrame();
    }

    // Explicit teardown order: everything holding GPU handles goes before the
    // device that created them.
    viewport.shutdownResources();
    icons.shutdownResources();
    ui.shutdownResources();
    gpu.shutdownResources();
    window.destroyWindow();
    return 0;
}
