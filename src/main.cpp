#include "raylib.h"
#include <array>
#include "sketch.h"

struct line2
{
    int a{-1};
    int b{-1};

    bool isValid()
    {
        return !(a < 0) and !(b < 0);
    }
    line2(int _a, int _b) : a{_a}, b{_b} {};
};

std::array<Vector3, 8> points =
    {
        Vector3{-2, -2, 2},
        {-2, -2, -2},
        {2, -2, 2},
        {2, -2, -2},
        {2, 2, 2},
        {2, 2, -2},
        {-2, 2, 2},
        {-2, 2, -2}};

std::array<struct line2, 12> lines =
    {
        line2{1, 2},
        {2, 4},
        {4, 3},
        {1, 3},
        {3, 5},
        {5, 6},
        {6, 4},
        {6, 8},
        {8, 2},
        {8, 7},
        {7, 1},
        {5, 7}};

std::array<CLITERAL(Color), 12> colors =
    {
        // LIGHTGRAY
        // GRAY
        // DARKGRAY
        YELLOW,
        BROWN,
        // GOLD,
        ORANGE,
        PINK,
        RED,
        MAROON,
        GREEN,
        LIME,
        // DARKGREEN
        SKYBLUE,
        BLUE,
        // DARKBLUE
        PURPLE,
        VIOLET
        // DARKPURPLE
        // BEIGE
        
        // DARKBROWN

};

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "DeckCAD");

    // Define the camera to look into our 3d world
    Camera3D camera = {0};
    camera.position = (Vector3){10.0f, 10.0f, 10.0f}; // Camera position
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};      // Camera looking at point
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                              // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;           // Camera projection type

    Vector3 cubePosition = {0.0f, 0.0f, 0.0f};

    DisableCursor(); // Limit cursor to relative movement inside the window

    SetTargetFPS(120); // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_FREE);

        if (IsKeyPressed('Z'))
            camera.target = (Vector3){0.0f, 0.0f, 0.0f};
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        DrawFPS( 10, 10);
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
        
        // Draw Points
        for(auto &p : points)
        {
            DrawSphere(p, 0.02, RED);
        }   

        // Draw Lines
        for (unsigned i{0}; i < lines.size(); i++)
        {
            line2 &l = lines[i];
            if (l.isValid())
            {
                DrawLine3D(points[l.a-1], points[l.b-1], BLACK);
            }
        }

        // DrawGrid(10, 1.0f);
        EndMode3D();

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}