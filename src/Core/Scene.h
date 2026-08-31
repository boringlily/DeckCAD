#pragma once
#include "Camera.h"
#include "Toolbox.h"
#include "Types.h"
#include <string>

namespace Core {

/// One open document: its camera, its tool state and its display options.
struct Scene {
    Scene() = default;
    explicit Scene(std::string scene_name)
        : name { std::move(scene_name) }
    {
    }

    std::string name { "Untitled" };
    Viewport::Camera camera {};
    Toolbox toolbox {};

    bool show_grid { true };
    bool show_origin_planes { true };
    bool show_axis_labels { true };
};

} // namespace Core
