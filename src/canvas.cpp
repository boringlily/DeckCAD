#include "assert.h"

#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <format>

#include "keyconfig.h"
#include "geometry_math.h"

// #include "experimental.h"

static Font system_font;
float canvas_scale{1.0F};
float grid_spacing_mm = 5;
constexpr distance GRID_SPACING_MIN{0.001};
constexpr distance GRID_SPACING_MAX{1000};

struct Plane3D
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

class CanvasCamera 
{
    public:

    CanvasCamera()
    {
        Reset();
    }; 

    void Reset()
    {
        raylibCamera.up = {.5,0,0};
        raylibCamera.position = (Vector3){ 15.0f, 15.0f, 15.0f }; // camera position
        raylibCamera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // camera looking at point
        raylibCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // camera up vector (rotation towards target)
        raylibCamera.fovy = 90;                                // camera field-of-view Y
        raylibCamera.projection = CAMERA_PERSPECTIVE; 
    }

    void ProcessPanTilt() 
    {
        Vector2 mouse_delta{GetMouseDelta()};

        bool orbit = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        
        static constexpr float CAMERA_MOUSE_ORBIT_SENSITIVITY{0.2F};
        static constexpr float CAMERA_MOUSE_PAN_SENSITIVITY{0.0012F};

        if(orbit)
        {   
            // Update rotation angles
            float yaw = -mouse_delta.x * CAMERA_MOUSE_ORBIT_SENSITIVITY * GetFrameTime();
            float pitch = -mouse_delta.y * CAMERA_MOUSE_ORBIT_SENSITIVITY * GetFrameTime();

            // Horizontal rotation (yaw)
            raylibCamera.position = Vector3RotateByAxisAngle(
                raylibCamera.position,
                Vector3{0, 1, 0}, 
                yaw);

            raylibCamera.target = Vector3RotateByAxisAngle(
                raylibCamera.target,
                Vector3{0, 1, 0}, 
                yaw);
        
            raylibCamera.up = Vector3Normalize(Vector3RotateByAxisAngle(
                raylibCamera.up,
                Vector3{0, 1, 0}, 
                yaw
                ));

            // Vertical rotation (pitch) (vertical )
            Vector3 right = Vector3Normalize(Vector3CrossProduct(raylibCamera.up, Vector3Subtract(raylibCamera.position, raylibCamera.target)));

            raylibCamera.position = Vector3RotateByAxisAngle(
                raylibCamera.position,
                right, 
                pitch
            );

            raylibCamera.target = Vector3RotateByAxisAngle(
                raylibCamera.target,
                right, 
                pitch
            );

            raylibCamera.up = Vector3Normalize(Vector3RotateByAxisAngle(
                raylibCamera.up,
                right, 
                pitch
            ));

        }
        else if(pan)
        {
            static bool moveInWorldPlane{true}; 
            if(IsKeyPressed(KEY_SPACE)) moveInWorldPlane = !moveInWorldPlane;
            
            float distance = Vector3Distance(raylibCamera.position, {0,0,0});
            float cameraMoveSpeed = CAMERA_MOUSE_PAN_SENSITIVITY * distance;

            DrawText(TextFormat("speed %f", cameraMoveSpeed),10,10,10,BLACK ); 
            float move_up = cameraMoveSpeed * mouse_delta.y; 
            float move_right = cameraMoveSpeed * mouse_delta.x; 
        
            if(mouse_delta.y != 0) CameraMoveUp(&raylibCamera, move_up);
            if(mouse_delta.x != 0) CameraMoveRight(&raylibCamera, -move_right, moveInWorldPlane);
        }
        float zoom = raylibCamera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove(): 0;
        CameraMoveToTarget(&raylibCamera, zoom);
    };

    auto GetScreenPosition()
    {
        return GetScreenToWorldRay(GetMousePosition(), raylibCamera).position;
    } 

    Camera3D GetCamera() const
    {
        return raylibCamera;
    }

    Camera3D raylibCamera = { 0 };
};

class Canvas
{
    public:
    
    Canvas()
    {
        example_model = LoadModel("../src/stand_dock_model.obj");
        grid_plane = UI::OriginPlane::XZ;
    };

    CanvasCamera canvasCamera{};
    Model example_model;
    //  = LoadModel("../src/stand_dock_model.obj");
    UI::OriginPlane grid_plane;
    RenderTexture renderTexture{};


    void Render(Clay_BoundingBox element)
    {

        if(HasScreenSizeChanged())
        {
            renderTexture = LoadRenderTexture(element.width, element.height); 
        }
        
        Vector3 mouse_world_pos = canvasCamera.GetScreenPosition();
    
        Rectangle canvasRec = {.x = 0,.y=0, .width=element.width,.height = -element.height};

        canvasCamera.ProcessPanTilt();
        BeginTextureMode(renderTexture);
            ClearBackground(WHITE);
            BeginMode3D(canvasCamera.raylibCamera);

            auto modelColor = (Color){140,140,140,255}; 
            auto wireframeColor = (Color){140,140,140,100}; 

                DrawModel(example_model, {0, 0, 0}, 1.0f, modelColor);
                DrawModelWires(example_model, {0, 0, 0}, 1.0f, wireframeColor);

                // DrawCube({0,0,0}, 10,10,10, BLUE);
                // DrawSphere({0,0,0}, .5, LIME); 
                // DrawSphere(canvasCamera.raylibCamera.target, 1, GREEN); 
                
                UI::DrawOriginPlane(UI::OriginPlane::XZ, {0,0,0}, {20, 20}, Color{100, 100, 10, 100});
                UI::DrawGrid(this->grid_plane, 10, 1.0f);
            EndMode3D();
        EndTextureMode();
        
        DrawTextureRec(renderTexture.texture, canvasRec, (Vector2){element.x, -element.y} , WHITE);
    }
    
};




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
