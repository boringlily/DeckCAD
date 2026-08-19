#pragma once
#include "App.h"
#include "AppState.h"
#include "DynamicLibrary.h"

#include <string>

/**
 * @brief Loads deckcad_core as a shared library and reloads it whenever a new
 * copy appears on disk.
 * @note Panel and app-logic changes show up without restarting DeckCAD. Only used
 * when DeckCAD is configured with DECKCAD_HOT_RELOAD=ON; see Core/CoreApi.h for the
 * two entry points this dispatches to.
 */
class HotReloadCore
{
public:
    /**
     * @brief Locates the Core library in @p library_directory_ref and performs the first load.
     * @return False on failure.
     */
    bool openLibrary(const std::string& library_directory_ref);

    /// Reloads Core if its file on disk changed, then re-runs CoreAppInit
    /// since the reload replaced Core's function pointers.
    void reloadIfChanged(Core::AppState& app_ref);

    void appInit(Core::AppState& app_ref);
    void buildFrame(Core::FrameContext& frame_ref);

private:
    using AppInitFunc = void (*)(Core::AppState*, ImGuiContext*);
    using BuildFrameFunc = void (*)(Core::FrameContext*, ImGuiContext*);

    void resolveFunctions();

    Platform::DynamicLibrary library_;
    AppInitFunc app_init_func_ptr_ { nullptr };
    BuildFrameFunc build_frame_func_ptr_ { nullptr };
};
