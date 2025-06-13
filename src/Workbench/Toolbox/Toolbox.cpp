#include "Workbench.h"


void DrawToolbox(Scene &scene)
{
    CLAY({.id = CLAY_ID("WorkbenchToolbox"),
            .layout =
                {
                    .sizing = layoutExpandMinWidth(200, 300),
                },
            .backgroundColor = COLOR_BLUE});
}