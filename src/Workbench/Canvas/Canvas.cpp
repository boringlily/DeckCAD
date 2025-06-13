#include "Workbench.h"
#include "Scene.h"

void DrawCanvas(Scene & scene)
{
    CLAY({
        .id = CLAY_ID("WorkbenchCanvas"),
        .layout =
            {
                .sizing = layoutExpandMinWidth(500),
            },
        .backgroundColor = COLOR_GREEN,
        .custom = { .customData = &scene},
    })
    {
        Vector3 mouse_world_pos = scene.camera.GetScreenPosition();
        scene.camera.ProcessPanTilt();
    };
};

