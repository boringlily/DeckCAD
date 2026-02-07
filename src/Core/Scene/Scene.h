#pragma once
#include <iostream>
#include "DumbTypes.h"
#include "Toolbox/Toolbox.h"
#include "Canvas/CanvasCamera.h"
#include "CAD/GeneralCommandManager.h"

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

    // Cad Command System
    GeneralCommandManager command_manager;

private:
    Scene(std::string name, RenderTexture& canvas_texture)
        : filename { name }
        , canvas_texture { canvas_texture } {};

    friend class AppMemory;
};