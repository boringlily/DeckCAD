#include "App.h"
#include "Widgets.h"

#include <imgui.h>

namespace Core {

void DrawStatusBar(FrameContext& context_ref)
{
    const Scene& scene_ref = context_ref.app_ref.getCurrentScene();
    const Ui::Palette& theme_ref = Ui::gui_theme;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Ui::ToImVec4(theme_ref.background_dark));
    ImGui::BeginChild("StatusBar", ImVec2(0, ImGui::GetFrameHeight()),
        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    const char* context_name_ptr = (scene_ref.toolbox.context & Toolbox::Sketch) ? "Sketch" : "Solid";
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "Context: %s", context_name_ptr);

    ImGui::SameLine();
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "|");
    ImGui::SameLine();
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "Tool: %s",
        scene_ref.toolbox.hasActiveTool() ? "active" : "none");

    ImGui::SameLine();
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "|");
    ImGui::SameLine();
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "%.1f ms", static_cast<double>(context_ref.delta_time * 1000.0f));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace Core
