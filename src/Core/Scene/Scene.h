#pragma once
#include <iostream>
#include "Canvas/CanvasCamera.h"
#include "raylib.h"
#include <string>

class Scene {
public:
    Scene(std::string name)
        : scene_name { name } {};
    Scene()
    {
        Load();
    };

    void Load()
    {
        exampleModel = LoadModel("../assets/example_model.obj");
    }

    std::string scene_name { "Unknown" };

    Model exampleModel {};
    CanvasCamera camera {};
    RenderTexture canvasTexture;
};