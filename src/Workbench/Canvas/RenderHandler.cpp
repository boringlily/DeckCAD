#include "clay.h"
#include "assert.h"

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>

#include "Scene.h"
#include "CanvasHelpers.cpp"
#include "RenderHandler.h"

void RenderCanvas(Scene& scene)
{
    auto modelColor = (Color) { 140, 140, 140, 255 };
    auto wireframeColor = (Color) { 140, 140, 140, 100 };

    DrawModel(scene.example_model, { 0, 0, 0 }, 1.0f, modelColor);
    DrawModelWires(scene.example_model, { 0, 0, 0 }, 1.0f, wireframeColor);
    UI::DrawOriginPlane(UI::OriginPlane::XZ, { 0, 0, 0 }, { 10, 10 }, Color { 20, 20, 100, 100 });
    UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);
}

void CanvasRenderHandler(Clay_RenderCommand* renderCommand)
{
    static RenderTexture canvas_texture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    static Clay_BoundingBox canvasSize {};
    static Rectangle canvasRec {};
    static bool screenSizeChanged { false };

    Clay_BoundingBox boundingBox = { roundf(renderCommand->boundingBox.x), roundf(renderCommand->boundingBox.y), roundf(renderCommand->boundingBox.width), roundf(renderCommand->boundingBox.height) };
    Clay_CustomRenderData* config = &renderCommand->renderData.custom;
    Scene& scene = *reinterpret_cast<Scene*>(renderCommand->renderData.custom.customData);

    screenSizeChanged = (canvasSize.height != boundingBox.height || canvasSize.width != boundingBox.width);

    if (screenSizeChanged) {
        canvas_texture = LoadRenderTexture(boundingBox.width, boundingBox.height);
    }

    canvasRec = { .x = 0, .y = 0, .width = boundingBox.width, .height = -boundingBox.height };

    BeginTextureMode(canvas_texture);
    ClearBackground(WHITE);
    BeginMode3D(scene.camera.raylibCamera);

    RenderCanvas(scene);

    EndMode3D();
    EndTextureMode();
    DrawTextureRec(canvas_texture.texture, canvasRec, (Vector2) { boundingBox.x, -boundingBox.y }, WHITE);
    canvasSize = boundingBox;
}
// DrawText(TextFormat("Target(%0.2f, %0.2f, %0.2f)",  camera.target.x, camera.target.y, camera.target.z), 10, screenHeight - 60, 10, DARKGRAY);
// DrawText(TextFormat("Position(%0.2f, %0.2f, %0.2f)", camera.position.x, camera.position.y, camera.position.z), 10, screenHeight - 40, 10, DARKGRAY);
// DrawText(TextFormat("Up(%0.2f, %0.2f, %0.2f)",  camera.up.x, camera.up.y, camera.up.z), 10, screenHeight - 20, 10, DARKGRAY);

// if(GuiButton(Rectangle{screenWidth-110, 10 ,100,40 }, "#185#Home"))
// {
//     camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
// };
// if(GuiButton(Rectangle{screenWidth-110, 10+40 ,100,40 }, "Reset"))
// {
//     camera.up = {0,1,0};
//     camera.target = Vector3(0.0, 0.0, 0.0);
// };
// if(GuiButton(Rectangle{screenWidth-110, 10 + 40*2 ,100, 40 }, "XY"))
// {
//     camera.up = {0, 1, 0};
//     camera.position = Vector3(0.0, 0.0, 10.0F);
//     grid_plane = UI::OriginPlane::XY;
// };
// if(GuiButton(Rectangle{screenWidth-110, 10 + 40*3 ,100, 40 }, "XZ"))
// {
//     // Vector3 right = Vector3Normalize(Vector3CrossProduct(camera.up, {0,0,1}));
//     camera.up = {0, 0, -1};
//     camera.position = {0.0,10,0.0};
//     grid_plane = UI::OriginPlane::XZ;
// };
// if(GuiButton(Rectangle{screenWidth-110, 10 + 40*4 ,100, 40 }, "YZ"))
// {
//     camera.up = {0, 1, 0};
//     camera.position = Vector3(10.0F, 0.0, 0.0);
//     grid_plane = UI::OriginPlane::YZ;
// };
