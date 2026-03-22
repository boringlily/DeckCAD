#pragma once
#include <iostream>
#include "DTL.h"

#include "Toolbox.h"
#include "CanvasCamera.h"
#include "CommandToolbox.h"

// forward declaration
class AppState;

class Scene {
public:
    Scene() = delete;

    std::string filename { "Untitled" };

    // Gui data
    CanvasCamera camera {};
    Toolbox toolbox;
    RenderTexture& canvas_texture;

    // CommandToolbox command_toolbox;
    PartCommandToolbox part_command_toolbox;

private:
    Scene(std::string name, RenderTexture& canvas_texture)
        : filename { name }
        , canvas_texture { canvas_texture } {};

    friend class AppState;
};