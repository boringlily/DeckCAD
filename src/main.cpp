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
    .width = CLAY_SIZING_GROW(),
    .height = CLAY_SIZING_GROW()
};

Clay_Sizing layoutExpandMinWidth(float min, float max = 0)
{
    return {
        .width = CLAY_SIZING_GROW(min, max),
        .height = CLAY_SIZING_GROW()
    };
};

constexpr int WindowMinWidth = 1024;
constexpr int WindowMinHeight = 800;

int main(void) {
  Clay_Raylib_Initialize(
      WindowMinWidth, WindowMinHeight, "DeckCAD",
        FLAG_WINDOW_RESIZABLE
        // | FLAG_WINDOW_HIGHDPI // when FLAG_WINDOW_HIGHDPI is set the clay layout engine stops working with any Windows display scaling other than 100%.
        // this seems like something worth investigating.  
        | FLAG_MSAA_4X_HINT
        | FLAG_VSYNC_HINT
        ); // Extra parameters to this function are new since
                            // the video was published

    SetWindowMinSize(WindowMinWidth, WindowMinHeight);

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
    const Clay_Color COLOR_LIGHT = (Clay_Color) {224, 215, 210, 255};
    const Clay_Color COLOR_RED = (Clay_Color) {200, 100, 100, 255};
    const Clay_Color COLOR_GREEN = (Clay_Color) {100, 200, 100, 255};
    const Clay_Color COLOR_BLUE = (Clay_Color) {100, 100, 200, 255};

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


        /// Workbench components
        CLAY({
        .id = CLAY_ID("Workbench"),
        .layout = {
            .sizing = layoutExpand,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = { 255, 255, 255, 255}
        })
        {
            CLAY({
            .id = CLAY_ID("WorkbenchInner"),
            .layout = {
                .sizing = layoutExpand, 
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            },
            .backgroundColor = {0,0,0,255}
            })
            {
                CLAY({
                .id = CLAY_ID("WorkbenchExplorer"),
                .layout = {
                    .sizing = layoutExpandMinWidth(300, 500), 
                },
                .backgroundColor = COLOR_RED 
                });
                
                CLAY({
                .id = CLAY_ID("WorkbenchCanvas"),
                .layout = {
                    .sizing = layoutExpandMinWidth(500), 
                },
                .backgroundColor = COLOR_GREEN
                });
                
                CLAY({
                .id = CLAY_ID("WorkbenchToolbox"),
                .layout = {
                    .sizing = layoutExpandMinWidth(200, 400), 
                },
                .backgroundColor = COLOR_BLUE 
                });

            };
            
            CLAY({
            .id = CLAY_ID("WorkbenchFooter"),
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(),
                    .height = CLAY_SIZING_FIXED(100)
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = COLOR_LIGHT 
            });
        };
    };

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();

    BeginDrawing();
    ClearBackground(BLACK);
    Clay_Raylib_Render(renderCommands, fonts);
    EndDrawing();
  }
  // This function is new since the video was published
  Clay_Raylib_Close();
}