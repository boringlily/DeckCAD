#define RAYLIB_IMPLEMENTATION
#include "raylib.h"
#include "rlgl.h"
#include "SceneCamera.h"

SceneCamera::SceneCamera()
{
    Reset();
};

void SceneCamera::Reset()
{
    raylibCamera.up = { .5, 0, 0 };
    raylibCamera.position = (Vector3) { 15.0f, 15.0f, 15.0f }; // camera position
    raylibCamera.target = (Vector3) { 0.0f, 0.0f, 0.0f }; // camera looking at point
    raylibCamera.up = (Vector3) { 0.0f, 1.0f, 0.0f }; // camera up vector (rotation towards target)
    raylibCamera.fovy = 90; // camera field-of-view Y
    raylibCamera.projection = CAMERA_PERSPECTIVE;
}

void SceneCamera::ProcessPanTilt()
{
    Vector2 mouseDelta { GetMouseDelta() };

    bool orbit = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

    static constexpr float CAMERA_MOUSE_ORBIT_SENSITIVITY { 0.2F };
    static constexpr float CAMERA_MOUSE_PAN_SENSITIVITY { 0.0012F };

    if (orbit) {
        // Update rotation angles
        float yaw = -mouseDelta.x * CAMERA_MOUSE_ORBIT_SENSITIVITY * GetFrameTime();
        float pitch = -mouseDelta.y * CAMERA_MOUSE_ORBIT_SENSITIVITY * GetFrameTime();

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

        // Vertical rotation (pitch)
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

        float moveUp = cameraMoveSpeed * mouseDelta.y;
        float moveRight = cameraMoveSpeed * mouseDelta.x;

        if (mouseDelta.y != 0)
            CameraMoveUp(&raylibCamera, moveUp);
        if (mouseDelta.x != 0)
            CameraMoveRight(&raylibCamera, -moveRight, moveInWorldPlane);
    }
    float zoom = raylibCamera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove() : 0;
    CameraMoveToTarget(&raylibCamera, zoom);
};

Vector3 SceneCamera::GetMouseScreenPosition()
{
    return GetScreenToWorldRay(GetMousePosition(), raylibCamera).position;
}