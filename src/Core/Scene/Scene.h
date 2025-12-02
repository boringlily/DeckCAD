#pragma once
#include <iostream>
#include "Toolbox/Toolbox.h"
#include "Canvas/CanvasCamera.h"
#include "CAD/CommandEngine.h"
#include "CAD/ParametricExpression.h"
#include "raylib.h"
#include <string>

struct Scene {
    Scene(std::string name)
        : filename { name } {};
    Scene() {};

    std::string filename { "Untitled" };

    // Project data
    ParameterEngine parameter_engine {};
    GeometryEngine geometry_engine {};

    // Gui data
    CanvasCamera camera {};
    Toolbox toolbox;
};