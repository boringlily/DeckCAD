#include "Components.h"
#include "AppState.h"
#include "Scene.h"
#include <array>

#include "SketchToolset.cpp"

enum class TabContext { Always,
    PartOnly,
    SketchOnly };

struct Toolset {
    std::string_view name;
    TabContext visibility;
    void (*draw)(Scene&);
};

static void DrawPartToolset(Scene& scene)
{
    if (ToolSelectButton("Create Sketch", IconId::Unknown)) {
        scene.command_toolbox.StartPartCommand(PartCommandType::CreateSketch);
    }
}

static void DrawInspectToolset(Scene& scene)
{
    CLAY_TEXT(CLAY_STRING("Inspector coming soon."), &TextStyle.body);
}

static std::array<Toolset, 3> toolset_list = { {
    { "Part", TabContext::PartOnly, &DrawPartToolset },
    { "Sketch", TabContext::SketchOnly, &DrawSketchToolset },
    { "Inspect", TabContext::Always, &DrawInspectToolset },
} };

void DrawToolbox(Scene& scene)
{
    static constexpr float TOOLBOX_MIN_WIDTH { 200 };
    static constexpr float TOOLBOX_MAX_WIDTH { 300 };

    bool is_sketch = scene.command_toolbox.IsSketchContext();

    auto tab_visible = [&](const Toolset& t) -> bool {
        switch (t.visibility) {
        case TabContext::Always:
            return true;
        case TabContext::PartOnly:
            return !is_sketch;
        case TabContext::SketchOnly:
            return is_sketch;
        }
        return true;
    };

    // If the active tab is hidden in the current context, clamp to the first visible tab.
    if (!tab_visible(toolset_list[scene.toolbox.active_toolset])) {
        for (u32 i = 0; i < toolset_list.size(); ++i) {
            if (tab_visible(toolset_list[i])) {
                scene.toolbox.active_toolset = i;
                break;
            }
        }
    }

    CLAY({ .id = CLAY_ID("Toolbox"),
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(TOOLBOX_MIN_WIDTH, TOOLBOX_MAX_WIDTH),
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = GuiTheme.BgBase })
    {
        // Tab bar
        CLAY({ .id = CLAY_ID("ToolsetTabs"),
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                .padding = CLAY_PADDING_ALL(4),
                .childGap = 8,
            },
            .backgroundColor = GuiTheme.BgBase })
        {
            for (u32 i = 0; i < toolset_list.size(); ++i) {
                const Toolset& toolset = toolset_list[i];
                if (!tab_visible(toolset)) {
                    continue;
                }

                bool tab_active = (i == scene.toolbox.active_toolset);
                static Clay_String tab_name {};
                tab_name = { true, static_cast<s32>(toolset.name.size()), toolset.name.data() };

                CLAY({ .layout = {
                           .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                           .padding = LAYOUT_PADDING_SIDES_AND_TOP(16, 4),
                           .childAlignment = ALIGN_CENTER },
                    .backgroundColor = tab_active || Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgBase,
                    .cornerRadius = CLAY_CORNER_RADIUS(8u),
                    .border = (Clay_BorderElementConfig) { .color = GuiTheme.AccentPrimary, .width = (tab_active ? (Clay_BorderWidth) { 2, 2, 2, 2, 0 } : (Clay_BorderWidth) { 0 }) } })
                {
                    CLAY_TEXT(tab_name, tab_active ? &TextStyle.buttonActive : &TextStyle.buttonMuted);

                    if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                        scene.toolbox.active_toolset = i;
                    }
                };
            }
        };

        // Active toolset content
        CLAY({ .id = CLAY_ID("Toolset"),
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                .padding = CLAY_PADDING_ALL(4),
                .childGap = 16,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = GuiTheme.BgBase })
        {
            toolset_list[scene.toolbox.active_toolset].draw(scene);
        };
    }
}
