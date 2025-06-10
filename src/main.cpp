#define CLAY_IMPLEMENTATION
#include "ui_components.h"
#include "clay.h"
#include "workbench.cpp"
#include "renderers/raylib/clay_renderer_raylib.c"
#include "scene.h"

void HandleClayErrors(Clay_ErrorData errorData) {
  printf("%s", errorData.errorText.chars);
}

static inline void MyCustomRenderCommand(Clay_RenderCommand *renderCommand)
{
    Clay_BoundingBox boundingBox = {roundf(renderCommand->boundingBox.x), roundf(renderCommand->boundingBox.y), roundf(renderCommand->boundingBox.width), roundf(renderCommand->boundingBox.height)};
    RenderCanvas(boundingBox);
}

int main(void) {
  Clay_Raylib_Initialize(
      WindowMinWidth, WindowMinHeight, "DeckCAD",
        FLAG_WINDOW_RESIZABLE
        // | FLAG_WINDOW_HIGHDPI // when FLAG_WINDOW_HIGHDPI is set the clay layout engine stops working with any Windows display scaling other than 100%.
        // this seems like something worth investigating.  
        | FLAG_MSAA_4X_HINT
        | FLAG_VSYNC_HINT
        );

    SetWindowMinSize(WindowMinWidth, WindowMinHeight);

  uint64_t clayRequiredMemory = Clay_MinMemorySize();

  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      clayRequiredMemory, malloc(clayRequiredMemory));

  Clay_Initialize(
      clayMemory, get_screen_size(),
      (Clay_ErrorHandler){HandleClayErrors});

  const uint32_t FONT_ID_BODY_16 = 0;
  
  Font fonts[1];
  fonts[FONT_ID_BODY_16] =
      LoadFontEx("resources/Roboto-Regular.ttf", 48, 0, 400);
  SetTextureFilter(fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

  Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

  Clay_SetCustomRenderCommandFunction(MyCustomRenderCommand);

  Scene active_scene{};
  CanvasInit();

 while (!WindowShouldClose()) 
 {
    // Run once per frame
    Clay_SetLayoutDimensions(get_screen_size());

    Vector2 mousePosition = GetMousePosition();
    Vector2 scrollDelta = GetMouseWheelMoveV();
    Clay_SetPointerState((Clay_Vector2){mousePosition.x, mousePosition.y},
                         IsMouseButtonDown(0));

    Clay_UpdateScrollContainers(
        true, (Clay_Vector2){scrollDelta.x, scrollDelta.y}, GetFrameTime());

    Clay_BeginLayout();

    CLAY(
        {
        .id = CLAY_ID("OuterContainer"),
        .layout = {
            .sizing = layoutExpand,
            .layoutDirection = CLAY_TOP_TO_BOTTOM 
        },
        .backgroundColor = {43, 41, 51, 255}
    })
    {
        DrawWorkbench(active_scene);
    };

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    BeginDrawing();
        ClearBackground(BLACK);
        Clay_Raylib_Render_Experimental(renderCommands, fonts);
    EndDrawing();
  }
  
  Clay_Raylib_Close();
}
