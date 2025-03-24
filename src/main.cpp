#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "renderers/raylib/clay_renderer_raylib.c"


void HandleClayErrors(Clay_ErrorData errorData) {
  printf("%s", errorData.errorText.chars);
}

Clay_Dimensions get_screen_size() {
  return Clay_Dimensions{.width = static_cast<float>(GetScreenWidth()),
                         .height = static_cast<float>(GetScreenHeight())};
}

Clay_Sizing layoutExpand = {
    .width = CLAY_SIZING_GROW(0),
    .height = CLAY_SIZING_GROW(0)
};

int main(void) {
  Clay_Raylib_Initialize(
      1024, 768, "DeckCAD 2D Sketcher",
      FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT |
          FLAG_VSYNC_HINT); // Extra parameters to this function are new since
                            // the video was published

  uint64_t clayRequiredMemory = Clay_MinMemorySize();

  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      clayRequiredMemory, malloc(clayRequiredMemory));

  Clay_Initialize(
      clayMemory, get_screen_size(),
      (Clay_ErrorHandler){HandleClayErrors}); // This final argument is new
                                              // since the video was published
  const uint32_t FONT_ID_BODY_16 = 0;
  
  Font fonts[1];
  fonts[FONT_ID_BODY_16] =
      LoadFontEx("resources/Roboto-Regular.ttf", 48, 0, 400);
  SetTextureFilter(fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

  Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

  while (!WindowShouldClose()) {
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
        .id = CLAY_ID("Main Container"),
        .layout = {
            .sizing = layoutExpand,
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 16,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = {43, 41, 51, 255}});

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    BeginDrawing();
    ClearBackground(BLACK);
    Clay_Raylib_Render(renderCommands, fonts);
    EndDrawing();
  }
  // This function is new since the video was published
  Clay_Raylib_Close();
}