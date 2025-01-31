#include "assert.h"

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

#include "keyconfig.h"
#include "geometry_math.h"

static Font system_font;
float canvas_scale{1.0F};
float grid_spacing_mm = 5;
constexpr distance GRID_SPACING_MIN{0.001};
constexpr distance GRID_SPACING_MAX{1000};


typedef struct Plane3D
{
Vector3 center;
float angle_x;
float angle_y;
Vector2 size{20, 20};
};


namespace UI
{
    enum class OriginPlane
    {
        XY,
        XZ,
        YZ
    };

    void DrawOriginPlane(OriginPlane plane, Vector3 centerPos, Vector2 size, Color color)
    {
        rlPushMatrix();
            rlTranslatef(centerPos.x, centerPos.y, centerPos.z);

            switch(plane)
            {
                case OriginPlane::XY:
                    rlRotatef(90, 1, 0, 0);
                break;
                case OriginPlane::XZ:
                    // Do nothing, already the home position
                break;
                case OriginPlane::YZ:
                    rlRotatef(90, 0, 0, -1);
                break;
                default:
                
                // If this condition is hit, crash.
                assert(false);
            }
            rlScalef(size.x, 1.0f, size.y);
            rlBegin(RL_QUADS);
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlNormal3f(0.0f, 1.0f, 0.0f);

                rlVertex3f(-0.5f, 0.0f, -0.5f);
                rlVertex3f(-0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, -0.5f);
            rlEnd();
        rlPopMatrix();
    };

    void DrawPlane(Plane3D plane)
    {
        rlPushMatrix();
            rlTranslatef(plane.center.x, plane.center.y, plane.center.z);
            rlRotatef(plane.angle_x, 1, 0, 0);
            rlRotatef(plane.angle_y, 0, 0, -1);

            rlScalef(plane.size.x, 1.0f, plane.size.y);
            rlBegin(RL_QUADS);
                rlColor4ub(255, 203, 0, 20);
                rlNormal3f(0.0f, 1.0f, 0.0f);

                rlVertex3f(-0.5f, 0.0f, -0.5f);
                rlVertex3f(-0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, -0.5f);
            rlEnd();
        rlPopMatrix();

    }
    
    void DrawGrid(OriginPlane plane,int slices, float spacing)
    {
        int halfSlices = slices/2;
        rlPushMatrix();
            switch(plane)
            {
                case OriginPlane::XY:
                    rlRotatef(90, 1, 0, 0);
                break;
                case OriginPlane::XZ:
                    // Do nothing, already the home position
                break;
                case OriginPlane::YZ:
                    rlRotatef(90, 0, 0, -1);
                break;
                default:
                
                // If this condition is hit, crash.
                assert(false);
            }
            rlBegin(RL_LINES);
                for (int i = -halfSlices; i <= halfSlices; i++)
                {
                    if (i == 0)
                    {
                        rlColor3f(0.5f, 0.5f, 0.5f);
                    }
                    else
                    {
                        rlColor3f(0.75f, 0.75f, 0.75f);
                    }

                    rlVertex3f((float)i*spacing, 0.0f, (float)-halfSlices*spacing);
                    rlVertex3f((float)i*spacing, 0.0f, (float)halfSlices*spacing);

                    rlVertex3f((float)-halfSlices*spacing, 0.0f, (float)i*spacing);
                    rlVertex3f((float)halfSlices*spacing, 0.0f, (float)i*spacing);
                }
            rlEnd();
        rlPopMatrix();
    }
}

Vector2 get_canvas_center()
{
    float x = float(GetScreenWidth()) * 0.5;
    float y = float(GetScreenHeight()) * 0.5;

    return {-x, -y};
}

