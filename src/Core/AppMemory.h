#pragma once
#include "DumbTypes.h"
#include "Scene/Scene.h"

struct AppMemory {
    // Global Data
    u32 active_context { 0 }; // A zero value means no activeContext;
    Scene scene {};
};