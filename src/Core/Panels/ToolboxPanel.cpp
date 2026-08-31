#include "App.h"
#include "Widgets.h"

#include <array>
#include <imgui.h>

// Toolbox tools
// Inspector    - View data about geometry; never modifies the model.
// ModelCommand - Appends commands to the CAD kernel's command list.
// MetaData     - Non-historic actions that change how models are displayed.

namespace Core
{
namespace ToolboxPanelInternal
{

    /// Displays a placeholder screen for a tool that has not been implemented yet.
    void ToolPlaceholder(FrameContext& context_ref)
    {
        Scene& scene_ref = context_ref.app_ref.getCurrentScene();

        ImGui::TextWrapped(
            "You have clicked a placeholder. This tool is still in development "
            "and will be enabled once it is ready.");
        ImGui::Dummy(ImVec2(0, 4));

        if(ImGui::Button("Ok, I understand", ImVec2(-FLT_MIN, 0)))
        {
            scene_ref.toolbox.dismissTool();
        }
    }

    /**
     * @brief Draws a full-width selectable entry for one tool.
     * @return True if the entry was clicked this frame.
     */
    bool ToolSelectButton(FrameContext& context_ref, const char* name_ptr, Ui::IconId icon, Toolbox::ToolFunction tool)
    {
        const f32 icon_size = ImGui::GetFontSize();
        bool clicked = false;

        ImGui::PushID(name_ptr);
        ImGui::BeginGroup();

        Ui::IconImage(context_ref.icons_ref, icon, icon_size, Ui::gui_theme.text_base);
        ImGui::SameLine();
        if(ImGui::Selectable(name_ptr, false, ImGuiSelectableFlags_None))
        {
            context_ref.app_ref.getCurrentScene().toolbox.activateTool(tool);
            clicked = true;
        }

        ImGui::EndGroup();
        ImGui::PopID();
        return clicked;
    }

    void BeginToolGroup(const char* name_ptr, bool& out_open_ref)
    {
        out_open_ref = ImGui::CollapsingHeader(name_ptr, ImGuiTreeNodeFlags_DefaultOpen);
        if(out_open_ref)
        {
            ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.5f);
        }
    }

    void EndToolGroup(bool open)
    {
        if(open)
        {
            ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.5f);
        }
    }

    void SolidToolset(FrameContext& context_ref)
    {
        bool open = false;
        BeginToolGroup("Create", open);
        if(open)
        {
            ToolSelectButton(context_ref, "Extrude", Ui::IconId::Unknown, &ToolPlaceholder);
            ToolSelectButton(context_ref, "Revolve", Ui::IconId::Unknown, &ToolPlaceholder);
        }
        EndToolGroup(open);

        BeginToolGroup("Modify", open);
        if(open)
        {
            ToolSelectButton(context_ref, "Fillet", Ui::IconId::Unknown, &ToolPlaceholder);
            ToolSelectButton(context_ref, "Chamfer", Ui::IconId::Unknown, &ToolPlaceholder);
        }
        EndToolGroup(open);
    }

    void SketchToolset(FrameContext& context_ref)
    {
        Scene& scene_ref = context_ref.app_ref.getCurrentScene();
        const bool in_sketch_context = scene_ref.toolbox.context == Toolbox::Sketch;

        const f32 icon_size = ImGui::GetFontSize();
        Ui::IconImage(context_ref.icons_ref, in_sketch_context ? Ui::IconId::Check : Ui::IconId::Plus,
            icon_size, Ui::gui_theme.text_base);
        ImGui::SameLine();
        if(ImGui::Button(in_sketch_context ? "Finish Sketch" : "Create Sketch", ImVec2(-FLT_MIN, 0)))
        {
            scene_ref.toolbox.context = in_sketch_context ? Toolbox::Solid : Toolbox::Sketch;
        }

        if(!in_sketch_context)
        {
            return;
        }

        ImGui::Dummy(ImVec2(0, 4));

        bool open = false;
        BeginToolGroup("Draw", open);
        if(open)
        {
            ToolSelectButton(context_ref, "Line", Ui::IconId::Unknown, &ToolPlaceholder);
        }
        EndToolGroup(open);

        BeginToolGroup("Dimensions", open);
        if(open)
        {
            ToolSelectButton(context_ref, "Length", Ui::IconId::Unknown, &ToolPlaceholder);
            ToolSelectButton(context_ref, "Angle", Ui::IconId::Unknown, &ToolPlaceholder);
        }
        EndToolGroup(open);

        BeginToolGroup("Constraints", open);
        if(open)
        {
            ToolSelectButton(context_ref, "Coincident", Ui::IconId::Unknown, &ToolPlaceholder);
        }
        EndToolGroup(open);
    }

    void InspectToolset(FrameContext& context_ref)
    {
        bool open = false;
        BeginToolGroup("Measure", open);
        if(open)
        {
            ToolSelectButton(context_ref, "Distance", Ui::IconId::Unknown, &ToolPlaceholder);
        }
        EndToolGroup(open);
    }

    struct Toolset
    {
        const char* name_ptr;
        void (*function)(FrameContext&);
    };

    constexpr std::array<Toolset, 3> TOOLSETS {
        Toolset { "Solid", &SolidToolset },
        Toolset { "Sketch", &SketchToolset },
        Toolset { "Inspect", &InspectToolset },
    };

} // namespace ToolboxPanelInternal
using namespace ToolboxPanelInternal;

void DrawToolboxPanel(FrameContext& context_ref)
{
    Scene& scene_ref = context_ref.app_ref.getCurrentScene();

    if(!ImGui::Begin("Toolbox", nullptr, PANEL_WINDOW_FLAGS))
    {
        ImGui::End();
        return;
    }

    // active tool takes over the whole panel until it dismisses itself
    if(scene_ref.toolbox.hasActiveTool())
    {
        scene_ref.toolbox.active_tool(context_ref);
        ImGui::End();
        return;
    }

    if(ImGui::BeginTabBar("ToolsetTabs", ImGuiTabBarFlags_None))
    {
        for(size_t index = 0; index < TOOLSETS.size(); ++index)
        {
            if(ImGui::BeginTabItem(TOOLSETS[index].name_ptr))
            {
                scene_ref.toolbox.active_toolset = static_cast<u32>(index);
                ImGui::Dummy(ImVec2(0, 4));

                if(TOOLSETS[index].function)
                {
                    TOOLSETS[index].function(context_ref);
                }
                else
                {
                    ImGui::TextUnformatted("The toolset function is not assigned.");
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace Core
