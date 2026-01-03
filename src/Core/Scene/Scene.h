#pragma once
#include <iostream>
#include "DumbTypes.h"
#include "Toolbox/Toolbox.h"
#include "Canvas/CanvasCamera.h"
#include "CAD/CommandEngine.h"
#include "CAD/ParametricExpression.h"
#include "raylib.h"
#include <string>

// forward declaration
class AppMemory;

class Scene {
public:
    Scene() = delete;

    std::string filename { "Untitled" };

    // Gui data
    CanvasCamera camera {};
    Toolbox toolbox;
    RenderTexture& canvas_texture;

private:
    Scene(std::string name, RenderTexture& canvas_texture)
        : filename { name }
        , canvas_texture { canvas_texture } {};

    friend class AppMemory;
};