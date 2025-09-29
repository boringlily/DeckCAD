#include "Components.h"

void DrawToolbox(Scene& scene)
{
    static Clay_ElementId TOOLBOX_ID = CLAY_ID("Toolbox");

    static constexpr float TOOLBOX_MIN_SHRINK_WIDTH { 200 };
    static constexpr float TOOLBOX_MAX_GROW_WIDTH { 300 };

    CLAY({ .id = TOOLBOX_ID,
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(TOOLBOX_MIN_SHRINK_WIDTH, TOOLBOX_MAX_GROW_WIDTH),
        },
        .backgroundColor = COLOR_BLUE });
}