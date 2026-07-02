#include "App.h"
#include "Graphics.h"
#include "Gui/Workbench.cpp"
#include "Gui/AppHeader.cpp"
#include <print>

#include <tracy/Tracy.hpp>

#ifdef __cplusplus
extern "C" {
#endif

APP_API
void AppUpdate(AppState& app)
{
    ZoneScoped;

    Graphics::BeginFrame();

    CLAY(
        {
            .id = CLAY_ID("OuterContainer"),
            .layout = { .sizing = LAYOUT_EXPAND,
                .layoutDirection = CLAY_TOP_TO_BOTTOM },
        })
    {
        LayoutAppHeader(app);

        switch (app.GetActiveLayer()) {
        case AppLayer::Home_Layer:
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

            break;
        case AppLayer::Settings_Layer:

            CLAY(
                { .id = CLAY_ID("SettingsPage"),
                    .layout = {
                        .sizing = LAYOUT_EXPAND,
                        .padding = CLAY_PADDING_ALL(8),
                        .childGap = 8,
                        .childAlignment = ALIGN_CENTER,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                    .backgroundColor = GuiTheme.BgBase })
            {
                CLAY_TEXT(CLAY_STRING("This is going to be the global settings page."), &TextStyle.title);
            };

            break;
        case AppLayer::Scene_Layer:

            Scene* scene_ptr = app.GetActiveScene();
            if (scene_ptr != nullptr) {
                DrawWorkbench(*scene_ptr);
            }

            break;
        }
    };

    Graphics::EndFrame();
}

#ifdef __cplusplus
}
#endif
