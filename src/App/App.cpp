#include "App.h"
#include "Graphics.h"
#include "Gui/Workbench.cpp"
#include "Gui/AppHeader.cpp"
#include "Gui/UiStyles.h"
#include <print>

#include <tracy/Tracy.hpp>

// ── src/Ui tree (migration in progress) ───────────────────────────────────────
// Views are ported here one by one; F11 flips between this and the Clay tree at
// runtime (see Graphics::BeginFrame). Until Phase 1 lands this is a placeholder
// proving the whole Ui stack: context, layout, theme colors, text, and icons.
namespace {

class UiShellPanel : public Ui::Panel {
    Ui::LayoutConfig Layout() const override
    {
        Ui::LayoutConfig c {};
        c.sizing = UiStyle::Expand();
        c.direction = Ui::Direction::TopToBottom;
        c.justify = Ui::Justify::Center;
        c.align = Ui::AlignCross::Center;
        c.gap = 8;
        return c;
    }
    Ui::UiColor Color() const override { return Ui::Colors().bgBase; }
};

void BuildUiTree(AppState& app)
{
    (void)app; // ported views will consume AppState here from Phase 1 on.

    static UiShellPanel shell;
    shell.Begin(Ui::NameId("OuterContainer"));
    UiStyle::DrawIcon(IconId::Home, Ui::Colors().accentSecondary, 48);
    UiStyle::Title("src/Ui path — migration in progress");
    UiStyle::Muted("F11 returns to the Clay tree.");
    shell.End();
}

} // namespace

#ifdef __cplusplus
extern "C" {
#endif

APP_API
void AppUpdate(AppState& app)
{
    ZoneScoped;

    Graphics::BeginFrame();

    if (Graphics::IsUiPathActive()) {
        BuildUiTree(app);
        Graphics::EndFrame();
        return;
    }

    CLAY(
        {
            .id = CLAY_ID("OuterContainer"),
            .layout = { .sizing = LAYOUT_EXPAND,
                .layoutDirection = CLAY_TOP_TO_BOTTOM },
        })
    {
        LayoutAppHeader(app);

        switch (app.GetActiveLayer()) {
        case AppLayer::Home_Layer:
            CLAY(
                { .id = CLAY_ID("HomePage"),
                    .layout = {
                        .sizing = LAYOUT_EXPAND,
                        .padding = CLAY_PADDING_ALL(8),
                        .childGap = 8,
                        .childAlignment = ALIGN_CENTER,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                    .backgroundColor = GuiTheme.BgBase })
            {
                CLAY_TEXT(CLAY_STRING("This is going to be the homepage."), &TextStyle.title);
            };

            break;
        case AppLayer::Settings_Layer:

            CLAY(
                { .id = CLAY_ID("SettingsPage"),
                    .layout = {
                        .sizing = LAYOUT_EXPAND,
                        .padding = CLAY_PADDING_ALL(8),
                        .childGap = 8,
                        .childAlignment = ALIGN_CENTER,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                    .backgroundColor = GuiTheme.BgBase })
            {
                CLAY_TEXT(CLAY_STRING("This is going to be the global settings page."), &TextStyle.title);
            };

            break;
        case AppLayer::Scene_Layer:

            Scene* scene_ptr = app.GetActiveScene();
            if (scene_ptr != nullptr) {
                DrawWorkbench(*scene_ptr);
            }

            break;
        }
    };

    Graphics::EndFrame();
}

#ifdef __cplusplus
}
#endif
