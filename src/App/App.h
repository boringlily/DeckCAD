#pragma once
#include "AppState.h"
#include "App.export.h"

#ifdef __cplusplus
extern "C" {
#endif

APP_API
void AppUpdate(AppState& app);

// Release App-owned GPU resources (the Ui canvas RenderTexture) while the GL context
// is still live. main() must call this AFTER the frame loop and BEFORE CloseWindow —
// the App.dll static that owns the texture is otherwise torn down (at DLL detach)
// only after the window/context is already gone.
APP_API
void AppShutdown();

#ifdef __cplusplus
}
#endif
