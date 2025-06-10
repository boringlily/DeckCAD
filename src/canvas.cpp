#include "assert.h"

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <format>

#include "scene.h"
#include "canvasorigin.cpp"


Vector2 get_canvas_center()
{
    float x = float(GetScreenWidth()) * 0.5;
    float y = float(GetScreenHeight()) * 0.5;

    return {-x, -y};
}

static RenderTexture canvas_texture;
static Clay_BoundingBox canvas_size{};
static Rectangle canvasRec{};
static bool screen_size_changed{false};
static Model example_model;

void CanvasInit()
{
    canvas_texture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    example_model = LoadModel("../src/stand_dock_model.obj");
}


static inline void RenderCanvas(Clay_BoundingBox & element)
{
    screen_size_changed = (canvas_size.height != element.height || canvas_size.width != element.width);
    
    if(screen_size_changed)
    {
        canvas_texture = LoadRenderTexture(element.width, element.height); 
    }
    
    canvasRec = {.x = 0,.y=0, .width=element.width,.height = -element.height};
    
    DrawTextureRec(canvas_texture.texture, canvasRec, (Vector2){element.x, -element.y} , WHITE);
    canvas_size = element;
}

void DrawCanvas(Scene & scene)
{
    CLAY({
        .id = CLAY_ID("WorkbenchCanvas"),
        .layout =
            {
                .sizing = layoutExpandMinWidth(500),
            },
        .backgroundColor = COLOR_GREEN,
        .custom = { .customData = &canvas_texture},
    }){};

    Vector3 mouse_world_pos = scene.camera.GetScreenPosition();

    scene.camera.ProcessPanTilt();
    BeginTextureMode(canvas_texture);
        ClearBackground(WHITE);
        BeginMode3D(scene.camera.raylibCamera);

            auto modelColor = (Color){140,140,140,255}; 
            auto wireframeColor = (Color){140,140,140,100}; 

            DrawModel(example_model, {0, 0, 0}, 1.0f, modelColor);
            DrawModelWires(example_model, {0, 0, 0}, 1.0f, wireframeColor);
            UI::DrawOriginPlane(UI::OriginPlane::XZ, {0,0,0}, {20, 20}, Color{100, 100, 10, 100});
            UI::DrawGrid(UI::OriginPlane::XZ, 10, 1.0f);
        
         EndMode3D();
    EndTextureMode();
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
