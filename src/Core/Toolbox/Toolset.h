#pragma once
#include "AppMemory.h"
#include "clay.h"
#include "DumbTypes.h"
#include <print>

using DrawToolsetFunc = void (*)();

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

/// @brief Used as a placeholder for tool buttons that aren't implemented.
static void ToolPlaceholderFunction()
{
    CLAY({ .layout = {
               .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
               .padding = CLAY_PADDING_ALL(4),
               .childGap = 8,
               .layoutDirection = CLAY_TOP_TO_BOTTOM,
           },
        .backgroundColor = GuiTheme.BgBase })
    {
        CLAY_TEXT(CLAY_STRING("Hi, you have clicked a placeholder. This tool currently is in-development and will be enabled once it is ready"), &TextStyle.body);
        CLAY(ToolboxButtonConfig(false))
        {
            CLAY_TEXT(CLAY_STRING("Ok, I understand."), &TextStyle.buttonActive);

            auto button_action = [](Clay_ElementId element_id, Clay_PointerData pointer_data, intptr_t user_data) -> void {
                if (pointer_data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
                    app_global->GetCurrentScene().toolbox.active_tool_status = Toolbox::ToolStatus::done;
                }
            };

            Clay_OnHover(button_action, 0u);
        };
    }
}

constexpr void BeginToolGroup(Clay_String group_name)
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

constexpr void EndToolGroup()
{
    Clay__CloseElement();
}

constexpr void ToolSelectButton(std::string_view name, IconId icon, Toolbox::ToolFunctionPointer function)
{
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
        auto activate_tool_button = [](Clay_ElementId element_id, Clay_PointerData pointer_data, intptr_t user_data) -> void {
            if (pointer_data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
                app_global->GetCurrentScene().toolbox.active_tool_status = Toolbox::ToolStatus::active;
                app_global->GetCurrentScene().toolbox.active_tool = reinterpret_cast<Toolbox::ToolFunctionPointer>(user_data);
            }
        };

        Clay_OnHover(activate_tool_button, reinterpret_cast<intptr_t>(function));
    };
}