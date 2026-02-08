#include "Graphics.h"
#include "App.h"
#include "stdio.h"
#include "assert.h"
#include <functional>
#include <print>

#ifdef __HOT_RELOAD_ENABLED__
#include "LibLoaderClass.h"

LibraryFunction<void, AppState&> AppUpdateFunction("AppUpdate");
LibraryLoader AppLib("App", "./", { &AppUpdateFunction });
#endif

void Update(AppState& app_memory)
{
#ifdef __HOT_RELOAD_ENABLED__
    if (AppLib.TryReload()) {
        std::println("----- Hot Reload {} -----", AppLib.reload_count);
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

    Graphics::Deactivate();
}