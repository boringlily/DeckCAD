#include "Components.h"
#include "AppMemory.h"
#include <string>
#include <array>
#include <initializer_list>

#include "SketchToolset.cpp"

// Toolbox tools
// Inspector - Provide a mechanism to view some kind of data about different geometry, does not modify model.
// ModelCommand - Creates commands in the command list for the cad kernel that dictate model generation.
// MetaData - Non-historic actions that modify metadata in the ModelCommands and affects how different models are displayed on the ui.

struct Toolset {
    std::string_view name;
    Toolbox::Context tab_visible_context { 0xFF }; // Visibility control for toolset context
    DrawToolsetFunc function = nullptr;
};

std::array<Toolset, 3> toolset_list = {
    (Toolset) {
        .name = "Solid",
        .tab_visible_context = Toolbox::Context::Solid,
    },
    (Toolset) {
        .name = "Sketch",
        .function = &SketchToolSet },
    (Toolset) {
        .name = "Inspect" }
};

void DrawToolbox(Scene& scene)
{
    static Clay_ElementId TOOLBOX_ID = CLAY_ID("Toolbox");
    static Clay_ElementId TOOLSET_TABS_ID = CLAY_ID("ToolsetTabs");
    static Clay_ElementId TOOLSET_ID = CLAY_ID("Toolset");

    static constexpr float TOOLBOX_MIN_SHRINK_WIDTH { 200 };
    static constexpr float TOOLBOX_MAX_GROW_WIDTH { 300 };

    CLAY({ .id = TOOLBOX_ID,
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(TOOLBOX_MIN_SHRINK_WIDTH, TOOLBOX_MAX_GROW_WIDTH),
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = GuiTheme.BgBase })
    {

        if (scene.toolbox.active_tool == nullptr) {

            // Draw toolset tabs
            CLAY({ .id = TOOLSET_TABS_ID,
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                    .padding = CLAY_PADDING_ALL(4),
                    .childGap = 8,
                },
                .backgroundColor = GuiTheme.BgBase })
            {

                u32 tabs {};

                for (auto& toolset : toolset_list) {

                    bool tab_active { tabs == scene.toolbox.active_toolset };

                    // Toolset Tab Element
                    CLAY({ .layout = {
                               .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                               .padding = LAYOUT_PADDING_SIDES_AND_TOP(16, 4),
                               .childAlignment = ALIGN_CENTER },
                        .backgroundColor = tab_active || Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgBase,
                        .cornerRadius = CLAY_CORNER_RADIUS(8u),
                        .border = (Clay_BorderElementConfig) { .color = GuiTheme.AccentPrimary, .width = (tab_active ? (Clay_BorderWidth) { 2, 2, 2, 2, 0 } : (Clay_BorderWidth) { 0 }) } })
                    {
                        static Clay_String tab_name {};
                        tab_name = { true, static_cast<s32>(toolset.name.size()), toolset.name.data() };

                        CLAY_TEXT(tab_name, tab_active ? &TextStyle.buttonActive : &TextStyle.buttonMuted);

                        if (Clay_Hovered() && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                            scene.toolbox.active_toolset = tabs;
                        }
                    };
                    tabs++;
                }
            }

            // Groups and Tools
            CLAY({ .id = TOOLSET_ID,
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                    .padding = CLAY_PADDING_ALL(4),
                    .childGap = 16,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = GuiTheme.BgBase })
            {
                auto toolset = toolset_list[scene.toolbox.active_toolset];

                if (toolset.function) {
                    toolset.function();
                } else {
                    CLAY_TEXT(CLAY_STRING("The toolset function is not assigned."), &TextStyle.body);
                }
            };
        } else {
            scene.toolbox.active_tool();
            if (scene.toolbox.active_tool_status == Toolbox::ToolStatus::done) {
                scene.toolbox.active_tool = nullptr;
            }
        }
    }
}