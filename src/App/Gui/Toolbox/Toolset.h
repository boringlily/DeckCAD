#pragma once
#include "Scene.h"
#include "clay.h"
#include "DTL.h"

inline Clay_ElementDeclaration ToolboxButtonConfig(bool active)
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

inline void BeginToolGroup(Clay_String group_name)
{
    Clay_ElementDeclaration group = {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(8),
            .childGap = 8,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .cornerRadius = CLAY_CORNER_RADIUS(10),
        .border = (Clay_BorderElementConfig) { .color = GuiTheme.BorderBase, .width = { 2, 2, 2, 2, 0 } }
    };

    Clay__OpenElement();
    Clay__ConfigureOpenElement(group);

    CLAY_TEXT(group_name, &TextStyle.subtitle);
}

inline void EndToolGroup()
{
    Clay__CloseElement();
}

inline bool ToolSelectButton(std::string_view name, IconId icon)
{
    bool clicked = false;
    CLAY({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(4),
            .childGap = 4,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgBase,
        .cornerRadius = CLAY_CORNER_RADIUS(8),
    })
    {
        DrawIcon(icon, GuiTheme.TextBase);

        static Clay_String tool_name {};
        tool_name = { true, static_cast<s32>(name.size()), name.data() };
        CLAY_TEXT(tool_name, &TextStyle.body);

        if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
            clicked = true;
        }
    };
    return clicked;
}
