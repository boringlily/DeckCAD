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

    // Clay -> src/Ui migration dual path. While migrating, both UI trees exist;
    // F11 flips between them at runtime (handled in BeginFrame). App code checks
    // this to decide which tree to declare for the current frame.
    GRAPHICS_API bool IsUiPathActive();

    // Host hook: called by Main right after a successful App.dll hot-reload so
    // the Ui context can drop per-id state that may have churned (scroll offsets).
    GRAPHICS_API void OnAppReloaded();

};

#ifdef __cplusplus
}
#endif
