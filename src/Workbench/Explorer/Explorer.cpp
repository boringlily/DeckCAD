#include "Workbench.h"

void DrawExplorer(Scene &scene)
{
    CLAY({.id = CLAY_ID("WorkbenchExplorer"),
            .layout =
                {
                    .sizing = layoutExpandMinWidth(300, 500),
                },
            .backgroundColor = COLOR_RED});
}