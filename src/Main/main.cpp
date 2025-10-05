#include "Graphics.h"
#include "Core.h"
#include "stdio.h"
#include "assert.h"
#include <functional>
#include <print>

#ifdef __HOT_RELOAD_ENABLED__
#include "LibLoaderClass.h"

LibraryFunction<void, AppMemory&> CoreInitFunc("CoreInit");
LibraryFunction<void> CoreUpdateFunc("CoreUpdate");
LibraryLoader CoreLib("CORE", "./", { &CoreInitFunc, &CoreUpdateFunc });
#endif

void Update(AppMemory& app_memory)
{
#ifdef __HOT_RELOAD_ENABLED__
    if (CoreLib.TryReload()) {
        std::println("----- Hot Reload {} -----", CoreLib.reload_count);

        CoreInitFunc(app_memory);
    }

    CoreUpdateFunc();
#else
    CoreUpdate();
#endif
}

int main(void)
{
    Graphics::Initialize();

    AppMemory app_memory {};

#ifndef __HOT_RELOAD_ENABLED__
    CoreInit(app_memory);
#endif

    while (!WindowShouldClose()) {
        Update(app_memory);
    }

    Graphics::Deactivate();
}