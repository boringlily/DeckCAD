#pragma once
#include "Scene.h"
#include "Types.h"
#include <vector>

namespace Core
{

/**
 * @brief All persistent application state.
 * @note Holds no GPU handles or pointers into the renderer, letting it survive
 * a hot-reload of the logic layer.
 */
struct AppState
{
    /// Which top-level tab is showing. 0 is the home page; N selects scenes[N-1].
    u32 active_tab { 0 };
    std::vector<Scene> scenes;

    bool show_metrics_window { false };
    bool show_style_editor { false };

    bool hasOpenScene() const
    {
        return active_tab > 0 && (active_tab - 1) < scenes.size();
    }

    /// Only valid when hasOpenScene() is true.
    Scene& getCurrentScene() { return scenes[active_tab - 1]; }
    const Scene& getCurrentScene() const { return scenes[active_tab - 1]; }

    /// Appends a scene and focuses it. Returns its 1-based tab index.
    u32 newScene();

    /// Closes scenes[index] and keeps active_tab pointing somewhere sensible.
    void closeScene(size_t index);
};

} // namespace Core
