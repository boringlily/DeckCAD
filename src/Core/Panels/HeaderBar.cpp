#include "App.h"
#include "Widgets.h"

#include <imgui.h>

namespace Core
{

void DrawHeaderBar(FrameContext& context_ref)
{
    const Ui::Palette& theme_ref = Ui::gui_theme;
    const f32 icon_size = ImGui::GetFontSize();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Ui::ToImVec4(theme_ref.background_dark));
    ImGui::BeginChild("HeaderBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing()),
        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    if(Ui::IconButton(context_ref.icons_ref, Ui::IconId::Home, "##home", icon_size,
           context_ref.app_ref.active_tab == 0 ? theme_ref.accent_primary : theme_ref.text_base))
    {
        context_ref.app_ref.active_tab = 0;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Home");
    }

    // close deferred to avoid mutating scenes mid-iteration
    size_t scene_to_close = static_cast<size_t>(-1);

    for(size_t index = 0; index < context_ref.app_ref.scenes.size(); ++index)
    {
        Scene& scene_ref = context_ref.app_ref.scenes[index];
        const u32 tab_id = static_cast<u32>(index) + 1;
        const bool active = context_ref.app_ref.active_tab == tab_id;

        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(index));

        if(Ui::TabButton(scene_ref.name.c_str(), active))
        {
            context_ref.app_ref.active_tab = tab_id;
        }

        if(active)
        {
            ImGui::SameLine(0.0f, 2.0f);
            if(Ui::IconButton(context_ref.icons_ref, Ui::IconId::Exit, "##close", icon_size * 0.75f, theme_ref.alert_danger))
            {
                scene_to_close = index;
            }
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Close %s", scene_ref.name.c_str());
            }
        }

        ImGui::PopID();
    }

    ImGui::SameLine();
    if(Ui::IconButton(context_ref.icons_ref, Ui::IconId::Plus, "##newscene", icon_size, theme_ref.text_base))
    {
        context_ref.app_ref.newScene();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("New scene");
    }

    const f32 utility_width = (icon_size + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x) * 2.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - utility_width);

    if(Ui::IconButton(context_ref.icons_ref, Ui::IconId::Parameters, "##metrics", icon_size, theme_ref.text_muted))
    {
        context_ref.app_ref.show_metrics_window = !context_ref.app_ref.show_metrics_window;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Renderer metrics");
    }

    ImGui::SameLine();
    if(Ui::IconButton(context_ref.icons_ref, Ui::IconId::Settings, "##settings", icon_size, theme_ref.text_muted))
    {
        context_ref.app_ref.show_style_editor = !context_ref.app_ref.show_style_editor;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Style editor");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if(scene_to_close != static_cast<size_t>(-1))
    {
        context_ref.app_ref.closeScene(scene_to_close);
    }
}

void DrawHomePage(FrameContext& context_ref)
{
    const Ui::Palette& theme_ref = Ui::gui_theme;

    ImGui::BeginChild("HomePage", ImVec2(0, 0), ImGuiChildFlags_None);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + available.y * 0.32f);

    const char* title_ptr = "DeckCAD";
    Ui::CenterNextItem(ImGui::CalcTextSize(title_ptr).x);
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_base), "%s", title_ptr);

    const char* subtitle_ptr = "Fast, offline, parametric 3D CAD.";
    Ui::CenterNextItem(ImGui::CalcTextSize(subtitle_ptr).x);
    ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "%s", subtitle_ptr);

    ImGui::Dummy(ImVec2(0, 16));

    const char* button_label_ptr = "New Scene";
    const f32 button_width = ImGui::CalcTextSize(button_label_ptr).x + ImGui::GetStyle().FramePadding.x * 4.0f;
    Ui::CenterNextItem(button_width);
    if(ImGui::Button(button_label_ptr, ImVec2(button_width, 0)))
    {
        context_ref.app_ref.newScene();
    }

    if(!context_ref.app_ref.scenes.empty())
    {
        ImGui::Dummy(ImVec2(0, 24));
        const char* recent_label_ptr = "Open scenes";
        Ui::CenterNextItem(ImGui::CalcTextSize(recent_label_ptr).x);
        ImGui::TextColored(Ui::ToImVec4(theme_ref.text_muted), "%s", recent_label_ptr);

        for(size_t index = 0; index < context_ref.app_ref.scenes.size(); ++index)
        {
            const std::string& name_ref = context_ref.app_ref.scenes[index].name;
            const f32 width = ImGui::CalcTextSize(name_ref.c_str()).x + ImGui::GetStyle().FramePadding.x * 4.0f;
            Ui::CenterNextItem(width);
            ImGui::PushID(static_cast<int>(index));
            if(ImGui::Button(name_ref.c_str(), ImVec2(width, 0)))
            {
                context_ref.app_ref.active_tab = static_cast<u32>(index) + 1;
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
}

} // namespace Core
