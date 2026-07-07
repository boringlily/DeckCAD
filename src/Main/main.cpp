#include "Graphics.h"
#include "App.h"
#include "stdio.h"
#include "assert.h"
#include <functional>
#include <print>

#ifdef __HOT_RELOAD_ENABLED__
#include "LibLoaderClass.h"

LibraryFunction<void, AppState&> AppUpdateFunction("AppUpdate");
LibraryFunction<void> AppShutdownFunction("AppShutdown");
LibraryLoader AppLib("App", "./", { &AppUpdateFunction, &AppShutdownFunction });
#endif

void Update(AppState& app_memory)
{
#ifdef __HOT_RELOAD_ENABLED__
    if (AppLib.TryReload()) {
        std::println("----- Hot Reload {} -----", AppLib.reload_count);
        Graphics::OnAppReloaded();
    }

    AppUpdateFunction(app_memory);
#else
    AppUpdate(app_memory);
#endif
}

int main(void)
{
    Graphics::Initialize();

    AppState app_memory {};

    while (!WindowShouldClose()) {
        Update(app_memory);
    }

    // Free App-owned GPU resources while the GL context is still live, before
    // CloseWindow (Graphics::Deactivate). See App.h AppShutdown.
#ifdef __HOT_RELOAD_ENABLED__
    AppShutdownFunction();
#else
    AppShutdown();
#endif

    Graphics::Deactivate();
}