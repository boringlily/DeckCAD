#include "Workbench.h"


void DrawToolbox(Scene &scene)
{
    CLAY({.id = CLAY_ID("WorkbenchToolbox"),
            .layout =
                {
                    .sizing = layoutExpandMinWidth(200, 400),
                },
            .backgroundColor = COLOR_BLUE});
}