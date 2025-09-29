#include "Graphics.h"
#include "Core.h"
#include "stdio.h"
#include "assert.h"
#include <functional>

#ifdef __HOT_RELOAD_ENABLED__
#include "LibLoaderClass.h"

LibraryFunction<void, AppMemory&> CoreUpdateFunc("CoreUpdate");
LibraryLoader CoreLib("CORE", "./", { &CoreUpdateFunc });
#endif

void Update(AppMemory& app_memory)
{
#ifdef __HOT_RELOAD_ENABLED__
    if (CoreLib.TryReload()) {
        printf("----- Hot Reload %u -----\n", CoreLib.reload_count);
    }

    CoreUpdateFunc(app_memory);

#else
    CoreUpdate(app_memory);
#endif
}

int main(void)
{
    Graphics::Initialize();

    AppMemory app_memory {};

    while (!WindowShouldClose()) {
        Update(app_memory);
    }

    Graphics::Deactivate();
}