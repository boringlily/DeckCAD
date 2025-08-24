#pragma once
#include <iostream>
#include "Canvas/CanvasCamera.h"
#include "raylib.h"

class Scene {
public:
    Scene()
    {
        Load();
    };

    void Load()
    {
        exampleModel = LoadModel("../assets/example_model.obj");
    }

    Model exampleModel {};
    CanvasCamera camera {};
    RenderTexture canvasTexture;
};