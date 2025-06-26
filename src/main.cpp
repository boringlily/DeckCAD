#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "ClayPrimitives.h"
#include "renderers/raylib/clay_renderer_raylib.c"
#include "Workbench/Workbench.h"
#include "Workbench/Scene/Scene.h"
#include "Workbench/Scene/SceneManager.h"
#include "Workbench/Canvas/RenderHandler.h"

static inline Clay_Dimensions GetScreenSize()
{
    return Clay_Dimensions { .width = static_cast<float>(GetRenderWidth()),
        .height = static_cast<float>(GetRenderHeight()) };
}

void HandleClayErrors(Clay_ErrorData errorData)
{
    printf("%s", errorData.errorText.chars);
}

int main(void)
{
    Clay_Raylib_Initialize(
        WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, "DeckCAD",
        FLAG_WINDOW_RESIZABLE
            // | FLAG_WINDOW_HIGHDPI // when FLAG_WINDOW_HIGHDPI is set the clay layout engine stops working with any Windows display scaling other than 100%.
            // this seems like something worth investigating.
            | FLAG_MSAA_4X_HINT
            | FLAG_VSYNC_HINT);

    SetWindowMinSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

    uint64_t clayRequiredMemory = Clay_MinMemorySize();

    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
        clayRequiredMemory, malloc(clayRequiredMemory));

    Clay_Initialize(
        clayMemory, GetScreenSize(),
        (Clay_ErrorHandler) { HandleClayErrors });

    const uint32_t FONT_ID_BODY_16 = 0;

    Font fonts[1];
    fonts[FONT_ID_BODY_16] = LoadFontEx("./assets/fonts/Roboto-Regular.ttf", 48, 0, 400);
    SetTextureFilter(fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

    Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

    Clay_SetCustomRenderCommandFunction(CanvasRenderHandler);

    SceneManager sceneManager;

    while (!WindowShouldClose()) {
        // Run once per frame
        Clay_SetLayoutDimensions(GetScreenSize());

        Vector2 mousePosition = GetMousePosition();
        Vector2 scrollDelta = GetMouseWheelMoveV();
        Clay_SetPointerState((Clay_Vector2) { mousePosition.x, mousePosition.y },
            IsMouseButtonDown(0));

        Clay_UpdateScrollContainers(
            true, (Clay_Vector2) { scrollDelta.x, scrollDelta.y }, GetFrameTime());

        Clay_BeginLayout();

        CLAY(
            { .id = CLAY_ID("OuterContainer"),
                .layout = {
                    .sizing = LAYOUT_EXPAND,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM },
                .backgroundColor = { 43, 41, 51, 255 } })
        {
            // DrawWorkbench(sceneManager.GetActiveScene());


        };

        Clay_RenderCommandArray renderCommands = Clay_EndLayout();

        BeginDrawing();
        ClearBackground(BLACK);
        Clay_Raylib_Render_Experimental(renderCommands, fonts);
        EndDrawing();
    }

    Clay_Raylib_Close();
}
