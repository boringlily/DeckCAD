#pragma once
#include "DumbTypes.h"
#include "Scene/Scene.h"
#include <vector>
#include "assert.h"

struct AppMemory {
    // Global Data
    u32 header_state { 0 }; // A zero means home-page, 1 and up is the open theme;
    RenderTexture canvas_texture;
    std::vector<Scene> scenes;

    Scene& GetCurrentScene()
    {
        // Crash if this value is zero
        assert(header_state > 0);
        return scenes[header_state - 1];
    }
};

static AppMemory* app_global { nullptr };