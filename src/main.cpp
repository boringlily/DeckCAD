#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <array>
#include <vector>
#include "geometry_math.h"
#include <string>
#include <sstream>
#include <format>


struct Line
{
public:
    Vector2 A,B;

    void draw(bool simple_colors = true)
    {
        Color point1 = simple_colors? LIGHTGRAY : PINK;
        Color point2 = simple_colors? GRAY : ORANGE;
        Color line =  simple_colors? BLACK : GREEN;

        DrawCircleV(A, 2, point1);
        DrawCircleV(B, 4, point2);
        DrawLineEx(A, B, 2, line);
    }

    void draw_with_stats()
    {
        CalcLength();
        draw(false);
        auto lengthStr = std::format("{:.2f} mm", length);
        DrawText(lengthStr.c_str(), A.x, A.y + 10, 10, SKYBLUE);
    }

    void CalcLength()
    {
        length = CustomMath::point_distance(A.x,A.y,B.x,B.y);
    }
    
    float length = 0;
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

    while (!WindowShouldClose())    // Detect window close button
    {
        // mouse_pos = GetMousePosition();
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


        BeginDrawing();
        
            ClearBackground(RAYWHITE);
            l1.draw_with_stats();
            l2.draw_with_stats();
            l3.draw();
        

        //     if(draw_line)
        //     {
        //         uint32_t size = points.size(); 
        //         for(uint32_t i{0}; i < size; i+=2)
        //         {
        //             DrawCircleV(points[i], 2, RED);
        //             DrawCircleV(points[i], 2, RED);
                    
        //             if(i>0)
        //             {
        //                 DrawLineV(points[i-1], points[i], GREEN);
        //             }
        //         }  
        //     }

        //     for(auto &polygon : polygons) 
        //     {
        //         uint32_t size = polygon.size(); 
        //         for(uint32_t i{0}; i < size; i++)
        //         {
        //             DrawCircleV(polygon[i], 4, BLACK);

        //             if(i>0)
        //             {
        //                 DrawLineV(polygon[i-1], polygon[i], GRAY);
        //             }
        //             else
        //             {
        //                 DrawLineV(polygon[i], polygon[size-1], GRAY);
        //             }
        //         }  
        //     }       

        //     if(draw_line)
        //     {
        //         DrawText("Draw Mode, press ESC to finish", 10, GetScreenHeight() - 20, 20, DARKGRAY);
        //     }
        //     else
        //     {
        //         DrawText("Press Mouse 1 to draw point", 10, GetScreenHeight() - 20, 20, DARKGRAY);
        //     }



        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;

} 