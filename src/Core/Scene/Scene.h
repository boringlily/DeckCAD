#pragma once
#include <iostream>
#include "Toolbox/Toolbox.h"
#include "Canvas/CanvasCamera.h"
#include "CAD/CommandList.h"
#include "raylib.h"
#include <string>

struct Scene {
    Scene(std::string name)
        : filename { name } {};
    Scene() {};

    std::string filename { "Untitled" };

    CanvasCamera camera {};
    Toolbox toolbox;
};