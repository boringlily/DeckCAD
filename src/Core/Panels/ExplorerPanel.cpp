#include "App.h"
#include "Widgets.h"

#include <imgui.h>

namespace Core {

void DrawExplorerPanel(FrameContext& context_ref)
{
    Scene& scene_ref = context_ref.app_ref.getCurrentScene();

    if (!ImGui::Begin("Explorer", nullptr, PANEL_WINDOW_FLAGS)) {
        ImGui::End();
        return;
    }

    const f32 icon_size = ImGui::GetFontSize();

    Ui::IconImage(context_ref.icons_ref, Ui::IconId::Project, icon_size, Ui::gui_theme.text_base);
    ImGui::SameLine();
    ImGui::TextUnformatted(scene_ref.name.c_str());

    ImGui::Separator();

    if (ImGui::TreeNodeEx("Origin", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Planes", &scene_ref.show_origin_planes);
        ImGui::Checkbox("Grid", &scene_ref.show_grid);
        ImGui::Checkbox("Axis labels", &scene_ref.show_axis_labels);
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        using Orientation = Viewport::Camera::Orientation;

        if (ImGui::Button("Iso")) {
            scene_ref.camera.setOrientation(Orientation::Isometric);
        }
        ImGui::SameLine();
        if (ImGui::Button("XY")) {
            scene_ref.camera.setOrientation(Orientation::PlaneXY);
        }
        ImGui::SameLine();
        if (ImGui::Button("XZ")) {
            scene_ref.camera.setOrientation(Orientation::PlaneXZ);
        }
        ImGui::SameLine();
        if (ImGui::Button("YZ")) {
            scene_ref.camera.setOrientation(Orientation::PlaneYZ);
        }

        bool orthographic = scene_ref.camera.getProjection() == Viewport::Camera::Projection::Orthographic;
        if (ImGui::Checkbox("Orthographic", &orthographic)) {
            scene_ref.camera.setProjection(orthographic
                    ? Viewport::Camera::Projection::Orthographic
                    : Viewport::Camera::Projection::Perspective);
        }

        const DeckMath::Vector3 position = scene_ref.camera.getPosition();
        ImGui::TextColored(Ui::ToImVec4(Ui::gui_theme.text_muted),
            "%.2f, %.2f, %.2f", static_cast<double>(position.x),
            static_cast<double>(position.y), static_cast<double>(position.z));

        if (ImGui::Button("Reset view")) {
            scene_ref.camera.resetCamera();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Grid", ImGuiTreeNodeFlags_None)) {
        Viewport::GridRenderer::Style& style_ref = context_ref.viewport_ref.getGrid().getMutableStyle();
        ImGui::DragFloat("Spacing", &style_ref.minor_spacing, 0.05f, 0.05f, 100.0f, "%.2f");
        ImGui::DragFloat("Major every", &style_ref.major_every, 1.0f, 2.0f, 100.0f, "%.0f");
        ImGui::DragFloat("Fade start", &style_ref.fade_start, 1.0f, 1.0f, 1000.0f, "%.0f");
        ImGui::DragFloat("Fade end", &style_ref.fade_end, 1.0f, 2.0f, 4000.0f, "%.0f");
        ImGui::TreePop();
    }

    ImGui::End();
}

} // namespace Core
