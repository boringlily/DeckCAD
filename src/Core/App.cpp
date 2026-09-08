#include "App.h"
#include "Widgets.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Core
{
namespace AppInternal
{

    constexpr const char* DOCK_SPACE_ID = "DeckCADDockSpace";
    constexpr const char* VIEWPORT_PANEL = "Viewport";
    constexpr const char* EXPLORER_PANEL = "Explorer";
    constexpr const char* TOOLBOX_PANEL = "Toolbox";

    /// Applied to the whole hierarchy: nothing can be undocked, split or
    /// docked over. The layout the app builds is the layout it keeps.
    constexpr ImGuiDockNodeFlags DOCK_SPACE_FLAGS = static_cast<int>(ImGuiDockNodeFlags_NoUndocking)
        | ImGuiDockNodeFlags_NoDocking;

    /// Per-node decorations a fixed panel has no use for.
    constexpr ImGuiDockNodeFlags DOCK_NODE_FLAGS = ImGuiDockNodeFlags_NoTabBar
        | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton;

    /// Strips the tab bar and window menu from a leaf node.
    void StripNodeDecorations(ImGuiID node_id)
    {
        if(ImGuiDockNode* node_ptr = ImGui::DockBuilderGetNode(node_id))
        {
            node_ptr->LocalFlags |= DOCK_NODE_FLAGS;
        }
    }

    /// Arranges the panels the first time a dockspace is created.
    void BuildDefaultLayout(ImGuiID dockspace_id)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID remaining = dockspace_id;
        ImGuiID left_id = ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.20f, nullptr, &remaining);
        ImGuiID right_id = ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.25f, nullptr, &remaining);

        StripNodeDecorations(left_id);
        StripNodeDecorations(right_id);
        StripNodeDecorations(remaining);

        ImGui::DockBuilderDockWindow(EXPLORER_PANEL, left_id);
        ImGui::DockBuilderDockWindow(TOOLBOX_PANEL, right_id);
        ImGui::DockBuilderDockWindow(VIEWPORT_PANEL, remaining);

        ImGui::DockBuilderFinish(dockspace_id);
    }

} // namespace AppInternal
using namespace AppInternal;

void BuildFrame(FrameContext& context_ref)
{
    const ImGuiViewport* main_viewport_ptr = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(main_viewport_ptr->WorkPos);
    ImGui::SetNextWindowSize(main_viewport_ptr->WorkSize);
    ImGui::SetNextWindowViewport(main_viewport_ptr->ID);

    ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DeckCADHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    DrawHeaderBar(context_ref);

    if(context_ref.app_ref.hasOpenScene())
    {
        // Reserve room for the status bar before handing the rest to the dockspace.
        const f32 status_bar_height = ImGui::GetFrameHeightWithSpacing();
        const ImVec2 dock_size(ImGui::GetContentRegionAvail().x,
            ImGui::GetContentRegionAvail().y - status_bar_height);

        ImGuiID dockspace_id = ImGui::GetID(DOCK_SPACE_ID);
        const bool first_use = ImGui::DockBuilderGetNode(dockspace_id) == nullptr;
        ImGui::DockSpace(dockspace_id, dock_size, DOCK_SPACE_FLAGS);
        if(first_use)
        {
            BuildDefaultLayout(dockspace_id);
        }

        DrawExplorerPanel(context_ref);
        DrawViewportPanel(context_ref);
        DrawToolboxPanel(context_ref);
        DrawStatusBar(context_ref);
    }
    else
    {
        DrawHomePage(context_ref);
    }

    ImGui::End();

    if(context_ref.app_ref.show_metrics_window)
    {
        ImGui::ShowMetricsWindow(&context_ref.app_ref.show_metrics_window);
    }
    if(context_ref.app_ref.show_style_editor)
    {
        ImGui::Begin("Style Editor", &context_ref.app_ref.show_style_editor);
        ImGui::ShowStyleEditor();
        ImGui::End();
    }
}

} // namespace Core
