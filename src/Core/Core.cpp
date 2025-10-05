#include "Core.h"
#include "Graphics.h"
#include "Scene/Scene.h"
#include "Workbench.cpp"
#include "AppHeader.cpp"
#include <print>

#ifdef __cplusplus
extern "C" {
#endif

CORE_API
void CoreInit(AppMemory& app)
{
    app_global = &app;
    std::println("app_global memory initialized.");
    assert(app_global && "Application memory null");
}

CORE_API
void CoreUpdate()
{
    AppMemory& app = *app_global;

    Graphics::BeginFrame();

    CLAY(
        {
            .id = CLAY_ID("OuterContainer"),
            .layout = { .sizing = LAYOUT_EXPAND,
                .layoutDirection = CLAY_TOP_TO_BOTTOM },
        })
    {
        LayoutAppHeader();

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