void update_canvas_camera(Camera3D &camera) 
{
    static Vector2 canvas_offset;

    Vector2 mouse_delta{GetMouseDelta()};

    bool orbit = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    
    static constexpr float CAMERA_MOUSE_ORBIT_SENSITIVITY{0.01F};
    static constexpr float CAMERA_MOUSE_PAN_SENSITIVITY{0.00120F};

    if(orbit)
    {   
        CameraYaw(&camera, -mouse_delta.x*CAMERA_MOUSE_ORBIT_SENSITIVITY, true);
        CameraPitch(&camera, -mouse_delta.y*CAMERA_MOUSE_ORBIT_SENSITIVITY, true, true, false);
    }
    else if(pan)
    {
        static bool moveInWorldPlane{false}; 
        if(IsKeyPressed(KEY_SPACE)) moveInWorldPlane = !moveInWorldPlane;
        
        float distance = Vector3Distance(camera.position, {0,0,0});
        float cameraMoveSpeed = CAMERA_MOUSE_PAN_SENSITIVITY * distance;

        DrawText(TextFormat("speed %f", cameraMoveSpeed),10,10,10,BLACK ); 
        float move_up = cameraMoveSpeed * mouse_delta.y; 
        float move_right = cameraMoveSpeed * mouse_delta.x; 
    
        if(mouse_delta.y != 0) CameraMoveUp(&camera, move_up);
        if(mouse_delta.x != 0) CameraMoveRight(&camera, -move_right, moveInWorldPlane);
    }
    else
    {
        if(camera.projection == CAMERA_ORTHOGRAPHIC)
        {
            camera.fovy += -GetMouseWheelMove();
        }
        else
        {
            CameraMoveToTarget(&camera, -GetMouseWheelMove());
        }
    }
}

void update_canvas_camera_pro(Camera3D &camera) 
{
    Vector3 rotation{};
    Vector3 translation{};

    Vector2 mouse_delta{GetMouseDelta()};

    bool orbit = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    
    static constexpr float CAMERA_MOUSE_ORBIT_SENSITIVITY{0.01F};
    static constexpr float CAMERA_MOUSE_PAN_SENSITIVITY{0.0012F};

    if(orbit)
    {   
        static float yaw = 0.0f;
        static float pitch = 0.0f;
        float sensitivity = 0.2f;
        float distance;

        Vector3 offset = Vector3Subtract(camera.position, camera.target);
        distance = Vector3Length(offset);
        
        Vector2 mouseDelta = GetMouseDelta();

        // Update rotation angles
        yaw = -mouseDelta.x * sensitivity * GetFrameTime();
        pitch = -mouseDelta.y * sensitivity * GetFrameTime();

        // Horizontal rotation (yaw)
        camera.position = Vector3RotateByAxisAngle(
            camera.position,
            (Vector3){0, 1, 0}, 
            yaw);

        camera.target = Vector3RotateByAxisAngle(
            camera.target,
            (Vector3){0, 1, 0}, 
            yaw);

        // Vertical rotation (pitch)
        Vector3 right = Vector3Normalize(Vector3CrossProduct(camera.up, Vector3Subtract(camera.position, camera.target)));

        camera.position = Vector3RotateByAxisAngle(
            camera.position,
            right, 
            pitch
        );

        camera.target = Vector3RotateByAxisAngle(
            camera.target,
            right, 
            pitch
        );
    }
    else if(pan)
    {
        static bool moveInWorldPlane{true}; 
        if(IsKeyPressed(KEY_SPACE)) moveInWorldPlane = !moveInWorldPlane;
        
        float distance = Vector3Distance(camera.position, {0,0,0});
        float cameraMoveSpeed = CAMERA_MOUSE_PAN_SENSITIVITY * distance;

        DrawText(TextFormat("speed %f", cameraMoveSpeed),10,10,10,BLACK ); 
        float move_up = cameraMoveSpeed * mouse_delta.y; 
        float move_right = cameraMoveSpeed * mouse_delta.x; 
    
        if(mouse_delta.y != 0) CameraMoveUp(&camera, move_up);
        if(mouse_delta.x != 0) CameraMoveRight(&camera, -move_right, moveInWorldPlane);
    }
    float zoom = camera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove(): 0;

    UpdateCameraPro(&camera, translation, rotation, zoom);
};

