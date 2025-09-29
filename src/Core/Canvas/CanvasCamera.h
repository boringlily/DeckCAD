#pragma once
#include "Graphics.h"
#include "raylib.h"
#include "rlgl.h"
#include "rcamera.h"
#include "raymath.h"

class CanvasCamera {
public:
    CanvasCamera();

    void Reset();
    void ProcessPanTilt();
    Vector3 GetMouseScreenPosition();

    enum class CameraOrientation {
        Isometric_XYZ,
        Plane_XY,
        Plane_XZ,
        Plane_YZ,
    };

    void SetOrientation(CameraOrientation orientation);

    Camera3D raylib_camera {};
};

CanvasCamera::CanvasCamera()
{
    Reset();
};

void CanvasCamera::Reset()
{
    raylib_camera.up = { .5, 0, 0 };
    raylib_camera.fovy = 90; // camera field-of-view Y
    SetOrientation(CanvasCamera::CameraOrientation::Isometric_XYZ);
    raylib_camera.projection = CAMERA_PERSPECTIVE;
}

void CanvasCamera::ProcessPanTilt()
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
        raylib_camera.position = Vector3RotateByAxisAngle(
            raylib_camera.position,
            Vector3 { 0, 1, 0 },
            yaw);

        raylib_camera.target = Vector3RotateByAxisAngle(
            raylib_camera.target,
            Vector3 { 0, 1, 0 },
            yaw);

        raylib_camera.up = Vector3Normalize(Vector3RotateByAxisAngle(
            raylib_camera.up,
            Vector3 { 0, 1, 0 },
            yaw));

        // Vertical rotation (pitch)
        Vector3 right = Vector3Normalize(Vector3CrossProduct(raylib_camera.up, Vector3Subtract(raylib_camera.position, raylib_camera.target)));

        raylib_camera.position = Vector3RotateByAxisAngle(
            raylib_camera.position,
            right,
            pitch);

        raylib_camera.target = Vector3RotateByAxisAngle(
            raylib_camera.target,
            right,
            pitch);

        raylib_camera.up = Vector3Normalize(Vector3RotateByAxisAngle(
            raylib_camera.up,
            right,
            pitch));

    } else if (pan) {
        static bool moveInWorldPlane { true };
        if (IsKeyPressed(KEY_SPACE))
            moveInWorldPlane = !moveInWorldPlane;

        float distance = Vector3Distance(raylib_camera.position, { 0, 0, 0 });
        float cameraMoveSpeed = CAMERA_MOUSE_PAN_SENSITIVITY * distance;

        float moveUp = cameraMoveSpeed * mouseDelta.y;
        float moveRight = cameraMoveSpeed * mouseDelta.x;

        if (mouseDelta.y != 0)
            CameraMoveUp(&raylib_camera, moveUp);
        if (mouseDelta.x != 0)
            CameraMoveRight(&raylib_camera, -moveRight, moveInWorldPlane);
    }
    float zoom = raylib_camera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove() : 0;
    CameraMoveToTarget(&raylib_camera, zoom);
};

void CanvasCamera::SetOrientation(CameraOrientation orientation)
{
    raylib_camera.target = (Vector3) { 0.0f, 0.0f, 0.0f }; // camera looking at point

    switch (orientation) {
    case CameraOrientation::Isometric_XYZ:
        raylib_camera.position = (Vector3) { 15.0f, 15.0f, 15.0f }; // camera position
        raylib_camera.up = (Vector3) { 0.0f, 1.0f, 0.0f }; // camera up vector (rotation towards target)
        break;
    case CameraOrientation::Plane_XY:
        raylib_camera.up = { 0, 1, 0 };
        raylib_camera.position = Vector3(0.0, 0.0, 15.0f);
        break;
    case CameraOrientation::Plane_XZ:
        raylib_camera.up = { 0, 0, -1 };
        raylib_camera.position = { 0.0, 15.0f, 0.0 };
        break;
    case CameraOrientation::Plane_YZ:
        raylib_camera.up = { 0, 1, 0 };
        raylib_camera.position = Vector3(15.0f, 0.0, 0.0);
        break;
    }
}

Vector3 CanvasCamera::GetMouseScreenPosition()
{
    return GetScreenToWorldRay(GetMousePosition(), raylib_camera).position;
}