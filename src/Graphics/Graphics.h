#pragma once
#include "Graphics.export.h"
#include "raylib.h"
#include "clay.h"
#include "Style.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace Graphics {

    GRAPHICS_API void Initialize();
    GRAPHICS_API void Deactivate();

    GRAPHICS_API void BeginFrame();
    GRAPHICS_API void EndFrame();

};

#ifdef __cplusplus
}
#endif
