#include "Workbench.h"

void DrawExplorer(Scene& scene)
{
    static constexpr float EXPLORER_SHRINK_MIN_WIDTH { 100 };
    static constexpr float EXPLORER_GROW_MAX_WIDTH { 350 };

    CLAY({ .id = CLAY_ID("WorkbenchExplorer"),
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(EXPLORER_SHRINK_MIN_WIDTH, EXPLORER_GROW_MAX_WIDTH),
        },
        .backgroundColor = COLOR_RED });
}