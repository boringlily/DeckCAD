#pragma once
#include "Graphics.h"
#include "raylib.h"
#include "rlgl.h"
#include "rcamera.h"
#include "raymath.h"
#include "Geometry.h"

class CanvasCamera {
public:
    CanvasCamera();

    void Reset();
    void ProcessPanTilt();
    void ProcessPan2D();
    Vector3 GetMouseScreenPosition();
    Geometry::Point2 GetMouseOnSketchPlane(Geometry::SketchPlane plane);
    // Project a caller-supplied ray onto the plane. The Ui canvas builds this ray
    // from the canvas sub-viewport rect (GetScreenToWorldRayEx) so picking is correct
    // when the 3D view does not fill the whole window.
    Geometry::Point2 GetMouseOnSketchPlane(Geometry::SketchPlane plane, Ray ray);

    enum class CameraOrientation {
        Isometric_XYZ,
        Plane_XY,
        Plane_XZ,
        Plane_YZ,
    };

    void SetOrientation(CameraOrientation orientation);
    static CameraOrientation OrientationForSketchPlane(Geometry::SketchPlane plane);

    Camera3D raylib_camera {};

    // Pan mode toggle (Space). A member (not a function-local static) so App.dll
    // hot-reloads don't reset it mid-session.
    bool move_in_world_plane { true };
};

inline CanvasCamera::CanvasCamera()
{
    Reset();
};

inline void CanvasCamera::Reset()
{
    raylib_camera.up = { .5, 0, 0 };
    raylib_camera.fovy = 90; // camera field-of-view Y
    SetOrientation(CanvasCamera::CameraOrientation::Isometric_XYZ);
    raylib_camera.projection = CAMERA_PERSPECTIVE;
}

inline void CanvasCamera::ProcessPanTilt()
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
        if (IsKeyPressed(KEY_SPACE))
            move_in_world_plane = !move_in_world_plane;

        float distance = Vector3Distance(raylib_camera.position, { 0, 0, 0 });
        float cameraMoveSpeed = CAMERA_MOUSE_PAN_SENSITIVITY * distance;

        float moveUp = cameraMoveSpeed * mouseDelta.y;
        float moveRight = cameraMoveSpeed * mouseDelta.x;

        if (mouseDelta.y != 0)
            CameraMoveUp(&raylib_camera, moveUp);
        if (mouseDelta.x != 0)
            CameraMoveRight(&raylib_camera, -moveRight, move_in_world_plane);
    }
    float zoom = raylib_camera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove() : 0;
    CameraMoveToTarget(&raylib_camera, zoom);
};

inline void CanvasCamera::SetOrientation(CameraOrientation orientation)
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

inline Vector3 CanvasCamera::GetMouseScreenPosition()
{
    return GetScreenToWorldRay(GetMousePosition(), raylib_camera).position;
}

inline void CanvasCamera::ProcessPan2D()
{
    Vector2 mouseDelta { GetMouseDelta() };
    static constexpr float PAN_SENS { 0.0012F };

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        float dist = Vector3Distance(raylib_camera.position, { 0, 0, 0 });
        float speed = PAN_SENS * dist;
        if (mouseDelta.y != 0)
            CameraMoveUp(&raylib_camera, speed * mouseDelta.y);
        if (mouseDelta.x != 0)
            CameraMoveRight(&raylib_camera, -speed * mouseDelta.x, true);
    }

    float zoom = raylib_camera.projection == CAMERA_PERSPECTIVE ? -GetMouseWheelMove() : 0;
    CameraMoveToTarget(&raylib_camera, zoom);
}

inline Geometry::Point2 CanvasCamera::GetMouseOnSketchPlane(Geometry::SketchPlane plane)
{
    return GetMouseOnSketchPlane(plane, GetScreenToWorldRay(GetMousePosition(), raylib_camera));
}

inline Geometry::Point2 CanvasCamera::GetMouseOnSketchPlane(Geometry::SketchPlane plane, Ray ray)
{
    switch (plane) {
    case Geometry::SketchPlane::XY: {
        float t = (ray.direction.z != 0.0f) ? -ray.position.z / ray.direction.z : 0.0f;
        return { (f64)(ray.position.x + t * ray.direction.x),
            (f64)(ray.position.y + t * ray.direction.y) };
    }
    case Geometry::SketchPlane::XZ: {
        float t = (ray.direction.y != 0.0f) ? -ray.position.y / ray.direction.y : 0.0f;
        return { (f64)(ray.position.x + t * ray.direction.x),
            (f64)(ray.position.z + t * ray.direction.z) };
    }
    case Geometry::SketchPlane::YZ: {
        float t = (ray.direction.x != 0.0f) ? -ray.position.x / ray.direction.x : 0.0f;
        return { (f64)(ray.position.y + t * ray.direction.y),
            (f64)(ray.position.z + t * ray.direction.z) };
    }
    }
    return {};
}

inline CanvasCamera::CameraOrientation CanvasCamera::OrientationForSketchPlane(Geometry::SketchPlane plane)
{
    switch (plane) {
    case Geometry::SketchPlane::XY:
        return CameraOrientation::Plane_XY;
    case Geometry::SketchPlane::XZ:
        return CameraOrientation::Plane_XZ;
    case Geometry::SketchPlane::YZ:
        return CameraOrientation::Plane_YZ;
    }
    return CameraOrientation::Plane_XY;
}