int main(void)
{
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1200;
    const int screenHeight = 800;

    InitWindow(screenWidth, screenHeight, "Testing UI");

    SetTargetFPS(60);              // Set our game to run at 60 frames-per-second
    // SetExitKey(0);

    system_font = GetFontDefault();

    //---------------------------------------------------------------------------------------
    Vector2 mouse_pos = {};
    
    Camera3D camera_3d = { 0 };
    camera_3d.up = {.5,0,0};
    camera_3d.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // camera_3d position
    camera_3d.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // camera_3d looking at point
    camera_3d.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // camera_3d up vector (rotation towards target)
    camera_3d.fovy = 45.0f;                                // camera_3d field-of-view Y
    camera_3d.projection = CAMERA_PERSPECTIVE; 

    Model example_model = LoadModel("../src/stand_dock_model.obj");



    UI::OriginPlane grid_plane{UI::OriginPlane::XZ};
    
    bool use_custom{true};
    while (!WindowShouldClose())
    { 
        Vector3 mouse_world_pos = GetScreenToWorldRay(GetMousePosition(), camera_3d).position;
        mouse_pos = GetMousePosition();
        

        if(IsKeyPressed(KEY_ONE))
        {
         use_custom = !use_custom;   
        };
        if(use_custom)
        {
            update_canvas_camera_pro(camera_3d);
        }
        else
        {
            update_canvas_camera(camera_3d);

        }
       
        BeginDrawing();
        ClearBackground(WHITE);

        if(GuiButton(Rectangle{screenWidth-110, 10 ,100,40 }, "#185#Home"))
        {
            camera_3d.position = (Vector3){ 10.0f, 10.0f, 10.0f };
        };
        if(GuiButton(Rectangle{screenWidth-110, 10+40 ,100,40 }, "Reset"))
        {
            camera_3d.target = Vector3(0.0, 0.0, 0.0);
        }; 
        if(GuiButton(Rectangle{screenWidth-110, 10 + 40*2 ,100, 40 }, "XY"))
        {
            camera_3d.position = Vector3(0.0, 0.0, 10.0F);
            grid_plane = UI::OriginPlane::XY;
        };
        if(GuiButton(Rectangle{screenWidth-110, 10 + 40*3 ,100, 40 }, "XZ"))
        {
            camera_3d.position = Vector3(0.0F, 10.0F, 0.0);
            grid_plane = UI::OriginPlane::XZ;
        };
        if(GuiButton(Rectangle{screenWidth-110, 10 + 40*4 ,100, 40 }, "YZ"))
        {
            camera_3d.position = Vector3(10.0F, 0.0, 0.0);
            grid_plane = UI::OriginPlane::YZ;
        };
        
        BeginMode3D(camera_3d);

            DrawModel(example_model, {0, 0, 0}, 1.0f, GRAY);    
            DrawModelWires(example_model, {0, 0, 0}, 1.0f, BLUE);
            DrawSphere({0,0,0}, .5, LIME); 
            DrawSphere(camera_3d.target, 1, GREEN); 
            
            // UI::DrawPlane(UI::OriginPlane::XZ, {0,0,0}, {20, 20}, Color{255, 161, 0, 50});
            
            UI::DrawGrid(grid_plane, 10, 1.0f);
        EndMode3D();
        
        DrawText(TextFormat("%s Nav", use_custom?"Custom":"Default"), 10, screenHeight - 40, 10, DARKGRAY);
        DrawText(TextFormat("Target(%0.2f, %0.2f, %0.2f), Position(%0.2f, %0.2f, %0.2f)", camera_3d.target.x, camera_3d.target.y, camera_3d.target.z, camera_3d.position.x, camera_3d.position.y, camera_3d.position.z), 10, screenHeight - 20, 10, DARKGRAY);

        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;

} 