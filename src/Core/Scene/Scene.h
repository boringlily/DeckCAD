#pragma once
#include <iostream>
#include "Canvas/CanvasCamera.h"
#include "raylib.h"
#include <string>

class Scene {
public:
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
};