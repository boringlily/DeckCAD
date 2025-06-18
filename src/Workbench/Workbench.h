#pragma once
#include "clay.h"
#include "ui_components.h"
class Scene;

void DrawExplorer(Scene& scene);
void DrawCanvas(Scene& scene);
void DrawToolbox(Scene& scene);

inline void DrawWorkbench(Scene& scene)
{

    /// Workbench components
    CLAY({ .id = CLAY_ID("Workbench"),
        .layout = {
            .sizing = layoutExpand,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = { 255, 255, 255, 255 } })
    {

        CLAY(
            { .id = CLAY_ID("WorkbenchInner"),
                .layout = { .sizing = layoutExpand,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT },
                .backgroundColor = { 0, 0, 0, 255 } })
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
                .backgroundColor = COLOR_LIGHT });
    };
}
