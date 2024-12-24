#include "raylib.h"
#include <array>
#include <vector>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

int main(void)
{
    std::vector<Vector2> points;
    std::vector<std::vector<Vector2>> polygons;

    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Testing UI");

    Color ballColor = DARKBLUE;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(0); //Dissable Exit Key
    //---------------------------------------------------------------------------------------

    // Main game loop
    bool draw_line{false};
    uint32_t current_polygon{0};
    while (!WindowShouldClose())    // Detect window close button
    {
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            draw_line = true;
            points.emplace_back(GetMousePosition());
        }

        if(IsKeyPressed(KEY_ESCAPE))
        {
            if(draw_line)
            {
                polygons.emplace_back(points);
                points.clear();
                draw_line = false;
            }
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            if(draw_line)
            {
                uint32_t size = points.size(); 
                for(uint32_t i{0}; i < size; i++)
                {
                    DrawCircleV(points[i], 2, RED);
                    
                    if(i>0)
                    {
                        DrawLineV(points[i-1], points[i], GREEN);
                    }
                }  
            }

            for(auto &polygon : polygons) 
            {
                uint32_t size = polygon.size(); 
                for(uint32_t i{0}; i < size; i++)
                {
                    DrawCircleV(polygon[i], 4, BLACK);

                    if(i>0)
                    {
                        DrawLineV(polygon[i-1], polygon[i], GRAY);
                    }
                    else
                    {
                        DrawLineV(polygon[i], polygon[size-1], GRAY);
                    }
                }  
            }       

            if(draw_line)
            {
                DrawText("Draw Mode, press ESC to finish", 10, 10, 20, DARKGRAY);
            }
            else
            {
                DrawText("Press Mouse 1 to draw point", 10, 10, 20, DARKGRAY);
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;

} 