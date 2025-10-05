#pragma once
#include <iostream>
#include "Toolbox/Toolbox.h"
#include "Canvas/CanvasCamera.h"
#include "raylib.h"
#include <string>

struct Scene {
    Scene(std::string name)
        : filename { name } {};
    Scene()
    {
        Load();
    };

    void Load()
    {
        exampleModel = LoadModel("../assets/example_model.obj");
    }

    std::string filename { "Untitled" };

    Model exampleModel {};
    CanvasCamera camera {};
    Toolbox toolbox;
};