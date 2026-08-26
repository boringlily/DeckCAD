#pragma once
#include "AppState.h"
#include "GpuContext.h"
#include "Icons.h"
#include "Types.h"
#include "Viewport.h"

#include <imgui.h>

namespace Core {

/// Panels are fixed furniture: they never carry a title bar, and neither the
/// window nor its dock tab can be dragged somewhere else.
constexpr ImGuiWindowFlags PANEL_WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

/// Everything a panel needs for one frame.
///
/// Renderer and GPU objects arrive by reference and are never stored, so the
/// logic layer stays free of GPU ownership.
struct FrameContext {
    AppState& app_ref;
    const Gpu::Context& gpu_ref;
    Ui::IconSet& icons_ref;
    Viewport::Viewport& viewport_ref;
    f32 delta_time { 0.0f };
    f32 display_scale { 1.0f };
};

/// Builds the whole UI for one frame and records the viewport render pass.
void BuildFrame(FrameContext& context_ref);

// Panels, in the order they appear on screen.
void DrawHeaderBar(FrameContext& context_ref);
void DrawHomePage(FrameContext& context_ref);
void DrawExplorerPanel(FrameContext& context_ref);
void DrawViewportPanel(FrameContext& context_ref);
void DrawToolboxPanel(FrameContext& context_ref);
void DrawStatusBar(FrameContext& context_ref);

} // namespace Core
