
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>

#include "Components.h"

#include "CanvasPlanes.cpp"

static Clay_ElementId canvasId = CLAY_SID(Clay_String("CanvasPanel"));

static void CanvasRenderToTexture(Scene& scene)
{
    Clay_Dimensions canvas_size = GetDimensions(canvasId);

    if (static_cast<int>(canvas_size.width) != app_global->canvas_texture.texture.width || static_cast<int>(canvas_size.height) != app_global->canvas_texture.texture.height) {
        if (app_global->canvas_texture.texture.id != 0) {
            UnloadRenderTexture(app_global->canvas_texture);
        }
        app_global->canvas_texture = LoadRenderTexture(canvas_size.width, canvas_size.height);
    }

    if (Clay_PointerOver(canvasId)) {
        scene.camera.ProcessPanTilt();
    }

    BeginTextureMode(app_global->canvas_texture);
    ClearBackground(WHITE);
    BeginMode3D(scene.camera.raylib_camera);

    auto modelColor = (Color) { 140, 140, 140, 255 };
    auto wireframeColor = (Color) { 140, 140, 140, 100 };

    DrawModel(scene.exampleModel, { 0, 0, 0 }, 1.0f, modelColor);
    DrawModelWires(scene.exampleModel, { 0, 0, 0 }, 1.0f, wireframeColor);
    UI::DrawOriginPlane(UI::OriginPlane::XZ, { 0, 0, 0 }, { 10, 10 }, Color { 20, 20, 100, 100 });
    UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);

    EndMode3D();
    EndTextureMode();
    SetTextureFilter(app_global->canvas_texture.texture, TEXTURE_FILTER_ANISOTROPIC_16X);
}

void DrawCanvas(Scene& scene)
{
    static constexpr float CANVAS_WIDTH_SHRINK_MIN { 500.0f };

    CLAY({ .id = canvasId,
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(CANVAS_WIDTH_SHRINK_MIN),
        },
        .custom = ClayCustom_TextureRenderConfig(app_global->canvas_texture) })
    {
        CanvasRenderToTexture(scene);
    };
}