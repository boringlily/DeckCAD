#pragma once
#include "rcamera.h"
#include "raymath.h"

struct Vector3;

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

    Camera3D raylibCamera {};
};