#include "Graphics.h"
#include <format>

void LayoutAppHeader()
{
    CLAY(
        { .id = CLAY_ID("Header"),
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                .padding = CLAY_PADDING_ALL(4),
                .childGap = 8,
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = GuiTheme.BgDark })
    {
        CLAY({
            .id = CLAY_ID("Header::ButtonHome"),
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                .padding = CLAY_PADDING_ALL(4),
                .childGap = 8,
                .childAlignment = ALIGN_CENTER,
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = Clay_Hovered() || app_global->header_state == 0 ? GuiTheme.BgLight : GuiTheme.BgDark,
            .cornerRadius = { 8u },
        })
        {
            if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                app_global->header_state = 0;
            }
            DrawIcon(IconId::Home, GuiTheme.TextBase);
        };

        if (app_global->scenes.size()) {
            u32 scene_id { 1 };
            for (auto& scene : app_global->scenes) {
                bool active = scene_id == app_global->header_state;

                CLAY({
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() },
                        .padding = CLAY_PADDING_ALL(4),
                        .childGap = 4,
                        .childAlignment = ALIGN_CENTER,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                    .backgroundColor = Clay_Hovered() || active ? GuiTheme.BgLight : GuiTheme.BgDark,
                    .cornerRadius = { 4u },
                })
                {

                    static Clay_String filename {};
                    filename = { false, static_cast<s32>(scene.filename.size()), scene.filename.c_str() };

                    if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                        app_global->header_state = scene_id;
                    }

                    CLAY_TEXT(filename, active ? &TextStyle.buttonActive : &TextStyle.buttonMuted);

                    if (active) {
                        CLAY({
                            .layout = {
                                .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                                .childAlignment = ALIGN_CENTER },
                            .backgroundColor = Clay_Hovered() ? GuiTheme.BgBase : GuiTheme.BgLight,
                            .cornerRadius = { 4u },
                        })
                        {
                            DrawIcon(IconId::Exit, Clay_Hovered() ? GuiTheme.AlertDanger : GuiTheme.TextBase);
                        };
                    }
                };
                scene_id++;
            }
        }

        CLAY({
            .id = CLAY_ID("Header::ButtonNewScene"),
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                .padding = CLAY_PADDING_ALL(4),
                .childGap = 8,
                .childAlignment = ALIGN_CENTER,
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgDark,
            .cornerRadius = { 8u },
        })
        {
            if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                u32 new_scene_id { static_cast<u32>(app_global->scenes.size() + 1u) };
                app_global->scenes.emplace_back(std::format("Untitled {}", new_scene_id));
                app_global->header_state = new_scene_id;
            }
            DrawIcon(IconId::Plus, GuiTheme.TextBase);
        };
    };
}