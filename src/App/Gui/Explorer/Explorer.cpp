#include "Inputs.h"
#include "Scene.h"
#include "Style.h"
#include <print>

void DrawExplorer(Scene& scene)
{
    static constexpr float EXPLORER_SHRINK_MIN_WIDTH { 100 };
    static constexpr float EXPLORER_GROW_MAX_WIDTH { 350 };

    CLAY({ .id = CLAY_ID("WorkbenchExplorer"),
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(EXPLORER_SHRINK_MIN_WIDTH, EXPLORER_GROW_MAX_WIDTH),
            .padding = CLAY_PADDING_ALL(4),
            .childGap = 8,
            .childAlignment = ALIGN_CENTER,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = GuiTheme.BgBase }) {};
}