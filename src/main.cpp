#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
// #define RCAMERA_IMPLEMENTATION
#include "raygui.h"
#include "raymath.h"
#include "rlgl.h"
#include "rcamera.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <format>

#include "geometry_math.h"

static Font system_font;
float canvas_scale{1.0F};
float grid_spacing_mm = 5;
constexpr distance GRID_SPACING_MIN{0.001};
constexpr distance GRID_SPACING_MAX{1000};

struct Line
{
public:
    Vector3 A,B;

    void draw(bool simple_colors = true)
    {
        Color point1 = simple_colors? LIGHTGRAY : PINK;
        Color point2 = simple_colors? GRAY : ORANGE;
        Color line =  simple_colors? BLACK : GREEN;

        // DrawCircleV(A, 2 * canvas_scale, point1);
        // DrawCircleV(B, 2 * canvas_scale, point2);
        DrawLine3D(A, B, line);
    }

    void draw_with_stats()
    {
        
        CalcLength();
        CalcAngle();

        // DrawText(std::format("{:.2f}mm {:.2f}deg", length, angle, B.x, B.y).c_str(), B.x, B.y - 20, 10, BLACK);
        // DrawText(std::format("{:.2f}mm", length).c_str(), B.x, B.y - 20, 10, BLACK);

        // Line Dimension Marker
        // auto PointMarker = [](Vector2 v, degree theta)
        // {
        // //    Vector2 v1 = v + Vector2{1, 1};
        // //    Vector2 v2 = {20, 0};
        // //    v2 = Vector2Rotate(v2, 180 + theta);
        // // theta += 90;
            
        //     Vector2 v2 = {20 * cos(theta), 10 * sin(theta)};
            
        //     // DrawText(std::format("{:.2f}deg", theta, v2.x, v2.y).c_str(), v.x, v.y - 20, 10, MAROON);
        //     DrawLineEx(v, (v2 + v), 2 * canvas_scale, LIGHTGRAY);
        // };

        // PointMarker(A, angle);

        // Draw Angle ARC
        // float ID = length/2;
        // float half_angle = angle * .48; 
        

        // DrawRing(A,ID, ID + (2 * canvas_scale), 0, half_angle, 20, MAROON);
        // DrawRing(A,ID, ID + (2 * canvas_scale),-half_angle,-angle, 20, DARKBLUE);
        // DrawTxt(std::format("{:.2f}deg", angle).c_str(), A.x + length/10, A.y - 20, 10, MAROON);
        
        draw(false);
    }

    void CalcLength()
    {
        // length = Vector3Distance(A,B);
    }

    void CalcAngle()
    {
        // angle = Vector3Angle((B-A), {0,10,0}) * CustomMath::radians_to_deg;
        // if(angle < 0)
        // {
        //     angle += 360;
        // }
    }

    distance length{0};
    degree angle{0};
};

Vector2 get_canvas_center()
{
    float x = float(GetScreenWidth()) * 0.5;
    float y = float(GetScreenHeight()) * 0.5;

    return {-x, -y};
}

int main(void)
{
    std::vector<Line> line;
    
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Testing UI");

    SetTargetFPS(60);              // Set our game to run at 60 frames-per-second
    SetExitKey(0);

    system_font = GetFontDefault();

    //---------------------------------------------------------------------------------------
    Vector2 mouse_pos = {};
  
    Line l1 = {{50, 100}, {300, 100}};
    Line l2 = {{l1.B.x + 20, l1.B.y} , {500, 380}};
    Line l3 = {{l2.B.x + 20, l2.B.y}, {700, 200}};

    // Line follower = {get_canvas_center(), {0,0}};

    Camera2D camera_2d{};
    camera_2d.target = get_canvas_center();
    camera_2d.zoom = 1.0f;
    
    // Camera3D camera_3d {};
    // Camera3D camera_3d = { { 0.0f, 10.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 90.0, CAMERA_PERSPECTIVE};
    // camera_3d.target = {0,0,0};
    // camera_3d.position = {0, 0, 10};
    Camera3D camera_3d = { 0 };
    camera_3d.up = {.5,0,0};
    camera_3d.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // camera_3d position
    camera_3d.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // camera_3d looking at point
    camera_3d.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // camera_3d up vector (rotation towards target)
    camera_3d.fovy = 45.0f;                                // camera_3d field-of-view Y
    camera_3d.projection = CAMERA_PERSPECTIVE;  

    Rectangle toolbar_ui  = {0, 0, screenWidth, 50}; 
    RenderTexture canvas_texture = LoadRenderTexture(screenWidth, screenHeight-toolbar_ui.height);
    Rectangle canvas_position = {0, 0, (float)canvas_texture.texture.width, (float)-canvas_texture.texture.height};

    while (!WindowShouldClose())
    {   
        // UpdateCamera(&camera_3d, CAMERA_THIRD_PERSON);

        CameraMoveToTarget(&camera_3d, -GetMouseWheelMove());


        Vector3 mouse_world_pos = GetScreenToWorldRay(GetMousePosition(), camera_3d).position;
        mouse_pos = GetMousePosition();

        BeginTextureMode(canvas_texture);
            ClearBackground(RAYWHITE);
            
                // DrawText(std::format("Target({}), Position({})", camera_3d.target.y, camera_3d.target.x, camera_3d.target.z, camera_3d.position).c_str(), 10, screenHeight - 20, 10, DARKGRAY);
                DrawText(TextFormat("UP(%0.2f, %0.2f, %0.2f), Position(%0.2f, %0.2f, %0.2f)", camera_3d.up.y, camera_3d.up.x, camera_3d.up.z, camera_3d.position.y, camera_3d.position.x, camera_3d.position.z), 10, canvas_texture.texture.height - 20, 10, DARKGRAY);

                // BeginMode3D(camera_3d);
                //     // rlPushMatrix();
                //     //     rlTranslatef(0, 25*50, 0);
                //     //     rlRotatef(90, 1, 0, 0);
                //     // rlPopMatrix();
                    
                //     DrawGrid(100, 50);
                    
                // EndMode3D();

                BeginMode3D(camera_3d);

                    DrawCube({0,0,0}, 2.0f, 2.0f, 2.0f, RED);
                    DrawCubeWires({0,0,0}, 2.0f, 2.0f, 2.0f, MAROON);

                    // rlPushMatrix();
                    // rlTranslatef(0, 25*50, 0);
                    // rlRotatef(90, 0, 1, 0);
                    // rlPopMatrix();
                    // DrawGrid(10, 1.0f);

            EndMode3D();

        EndTextureMode();


       BeginDrawing();
        ClearBackground(SKYBLUE);
            DrawTextureRec(canvas_texture.texture, canvas_position, {0, toolbar_ui.height}, WHITE);
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;

} 