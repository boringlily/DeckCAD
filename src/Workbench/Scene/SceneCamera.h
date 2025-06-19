#pragma once
#include "rcamera.h"
#include "raymath.h"

struct Vector3;

class SceneCamera {
public:
    SceneCamera();

    void Reset();
    void ProcessPanTilt();
    Vector3 GetMouseScreenPosition();

    Camera3D raylibCamera {};
};