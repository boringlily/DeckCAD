#pragma once
#include "rcamera.h"
#include "raymath.h"
class SceneCamera {
public:
    SceneCamera()
    {
        Reset();
    }

    void Reset()
    {
        raylibCamera.up = { .5, 0, 0 };
        raylibCamera.position = (Vector3) { 15.0f, 15.0f, 15.0f }; // camera position
        raylibCamera.target = (Vector3) { 0.0f, 0.0f, 0.0f }; // camera looking at point
        raylibCamera.up = (Vector3) { 0.0f, 1.0f, 0.0f }; // camera up vector (rotation towards target)
        raylibCamera.fovy = 90; // camera field-of-view Y
        raylibCamera.projection = CAMERA_PERSPECTIVE;
    }

    void ProcessPanTilt()
    {
        Vector2 mouse_delta { GetMouseDelta() };

        bool orbit = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

        static constexpr float CAMERA_MOUSE_ORBIT_SENSITIVITY { 0.2F };
        static constexpr float CAMERA_MOUSE_PAN_SENSITIVITY { 0.0012F };

        if (orbit) {
            // Update rotation angles
            float yaw = -mouse_delta.x * CAMERA_MOUSE_ORBIT_SENSITIVITY * GetFrameTime();
            float pitch = -mouse_delta.y * CAMERA_MOUSE_ORBIT_SENSITIVITY * GetFrameTime();

            // Horizontal rotation (yaw)
            raylibCamera.position = Vector3RotateByAxisAngle(
                raylibCamera.position,
                Vector3 { 0, 1, 0 },
                yaw);

            raylibCamera.target = Vector3RotateByAxisAngle(
                raylibCamera.target,
                Vector3 { 0, 1, 0 },
                yaw);

            raylibCamera.up = Vector3Normalize(Vector3RotateByAxisAngle(
                raylibCamera.up,
                Vector3 { 0, 1, 0 },
                yaw));

            // Vertical rotation (pitch) (vertical )
            Vector3 right = Vector3Normalize(Vector3CrossProduct(raylibCamera.up, Vector3Subtract(raylibCamera.position, raylibCamera.target)));

            raylibCamera.position = Vector3RotateByAxisAngle(
                raylibCamera.position,
                right,
                pitch);

            raylibCamera.target = Vector3RotateByAxisAngle(
                raylibCamera.target,
                right,
                pitch);

            raylibCamera.up = Vector3Normalize(Vector3RotateByAxisAngle(
                raylibCamera.up,
                right,
                pitch));

        } else if (pan) {
            static bool moveInWorldPlane { true };
            if (IsKeyPressed(KEY_SPACE))
                moveInWorldPlane = !moveInWorldPlane;

            float distance = Vector3Distance(raylibCamera.position, { 0, 0, 0 });
            float cameraMoveSpeed = CAMERA_MOUSE_PAN_SENSITIVITY * distance;

            DrawText(TextFormat("speed %f", cameraMoveSpeed), 10, 10, 10, BLACK);
            float move_up = cameraMoveSpeed * mouse_delta.y;
            float move_right = cameraMoveSpeed * mouse_delta.x;

            if (mouse_delta.y != 0)
                CameraMoveUp(&raylibCamera, move_up);
            if (mouse_delta.x != 0)
                CameraMoveRight(&raylibCamera, -move_right, moveInWorldPlane);
        }
        float zoom = raylibCamera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove() : 0;
        CameraMoveToTarget(&raylibCamera, zoom);
    };

    Vector3 GetScreenPosition()
    {
        return GetScreenToWorldRay(GetMousePosition(), raylibCamera).position;
    }

    Camera3D raylibCamera;
};

class Scene {
public:
    Scene()
    {
        example_model = LoadModel("../src/stand_dock_model.obj");
    };

    SceneCamera camera {};

    Model example_model;

private:
};
