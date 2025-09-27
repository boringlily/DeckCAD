#include "Components.h"
#include <string>
#include <array>
#include <initializer_list>

// Toolbox tools
// Inspector - Provide a mechanism to view some kind of data about different geometry, does not modify model.
// ModelCommand - Creates commands in the command list for the cad kernel that dictate model generation.
// MetaData - Non-historic actions that modify metadata in the ModelCommands and affects how different models are displayed on the ui.

// Flags
enum ToolContext : u32 {
    Solid = 0x01,
    Sketch = 0x02,
};

enum ToolStatus : u32 {
    active,
    done
};

inline bool MouseClickedAndHovered()
{
    return Clay_Hovered() && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

Clay_ElementDeclaration ToolboxButtonConfig(bool active)
{
    return {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(4),
            .childAlignment = ALIGN_CENTER,
        },
        .backgroundColor = active || Clay_Hovered() ? GuiTheme.BgBase : GuiTheme.BgLight,
        .cornerRadius = { 4u },
    };
}

using DrawToolFunc = ToolStatus (*)();
using DrawToolsetFunc = void (*)();

static ToolStatus PlaceholderFunc()
{
    ToolStatus status = ToolStatus::active;

    CLAY({ .layout = {
               .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
               .padding = CLAY_PADDING_ALL(4),
               .childGap = 8,
               .layoutDirection = CLAY_TOP_TO_BOTTOM,
           },
        .backgroundColor = GuiTheme.BgBase })
    {
        CLAY_TEXT(CLAY_STRING("Hi, you have clicked a placeholder."), &TextStyle.title);
        CLAY(ToolboxButtonConfig(false))
        {
            CLAY_TEXT(CLAY_STRING("Done"), &TextStyle.buttonActive);

            if (MouseClickedAndHovered()) {
                status = done;
            }
        };
    }

    return status;
}

struct Toolset {
    std::string_view name;
    ToolContext tab_visible_context { 0xFF }; // In what contexts the toolset is available.
    DrawToolsetFunc function = nullptr;
};

ToolContext context { Solid };
u32 active_toolset { 0 };
DrawToolFunc active_tool { nullptr };

constexpr void BeginToolGroup(Clay_String group_name)
{
    Clay_ElementDeclaration group = {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(4),
            .childGap = 8,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = GuiTheme.BgBase
    };

    Clay__OpenElement();
    Clay__ConfigureOpenElement(group);

    CLAY_TEXT(group_name, &TextStyle.subtitle);
}

void EndToolGroup()
{
    Clay__CloseElement();
}

constexpr void ToolSelectButton(std::string_view name, IconId icon, DrawToolFunc function)
{
    CLAY({
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(4),
            .childGap = 4,
            .childAlignment = ALIGN_CENTER,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = Clay_Hovered() ? GuiTheme.BgBase : GuiTheme.BgLight,
        .cornerRadius = { 4u },
    })
    {

        DrawIcon(icon, GuiTheme.TextBase);

        static Clay_String tool_name {};
        tool_name = { true, static_cast<s32>(name.size()), name.data() };

        CLAY_TEXT(tool_name, &TextStyle.body);

        if (Clay_Hovered() && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            active_tool = function;
        }
    };
}

void SketchToolSet()
{
    BeginToolGroup(CLAY_STRING("Draw"));
    ToolSelectButton("Line", Unknown, &PlaceholderFunc);
    EndToolGroup();
}

std::array<Toolset, 2> toolset_list = {
    // (Toolset){
    //     .name = "Solid",
    //     .tab_visible_context = ToolContext::Solid,
    // },
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

        //
        if (active_tool == nullptr) {
            // Tabs
            CLAY({ .id = TOOLSET_TABS_ID,
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                    .padding = CLAY_PADDING_ALL(4),
                    .childGap = 8,
                },
                .backgroundColor = GuiTheme.BgBase })
            {

                u32 tabs {};

                for (auto& toolset : toolset_list) {

                    bool tab_active { tabs == active_toolset };

                    CLAY({
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                            .padding = CLAY_PADDING_ALL(4),
                            .childAlignment = ALIGN_CENTER },
                        .backgroundColor = tab_active || Clay_Hovered() ? GuiTheme.BgBase : GuiTheme.BgLight,
                        .cornerRadius = { 4u },
                    })
                    {
                        static Clay_String tab_name {};
                        tab_name = { true, static_cast<s32>(toolset.name.size()), toolset.name.data() };

                        CLAY_TEXT(tab_name, &TextStyle.buttonActive);

                        if (Clay_Hovered() && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                            active_toolset = tabs;
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
                    .childGap = 8,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = GuiTheme.BgBase })
            {
                auto toolset = toolset_list[active_toolset];

                if (toolset.function) {
                    toolset.function();
                }

                // Groups
                // for(auto &group: toolset->groups)
                // {
                //     CLAY({
                //         .layout = {
                //             .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                //             .padding = CLAY_PADDING_ALL(4),
                //             .childGap = 8,
                //             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                //         },
                //         .backgroundColor = GuiTheme.BgBase })
                //         {
                //             // Tools
                //             for(auto &tool: group.tools)
                //             {

                //

                //             }
                //         }
                // }
            };
        } else {
            if (active_tool() == ToolStatus::done) {
                active_tool = nullptr;
            }
        }
    }
}