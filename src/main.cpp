#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raymath.h"
#include "rlgl.h"

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

namespace UI
{
    void DrawLineDimensions()
    {

    }
};

struct Line
{
public:
    Vector2 A,B;

    void draw(bool simple_colors = true)
    {
        Color point1 = simple_colors? LIGHTGRAY : PINK;
        Color point2 = simple_colors? GRAY : ORANGE;
        Color line =  simple_colors? BLACK : GREEN;

        DrawCircleV(A, 2 * canvas_scale, point1);
        DrawCircleV(B, 3 * canvas_scale, point2);
        DrawLineEx(A, B, 2 * canvas_scale, line);
    }
    void draw_with_stats()
    {
        
        CalcLength();
        CalcAngle();

        // DrawText(std::format("{:.2f}mm {:.2f}deg", length, angle, B.x, B.y).c_str(), B.x, B.y - 20, 10, BLACK);
        // DrawText(std::format("{:.2f}mm", length).c_str(), B.x, B.y - 20, 10, BLACK);

        // Line Dimension Marker
        auto PointMarker = [](Vector2 v, degree theta)
        {
        //    Vector2 v1 = v + Vector2{1, 1};
        //    Vector2 v2 = {20, 0};
        //    v2 = Vector2Rotate(v2, 180 + theta);
        // theta += 90;
            
            Vector2 v2 = {20 * cos(theta), 10 * sin(theta)};
            
            // DrawText(std::format("{:.2f}deg", theta, v2.x, v2.y).c_str(), v.x, v.y - 20, 10, MAROON);
            DrawLineEx(v, (v2 + v), 2 * canvas_scale, LIGHTGRAY);
        };

        PointMarker(A, angle);

        // Draw Angle ARC
        // DrawCircleSectorLines(A, length/2, 0, -(angle * CustomMath::radians_to_deg), 10, DARKGRAY);
        float ID = length/2;
        DrawRing(A,ID, ID + (2 * canvas_scale),0,-angle, 20, MAROON);
        // DrawText(std::format("{:.2f}deg", angle).c_str(), A.x + length/10, A.y - 20, 10, MAROON);
        
        draw(false);
    }

    void CalcLength()
    {
        length = Vector2Distance(A,B);
    }

    void CalcAngle()
    {
        // angle = Vector2LineAngle(A, B);
        angle = Vector2LineAngle(A, B) * CustomMath::radians_to_deg;
        if(angle < 0)
        {
            angle += 360;
        }
    }

    distance length{0};
    degree angle{0};
};

int main(void)
{
    std::vector<Line> line;
    
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Testing UI");

    Color ballColor = DARKBLUE;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(0); //Disable Exit Key

    system_font = GetFontDefault();

    //---------------------------------------------------------------------------------------

    bool draw_line{false};
    uint32_t current_polygon{0};
    Vector2 mouse_pos = {};
    Rectangle canvas = {0, 40, float(GetScreenWidth()), float(GetScreenHeight())};

    enum class Tool : unsigned
    {
        none,
        twoPointLine,
    };

    Tool current_tool{Tool::none};

    Line l1 = {{50, 100}, {300, 100}};
    Line l2 = {{l1.B.x + 20, l1.B.y} , {500, 380}};
    Line l3 = {{l2.B.x + 20, l2.B.y}, {700, 200}};

    Line follower = {{300, 300}, {0,0}};

   Camera2D camera = { 0 };
    camera.zoom = 1.0f;

    int zoomMode = 0;   // 0-Mouse Wheel, 1-Mouse Move

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button
    {
        // Translate based on mouse right click
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f/camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
        }
        
        // Zoom based on mouse wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            // Get the world point that is under the mouse
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

            // Set the offset to where the mouse is
            camera.offset = GetMousePosition();

            // Set the target to match, so that the camera maps the world space point 
            // under the cursor to the screen space point under the cursor at any zoom
            camera.target = mouseWorldPos;

            // Zoom increment
            // float scaleFactor = 1.0f + (0.25f*fabsf(wheel));
            float scaleFactor = 1.0f + (0.25f*fabsf(wheel));
            if (wheel < 0) scaleFactor = 1.0f/scaleFactor;
            camera.zoom = Clamp(camera.zoom*scaleFactor, 0.125f, 64.0f);
            canvas_scale = 1/camera.zoom;
        }

        // canvas_scale = int(camera.zoom);
        Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), camera);
        mouse_pos = GetMousePosition();

        // bool mouse_on_canvas = CheckCollisionPointRec(mouse_pos, canvas);

        // if (GuiButton({0, 0, 100, 40}, "Line")) { current_tool = Tool::twoPointLine;}
        
        // if(current_tool != Tool::none)
        // {        
        //     if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_on_canvas)
        //     {
        //         draw_line = true;
        //     }

        //     if(IsKeyPressed(KEY_ESCAPE))
        //     {
        //         current_tool = Tool::none;
        //     }            

        // }

        
        follower.B = mouse_world_pos;

        //----------------------------------------------------------------------------------
        
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode2D(camera);
                // Draw the 3d grid, rotated 90 degrees and centered around 0,0 
                // just so we have something in the XY plane
                rlPushMatrix();
                    rlTranslatef(0, 25*50, 0);
                    rlRotatef(90, 1, 0, 0);
                    DrawGrid(100, 50);
                rlPopMatrix();
                
                follower.draw_with_stats();
            EndMode2D();
    
            // Draw mouse reference
            Vector2 mousePos = GetWorldToScreen2D(GetMousePosition(), camera);
            // DrawCircleV(mousePos, 2, LIME);
            // DrawCircleV(GetMousePosition(), 4, DARKGRAY);
            // DrawTextEx(GetFontDefault(), TextFormat("[%i, %i]", GetMouseX(), GetMouseY()), 
            //     Vector2Add(GetMousePosition(), (Vector2){ -44, -24 }), 20, 2, BLACK);

            DrawText(TextFormat("ZOOM: %0.2f, Scale: %0.2f", camera.zoom, canvas_scale), 20, 20, 20, DARKGRAY);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;

} 