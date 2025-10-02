#pragma once
#include <iostream>
#include "Canvas/CanvasCamera.h"
#include "raylib.h"
#include <string>

struct Toolbox {
    enum Context : u32 {
        Solid = 0x01,
        Sketch = 0x02,
    };

    enum ToolStatus : u32 {
        active,
        done
    };

    using ToolFunctionPointer = Toolbox::ToolStatus (*)();
    Toolbox() {};

    Context context { Solid };
    u32 active_toolset { 0 };
    ToolFunctionPointer active_tool { nullptr };
};

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