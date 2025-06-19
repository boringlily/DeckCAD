#pragma once
#include "SceneCamera.h"
#include <iostream>

class Scene {
public:
    Scene()
    {
        Load();
    };

    void Load()
    {
        exampleModel = LoadModel("./assets/exampleModel.obj");
    }

    SceneCamera camera {};

    Model exampleModel {};
};
