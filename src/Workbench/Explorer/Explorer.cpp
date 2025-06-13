#include "Workbench.h"

void DrawExplorer(Scene &scene)
{
    CLAY({.id = CLAY_ID("WorkbenchExplorer"),
            .layout =
                {
                    .sizing = layoutExpandMinWidth(100, 350),
                },
            .backgroundColor = COLOR_RED});
}