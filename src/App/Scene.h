#pragma once
#include <iostream>
#include "DumbTypes.h"

#include "Toolbox.h"
#include "CanvasCamera.h"
#include "GeneralCommandManager.h"

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

    GeneralCommandManager command_manager;

private:
    Scene(std::string name, RenderTexture& canvas_texture)
        : filename { name }
        , canvas_texture { canvas_texture } {};

    friend class AppState;
};