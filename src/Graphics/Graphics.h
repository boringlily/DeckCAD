#pragma once
#include "Graphics.export.h"
#include "raylib.h"
#include "Style.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace Graphics {

    GRAPHICS_API void Initialize();
    GRAPHICS_API void Deactivate();

    GRAPHICS_API void BeginFrame();
    GRAPHICS_API void EndFrame();

    // Host hook: called by Main right after a successful App.dll hot-reload so
    // the Ui context can drop per-id state that may have churned (scroll offsets).
    GRAPHICS_API void OnAppReloaded();

};

#ifdef __cplusplus
}
#endif
