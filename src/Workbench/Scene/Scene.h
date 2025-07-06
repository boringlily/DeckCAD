#pragma once
#include <iostream>
#include "Canvas.h"

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

    void Update()
    {
        canvas.Draw();
    }

private:
    CanvasComponent canvas { exampleModel };

    Model exampleModel {};
};
