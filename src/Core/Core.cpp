#include "Workbench.cpp"
#include "Graphics.h"
#include "Scene/Scene.h"
#include "Core.h"

#ifdef __cplusplus
extern "C" {
#endif

CORE_API
void CoreUpdate(AppMemory& appMemory)
{
    Graphics::BeginFrame();

    CLAY(
        {
            .id = CLAY_ID("OuterContainer"),
            .layout = {
                .sizing = LAYOUT_EXPAND,
                .layoutDirection = CLAY_TOP_TO_BOTTOM },
        })
    {
        DrawWorkbench(appMemory.scene);
    };

    Graphics::EndFrame();
}

#ifdef __cplusplus
}
#endif
