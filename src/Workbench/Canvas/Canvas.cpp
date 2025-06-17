#include "Workbench.h"
#include "Scene.h"


void DrawCanvas(Scene& scene)
{
    auto WorkbenchCanvasId = CLAY_ID("WorkbenchCanvas");
    
    CLAY({
        .id = WorkbenchCanvasId,
        .layout =
            {
                .sizing = layoutExpandMinWidth(500),
            },
        .backgroundColor = COLOR_GREEN,
        .custom = { .customData = &scene},
    })
    {
        if(Clay_PointerOver(WorkbenchCanvasId))
        {
            Vector3 mouse_world_pos = scene.camera.GetScreenPosition();
            scene.camera.ProcessPanTilt();
        }
    };
};

