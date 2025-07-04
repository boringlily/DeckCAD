#define RAYLIB_IMPLEMENTATION
#include "raylib.h"
#include "clay.h"
#include "ClayPrimitives.h"
#include "raymath.h"
#include "stdint.h"
#include "cstring"
#include "stdio.h"
#include "stdlib.h"
#include <string>
#include "LayoutEngine.h"
#include "RenderHandler.h"

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle) (Rectangle) { .x = rectangle.x, .y = rectangle.y, .width = rectangle.width, .height = rectangle.height }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color) (Color) { .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g), .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a) }

static inline Clay_Dimensions Raylib_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    // Measure string size for Font
    Clay_Dimensions textSize = { 0 };

    float maxTextWidth = 0.0f;
    float lineTextWidth = 0;
    int maxLineCharCount = 0;
    int lineCharCount = 0;

    float textHeight = config->fontSize;
    Font* fonts = (Font*)userData;
    Font fontToUse = fonts[config->fontId];
    // Font failed to load, likely the fonts are in the wrong place relative to the execution dir.
    // RayLib ships with a default font, so we can continue with that built in one. 
    if (!fontToUse.glyphs) {
        fontToUse = GetFontDefault();
    }

    float scaleFactor = config->fontSize/(float)fontToUse.baseSize;

    for (int i = 0; i < text.length; ++i, lineCharCount++)
    {
        if (text.chars[i] == '\n') {
            maxTextWidth = fmax(maxTextWidth, lineTextWidth);
            maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);
            lineTextWidth = 0;
            lineCharCount = 0;
            continue;
        }
        int index = text.chars[i] - 32;
        if (fontToUse.glyphs[index].advanceX != 0) lineTextWidth += fontToUse.glyphs[index].advanceX;
        else lineTextWidth += (fontToUse.recs[index].width + fontToUse.glyphs[index].offsetX);
    }

    maxTextWidth = fmax(maxTextWidth, lineTextWidth);
    maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);

    textSize.width = maxTextWidth * scaleFactor + (lineCharCount * config->letterSpacing);
    textSize.height = textHeight;

    return textSize;
}

static inline Clay_Dimensions GetScreenSize()
{
    return Clay_Dimensions { .width = static_cast<float>(GetRenderWidth()),
        .height = static_cast<float>(GetRenderHeight()) };
}

static inline bool HasScreenSizeChanged(Clay_Dimensions &A, Clay_Dimensions &B)
{
   return (A.height != B.height || A.width != B.width);
}

void HandleClayErrors(Clay_ErrorData errorData)
{
    printf("%s", errorData.errorText.chars);
}

using namespace UI;

ClayRender::ClayRender() 
{
    constexpr int WINDOW_MIN_WIDTH = 1024;
    constexpr int WINDOW_MIN_HEIGHT = 800;
    std::string WINDOW_TITLE = "DeckCAD"; 
    SetConfigFlags(
        FLAG_WINDOW_RESIZABLE
        // | FLAG_WINDOW_HIGHDPI // when FLAG_WINDOW_HIGHDPI is set the clay layout engine stops working with any Windows display scaling other than 100%.
        // this seems like something worth investigating.
        | FLAG_MSAA_4X_HINT
        | FLAG_VSYNC_HINT);

    InitWindow(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, const_cast<char*>(WINDOW_TITLE.c_str()));
    SetWindowMinSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

    uint64_t clayRequiredMemory = Clay_MinMemorySize();
    
    static Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
        clayRequiredMemory, malloc(clayRequiredMemory));

    Clay_Initialize(
        clayMemory, GetScreenSize(),
        (Clay_ErrorHandler) { HandleClayErrors });

    const uint32_t FONT_ID_BODY_16 = 0;

    // Font fonts[1];
    LoadedFonts[FONT_ID_BODY_16] = LoadFontEx("./assets/fonts/Roboto-Regular.ttf", 48, 0, 400);
    SetTextureFilter(LoadedFonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

    Clay_SetMeasureTextFunction(Raylib_MeasureText, LoadedFonts.data());
}

ClayRender::~ClayRender()
{
    CloseWindow();
}

void ClayRender::StartFrame() 
{

    Clay_Dimensions newScreenSize = GetScreenSize();
    screenSizeChanged = HasScreenSizeChanged(screenSize, newScreenSize);
    screenSize = newScreenSize;
    
    mousePosition = GetMousePosition();
    scrollDelta = GetMouseWheelMoveV();
    
    Clay_SetLayoutDimensions(screenSize);
    Clay_SetPointerState((Clay_Vector2) { mousePosition.x, mousePosition.y }, IsMouseButtonDown(0));
    Clay_UpdateScrollContainers(true, (Clay_Vector2) { scrollDelta.x, scrollDelta.y }, GetFrameTime());

    Clay_BeginLayout();
}

void ClayRender::EndFrame() 
{
    Clay_RenderCommandArray renderCommands = Clay_EndLayout();
    BeginDrawing();
        ClearBackground(BLACK);

        Render(renderCommands, LoadedFonts.data());
    EndDrawing();
}

