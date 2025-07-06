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

#include "CanvasPlanes.cpp"

#include "Canvas.h"

void CanvasComponent::UpdateInternal()
{
    static Clay_Dimensions canvasSize {};
    bool hasChanged = CheckDimensionChanged(canvasSize);

    if (hasChanged) {
        canvasTexture = LoadRenderTexture(canvasSize.width, canvasSize.height);
    }

    if (Clay_PointerOver(clayId)) {
        camera.ProcessPanTilt();
    }

    BeginTextureMode(canvasTexture);
    ClearBackground(WHITE);
    BeginMode3D(camera.raylibCamera);

    auto modelColor = (Color) { 140, 140, 140, 255 };
    auto wireframeColor = (Color) { 140, 140, 140, 100 };

    DrawModel(exampleModel, { 0, 0, 0 }, 1.0f, modelColor);
    DrawModelWires(exampleModel, { 0, 0, 0 }, 1.0f, wireframeColor);
    UI::DrawOriginPlane(UI::OriginPlane::XZ, { 0, 0, 0 }, { 10, 10 }, Color { 20, 20, 100, 100 });
    UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);

    EndMode3D();
    EndTextureMode();
}