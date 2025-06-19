#include "Workbench.h"

void DrawToolbox(Scene& scene)
{
    static constexpr float TOOLBOX_MIN_SHRINK_WIDTH { 200 };
    static constexpr float TOOLBOX_MAX_GROW_WIDTH { 300 };

    CLAY({ .id = CLAY_ID("WorkbenchToolbox"),
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(TOOLBOX_MIN_SHRINK_WIDTH, TOOLBOX_MAX_GROW_WIDTH),
        },
        .backgroundColor = COLOR_BLUE });
}