void ClayRender::Render(Clay_RenderCommandArray renderCommands, Font* fonts)
{
    for (int j = 0; j < renderCommands.length; j++)
    {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, j);
        Clay_BoundingBox boundingBox = {roundf(renderCommand->boundingBox.x), roundf(renderCommand->boundingBox.y), roundf(renderCommand->boundingBox.width), roundf(renderCommand->boundingBox.height)};
        switch (renderCommand->commandType)
        {
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData *textData = &renderCommand->renderData.text;
                Font fontToUse = fonts[textData->fontId];
                int strlen = textData->stringContents.length + 1;
    
                if(strlen > tempRenderBuffer.capacity())
                {
                    tempRenderBuffer.reserve(strlen);
                }
    
                // Raylib uses standard C strings so isn't compatible with cheap slices, we need to clone the string to append null terminator
                memcpy(tempRenderBuffer.data(), textData->stringContents.chars, textData->stringContents.length);
                tempRenderBuffer[textData->stringContents.length] = '\0';
                DrawTextEx(fontToUse, tempRenderBuffer.data(), (Vector2){boundingBox.x, boundingBox.y}, (float)textData->fontSize, (float)textData->letterSpacing, CLAY_COLOR_TO_RAYLIB_COLOR(textData->textColor));
    
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                Texture2D imageTexture = *(Texture2D *)renderCommand->renderData.image.imageData;
                Clay_Color tintColor = renderCommand->renderData.image.backgroundColor;
                if (tintColor.r == 0 && tintColor.g == 0 && tintColor.b == 0 && tintColor.a == 0) {
                    tintColor = (Clay_Color) { 255, 255, 255, 255 };
                }
                DrawTexturePro(
                    imageTexture,
                    (Rectangle) { 0, 0, (float)imageTexture.width, (float)imageTexture.height },
                    (Rectangle){boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height},
                    (Vector2) {},
                    0,
                    CLAY_COLOR_TO_RAYLIB_COLOR(tintColor));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width), (int)roundf(boundingBox.height));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                EndScissorMode();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
                if (config->cornerRadius.topLeft > 0) {
                    float radius = (config->cornerRadius.topLeft * 2) / (float)((boundingBox.width > boundingBox.height) ? boundingBox.height : boundingBox.width);
                    DrawRectangleRounded((Rectangle) { boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height }, radius, 8, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
                } else {
                    DrawRectangle(boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderRenderData *config = &renderCommand->renderData.border;
                // Left border
                if (config->width.left > 0) {
                    DrawRectangle((int)roundf(boundingBox.x), (int)roundf(boundingBox.y + config->cornerRadius.topLeft), (int)config->width.left, (int)roundf(boundingBox.height - config->cornerRadius.topLeft - config->cornerRadius.bottomLeft), CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                // Right border
                if (config->width.right > 0) {
                    DrawRectangle((int)roundf(boundingBox.x + boundingBox.width - config->width.right), (int)roundf(boundingBox.y + config->cornerRadius.topRight), (int)config->width.right, (int)roundf(boundingBox.height - config->cornerRadius.topRight - config->cornerRadius.bottomRight), CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                // Top border
                if (config->width.top > 0) {
                    DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.topLeft), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width - config->cornerRadius.topLeft - config->cornerRadius.topRight), (int)config->width.top, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                // Bottom border
                if (config->width.bottom > 0) {
                    DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.bottomLeft), (int)roundf(boundingBox.y + boundingBox.height - config->width.bottom), (int)roundf(boundingBox.width - config->cornerRadius.bottomLeft - config->cornerRadius.bottomRight), (int)config->width.bottom, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                if (config->cornerRadius.topLeft > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + config->cornerRadius.topLeft), roundf(boundingBox.y + config->cornerRadius.topLeft) }, roundf(config->cornerRadius.topLeft - config->width.top), config->cornerRadius.topLeft, 180, 270, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                if (config->cornerRadius.topRight > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + boundingBox.width - config->cornerRadius.topRight), roundf(boundingBox.y + config->cornerRadius.topRight) }, roundf(config->cornerRadius.topRight - config->width.top), config->cornerRadius.topRight, 270, 360, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                if (config->cornerRadius.bottomLeft > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + config->cornerRadius.bottomLeft), roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomLeft) }, roundf(config->cornerRadius.bottomLeft - config->width.bottom), config->cornerRadius.bottomLeft, 90, 180, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                if (config->cornerRadius.bottomRight > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + boundingBox.width - config->cornerRadius.bottomRight), roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomRight) }, roundf(config->cornerRadius.bottomRight - config->width.bottom), config->cornerRadius.bottomRight, 0.1, 90, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM: 
            {
                CanvasRenderHandler(renderCommand);
                break;
            }
            default: {
                printf("Error: unhandled render command.");
                exit(1);
            }
        }
    }
}


/// TODO: Move this into the canvas logic space, I think it belongs there.

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera3D camera, int screenWidth, int screenHeight, float zDistance)
{
    Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f*position.x)/(float)screenWidth - 1.0f;
    float y = 1.0f - (2.0f*position.y)/(float)screenHeight;
    float z = 1.0f;

    // Store values in a vector
    Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)screenWidth/(double)screenHeight), 0.01f, zDistance);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        double aspect = (double)screenWidth/(double)screenHeight;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    Vector3 nearPoint = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    Vector3 farPoint = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Calculate normalized direction vector
    Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    ray.position = farPoint;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}