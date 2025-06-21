#include "Workbench.h"
#include "Scene.h"

void DrawCanvas(Scene& scene)
{
    Clay_ElementId WorkbenchCanvasId = CLAY_ID("WorkbenchCanvas");
    static constexpr float CANVAS_WIDTH_SHRINK_MIN { 500.0f };

    CLAY({
        .id = WorkbenchCanvasId,
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(CANVAS_WIDTH_SHRINK_MIN),
        },
        .backgroundColor = COLOR_GREEN, // This background color should not be visible as the Canvas Texture should be rendered to fill all the available space.
        .custom = { .customData = &scene },
    })
    {
        if (Clay_PointerOver(WorkbenchCanvasId)) {
            scene.camera.ProcessPanTilt();
        }
    };
};
