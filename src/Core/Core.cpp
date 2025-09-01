#include "Workbench.cpp"
#include "Graphics.h"
#include "Scene/Scene.h"
#include "Core.h"
#include <format>

#ifdef __cplusplus
extern "C" {
#endif

CORE_API
void CoreInit(AppMemory& app)
{
    app_global = &app;
    printf("app_global memory initialized.\n");
    assert(app_global && "Application memory null");
}

// static AppMemory * app_global{nullptr};
void HeaderButtonAction(Clay_ElementId element_id, Clay_PointerData pointer_data, intptr_t user_data)
{
    if (pointer_data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        app_global->header_state = (u32)user_data;
    }
};

void AddSceneButtonAction(Clay_ElementId element_id, Clay_PointerData pointer_data, intptr_t user_data)
{
    if (pointer_data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        u32 new_scene_id { static_cast<u32>(app_global->scenes.size() + 1u) };
        app_global->scenes.emplace_back(std::format("Scene %4u", new_scene_id));
        app_global->header_state = new_scene_id;
    }
};

CORE_API
void CoreUpdate()
{
    AppMemory& app = *app_global;

    Graphics::BeginFrame();

    CLAY(
        {
            .id = CLAY_ID("OuterContainer"),
            .layout = {
                .sizing = LAYOUT_EXPAND,
                .layoutDirection = CLAY_TOP_TO_BOTTOM },
        })
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
                Clay_OnHover(HeaderButtonAction, 0);
                DrawIcon(IconId::Home, GuiTheme.TextBase);
            };

            if (app_global->scenes.size()) {
                u32 scene_id { 1 };
                for (auto& scene : app_global->scenes) {
                    auto active = [scene_id]() -> bool {
                        return scene_id == app_global->header_state;
                    };

                    CLAY({
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                            .padding = LAYOUT_PADDING_RECTANGLE_MEDIUM,
                            .childGap = 8,
                            .childAlignment = ALIGN_CENTER,
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        },
                        .backgroundColor = Clay_Hovered() || active() ? GuiTheme.BgLight : GuiTheme.BgDark,
                        .cornerRadius = { 4u },
                    })
                    {
                        Clay_OnHover(HeaderButtonAction, scene_id);
                        CLAY_TEXT(CLAY_STRING("SCENE"), active() ? &TextStyle.buttonActive : &TextStyle.buttonMuted);

                        if (active()) {
                            CLAY({
                                .layout = {
                                    .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                                    .childAlignment = ALIGN_CENTER },
                                .backgroundColor = Clay_Hovered() ? GuiTheme.AlertDanger : GuiTheme.BgLight,
                                .cornerRadius = { 4u },
                            })
                            {
                                DrawIcon(IconId::Exit, Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.TextBase);
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
                Clay_OnHover(AddSceneButtonAction, 0);
                DrawIcon(IconId::Plus, GuiTheme.TextBase);
            };
        };

        if (app.header_state) {
            DrawWorkbench(app);
        } else {
            CLAY(
                { .id = CLAY_ID("HomePage"),
                    .layout = {
                        .sizing = LAYOUT_EXPAND,
                        .padding = CLAY_PADDING_ALL(8),
                        .childGap = 8,
                        .childAlignment = ALIGN_CENTER,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                    .backgroundColor = GuiTheme.BgBase })
            {
                CLAY_TEXT(CLAY_STRING("This is going to be the homepage."), &TextStyle.title);
            };
        }
    };

    Graphics::EndFrame();
}

#ifdef __cplusplus
}
#endif
