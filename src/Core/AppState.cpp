#include "AppState.h"
#include <format>

namespace Core {

u32 AppState::newScene()
{
    // Names are only for display, so a running counter is enough; duplicates
    // after closing a tab are harmless.
    scenes.emplace_back(std::format("Untitled {}", scenes.size() + 1));
    active_tab = static_cast<u32>(scenes.size());
    return active_tab;
}

void AppState::closeScene(size_t index)
{
    if (index >= scenes.size()) {
        return;
    }
    scenes.erase(scenes.begin() + static_cast<std::ptrdiff_t>(index));

    if (scenes.empty()) {
        active_tab = 0;
        return;
    }

    // Tabs are 1-based, so closing a tab at or before the active one shifts it.
    const u32 closed_tab = static_cast<u32>(index) + 1;
    if (active_tab > closed_tab) {
        active_tab -= 1;
    } else if (active_tab == closed_tab) {
        // Fall back to the scene that slid into this slot, or the last one.
        active_tab = std::min(active_tab, static_cast<u32>(scenes.size()));
    }
}

} // namespace Core
