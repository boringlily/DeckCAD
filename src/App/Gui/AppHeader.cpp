#include "Graphics.h"
#include <format>

void LayoutAppHeader(AppState& app)
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
            .backgroundColor = Clay_Hovered() || app.IsHomeLayerActive() ? GuiTheme.BgLight : GuiTheme.BgDark,
            .cornerRadius = { 8u },
        })
        {
            if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                app.ActivateHomeLayer();
            }
            DrawIcon(IconId::Home, GuiTheme.TextBase);
        };

        SceneList& scene_list = app.GetSceneList();

        if (scene_list.size()) {
            u32 scene_id { 0 };
            for (auto& scene : scene_list) {
                bool active = scene_id == app.GetActiveSceneId();

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
                        if (app.TryActivateScene(scene_id)) {
                            app.ActivateSceneLayer();
                        } else {
                            // TODO: Show error "ERROR: Failed to activate scene {id}"
                        }
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

                app.CreateNewScene();
            }
            DrawIcon(IconId::Plus, GuiTheme.TextBase);
        };
    };
}