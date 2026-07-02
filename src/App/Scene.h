#pragma once
#include <iostream>
#include <vector>
#include "DTL.h"

#include "Toolbox.h"
#include "CanvasCamera.h"
#include "CommandToolbox.h"
#include "Sketch.h"

// forward declaration
class AppState;

class Scene {
public:
    Scene() = delete;

    std::string filename { "Untitled" };

    // Gui data
    CanvasCamera camera {};
    Toolbox toolbox {};
    RenderTexture& canvas_texture;

    // CommandToolbox command_toolbox;
    CommandToolbox command_toolbox {};

    std::vector<Geometry::Sketch> geometry;

private:
    Scene(std::string name, RenderTexture& canvas_texture)
        : filename { name }
        , canvas_texture { canvas_texture } {};

    friend class AppState;
};