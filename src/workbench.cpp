#include "canvas.cpp"
#include "ui_components.h"
#include "experimental.h"

void DrawWorkbench(Scene &scene) 
{

/// Workbench components
CLAY({.id = CLAY_ID("Workbench"),
        .layout =
            {
                .sizing = layoutExpand,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
        .backgroundColor = {255, 255, 255, 255}})
        
        {

    CLAY({.id = CLAY_ID("WorkbenchInner"),
        .layout = {.sizing = layoutExpand,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT},
        .backgroundColor = {0, 0, 0, 255}}) {
    CLAY({.id = CLAY_ID("WorkbenchExplorer"),
            .layout =
                {
                    .sizing = layoutExpandMinWidth(300, 500),
                },
            .backgroundColor = COLOR_RED});


                DrawCanvas(scene);


    CLAY({.id = CLAY_ID("WorkbenchToolbox"),
            .layout =
                {
                    .sizing = layoutExpandMinWidth(200, 400),
                },
            .backgroundColor = COLOR_BLUE});
    };

    CLAY({.id = CLAY_ID("WorkbenchFooter"),
        .layout =
            {
                .sizing = {.width = CLAY_SIZING_GROW(),
                            .height = CLAY_SIZING_FIXED(100)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
        .backgroundColor = COLOR_LIGHT});
};
}