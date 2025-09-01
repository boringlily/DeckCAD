#include "Scene/Scene.h"
#include "Components.h"
#include "Explorer/Explorer.cpp"
#include "Canvas/Canvas.cpp"
#include "Toolbox/Toolbox.cpp"
#include "AppMemory.h"

inline void DrawWorkbench(AppMemory& app)
{
    /// Workbench components
    CLAY({ .id = CLAY_ID("Workbench"),
        .layout = {
            .sizing = LAYOUT_EXPAND,
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        } })
    {
        CLAY(
            { .id = CLAY_ID("WorkbenchInner"),
                .layout = { .sizing = LAYOUT_EXPAND,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT } })
        {
            DrawExplorer(app.GetCurrentScene());

            DrawCanvas(app.GetCurrentScene());

            DrawToolbox(app.GetCurrentScene());
        };

        CLAY(
            { .id = CLAY_ID("WorkbenchFooter"),
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                    .padding = CLAY_PADDING_ALL(8),
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
                .backgroundColor = GuiTheme.BgDark })
        {
            DrawIcon(IconId::Settings, GuiTheme.TextBase);
            CLAY_TEXT(CLAY_STRING("Active Mode: {UNKNOWN}"), &TextStyle.body);
        };
    };
}
