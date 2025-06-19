#pragma once
#include "ClayPrimitives.h"

class Scene;

void DrawExplorer(Scene& scene);
void DrawCanvas(Scene& scene);
void DrawToolbox(Scene& scene);

inline void DrawWorkbench(Scene& scene)
{

    /// Workbench components
    CLAY({ .id = CLAY_ID("Workbench"),
        .layout = {
            .sizing = LAYOUT_EXPAND,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        } })
    {

        CLAY(
            { .id = CLAY_ID("WorkbenchInner"),
                .layout = { .sizing = LAYOUT_EXPAND,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT } })
        {
            DrawExplorer(scene);

            DrawCanvas(scene);

            DrawToolbox(scene);
        };

        CLAY(
            { .id = CLAY_ID("WorkbenchFooter"),
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(60) },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
                .backgroundColor = COLOR_LIGHT_GREY });
    };
}
