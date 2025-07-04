#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "ClayPrimitives.h"
#include "UI/Layout/LayoutEngine.h"
#include "Workbench/Workbench.h"
#include "Workbench/Scene/Scene.h"
#include "Workbench/Scene/SceneManager.h"
#include "Workbench/Canvas/RenderHandler.h"


int main(void)
{
    UI::ClayRender clayRender{}; 
    
    SceneManager sceneManager;

    while (!WindowShouldClose()) 
    { 
        // Run once per frame

        clayRender.StartFrame();

        CLAY(
            { .id = CLAY_ID("OuterContainer"),
                .layout = {
                    .sizing = LAYOUT_EXPAND,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM },
                })
        {

            DrawWorkbench(sceneManager.GetActiveScene());
        };
        
        clayRender.EndFrame();
    }
}
