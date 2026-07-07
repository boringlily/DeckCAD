#include <array>
#include <string_view>

// Toolset registry: tab name + the context it is visible in. The Ui toolbox
// (App.cpp UiToolbox) dispatches the active toolset's content by index, so the
// old Clay per-toolset draw-callback member was dropped with the Clay teardown.
enum class TabContext { Always,
    PartOnly,
    SketchOnly };

struct Toolset {
    std::string_view name;
    TabContext visibility;
};

static std::array<Toolset, 3> toolset_list = { {
    { "Part", TabContext::PartOnly },
    { "Sketch", TabContext::SketchOnly },
    { "Inspect", TabContext::Always },
} };
