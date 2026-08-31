#include "App.h"
#include "Widgets.h"

#include <imgui.h>

namespace Core {
namespace ViewportPanelInternal {

    /// Queues the scene decorations the viewport draws every frame.
    void QueueSceneContent(FrameContext& context_ref, Scene& scene_ref)
    {
        context_ref.viewport_ref.beginScene();
        context_ref.viewport_ref.getGrid().setEnabled(scene_ref.show_grid);

        if (scene_ref.show_origin_planes) {
            constexpr f32 PLANE_HALF_SIZE = 5.0f;
            // color matches each plane's axis pair, alpha kept low to read as construction geometry, not a solid
            context_ref.viewport_ref.getSolids().addOriginPlane(Viewport::OriginPlane::XZ, PLANE_HALF_SIZE,
                { 0.35f, 0.65f, 0.35f, 0.10f });
            context_ref.viewport_ref.getSolids().addOriginPlane(Viewport::OriginPlane::XY, PLANE_HALF_SIZE,
                { 0.35f, 0.45f, 0.85f, 0.10f });
            context_ref.viewport_ref.getSolids().addOriginPlane(Viewport::OriginPlane::YZ, PLANE_HALF_SIZE,
                { 0.85f, 0.40f, 0.40f, 0.10f });
        }

        if (scene_ref.show_axis_labels) {
            constexpr f32 AXIS_LENGTH = 5.5f;
            constexpr f32 LABEL_PIXEL_SIZE = 15.0f;
            context_ref.viewport_ref.getLabels().addLabel("X", { AXIS_LENGTH, 0.0f, 0.0f }, LABEL_PIXEL_SIZE,
                { 0.90f, 0.35f, 0.38f, 1.0f });
            context_ref.viewport_ref.getLabels().addLabel("Y", { 0.0f, AXIS_LENGTH, 0.0f }, LABEL_PIXEL_SIZE,
                { 0.45f, 0.85f, 0.45f, 1.0f });
            context_ref.viewport_ref.getLabels().addLabel("Z", { 0.0f, 0.0f, AXIS_LENGTH }, LABEL_PIXEL_SIZE,
                { 0.40f, 0.60f, 0.95f, 1.0f });
            context_ref.viewport_ref.getLabels().addLabel("origin", { 0.0f, 0.0f, 0.0f }, 12.0f,
                { 0.75f, 0.75f, 0.80f, 0.9f }, Text::AlignHorizontal::Left, Text::AlignVertical::Top);
        }
    }

    /**
     * @brief Applies mouse input to the camera.
     * @note Mapping is carried over from the original canvas: left+right together orbits, middle pans, wheel zooms.
     */
    void ApplyCameraInput(Scene& scene_ref, bool active, bool hovered)
    {
        const ImGuiIO& io_ref = ImGui::GetIO();

        if (active) {
            const bool orbit = ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDown(ImGuiMouseButton_Right);
            const bool pan = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

            if (orbit) {
                scene_ref.camera.orbitAroundTarget(io_ref.MouseDelta.x, io_ref.MouseDelta.y);
            } else if (pan) {
                scene_ref.camera.panAcrossView(io_ref.MouseDelta.x, io_ref.MouseDelta.y);
            }
        }

        // wheel gated on hover, keeps scrolling a docked panel from moving the camera
        if (hovered && io_ref.MouseWheel != 0.0f) {
            scene_ref.camera.zoomTowardTarget(io_ref.MouseWheel);
        }
    }

    void DrawViewportOverlay(FrameContext& context_ref, Scene& scene_ref, ImVec2 image_top_left)
    {
        const ImGuiIO& io_ref = ImGui::GetIO();

        ImGui::SetCursorScreenPos(ImVec2(image_top_left.x + 8.0f, image_top_left.y + 8.0f));
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, Ui::ToImVec4(Ui::gui_theme.text_muted));
        ImGui::Text("%.0f FPS  |  %ux%u", io_ref.Framerate, context_ref.viewport_ref.getWidth(), context_ref.viewport_ref.getHeight());
        ImGui::Text("dist %.2f", static_cast<double>(scene_ref.camera.getDistanceToTarget()));
        ImGui::PopStyleColor();
        ImGui::EndGroup();
    }

} // namespace ViewportPanelInternal
using namespace ViewportPanelInternal;

void DrawViewportPanel(FrameContext& context_ref)
{
    Scene& scene_ref = context_ref.app_ref.getCurrentScene();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool open = ImGui::Begin("Viewport", nullptr,
        PANEL_WINDOW_FLAGS | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (!open) {
        ImGui::End();
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x < 1.0f || available.y < 1.0f) {
        ImGui::End();
        return;
    }

    // ImGui works in logical points, render target must be sized in physical pixels or the viewport is soft on HiDPI
    const u32 pixel_width = static_cast<u32>(available.x * context_ref.display_scale);
    const u32 pixel_height = static_cast<u32>(available.y * context_ref.display_scale);
    context_ref.viewport_ref.resizeTarget(context_ref.gpu_ref, pixel_width, pixel_height);

    QueueSceneContent(context_ref, scene_ref);
    // recorded into this frame's encoder, ahead of the ImGui pass that samples the result, texture write precedes the read
    context_ref.viewport_ref.renderFrame(context_ref.gpu_ref, scene_ref.camera);

    const ImVec2 image_top_left = ImGui::GetCursorScreenPos();

    if (context_ref.viewport_ref.isReady()) {
        f32 uv_max_u = 1.0f;
        f32 uv_max_v = 1.0f;
        context_ref.viewport_ref.getContentUvMax(uv_max_u, uv_max_v);

        ImGui::Image(Ui::ToImTextureID(context_ref.viewport_ref.getColorTextureView().Get()),
            available, ImVec2(0.0f, 0.0f), ImVec2(uv_max_u, uv_max_v));
    } else {
        ImGui::Dummy(available);
    }

    // invisible button over the same rect gives proper drag capture, keeps an orbit tracking even off-panel
    ImGui::SetCursorScreenPos(image_top_left);
    ImGui::InvisibleButton("##viewport_input", available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    ApplyCameraInput(scene_ref, ImGui::IsItemActive(), ImGui::IsItemHovered());

    DrawViewportOverlay(context_ref, scene_ref, image_top_left);

    ImGui::End();
}

} // namespace Core
