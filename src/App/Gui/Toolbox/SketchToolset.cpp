#include "Scene.h"
#include "Toolset.h"

void DrawSketchToolset(Scene& scene)
{
    // Finish Sketch ends the active CreateSketch part command, returning to part context.
    CLAY({ .id = CLAY_ID("SketchFinishButton"),
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(4),
            .childGap = 4,
            .childAlignment = ALIGN_CENTER,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgBase,
        .cornerRadius = CLAY_CORNER_RADIUS(8),
        .border = (Clay_BorderElementConfig) { .color = GuiTheme.BgDark, .width = { 2, 2, 2, 2, 0 } } })
    {
        DrawIcon(IconId::Unknown, GuiTheme.TextBase);
        CLAY_TEXT(CLAY_STRING("Finish Sketch"), &TextStyle.body);
        if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
            scene.command_toolbox.FinishPartCommand();
        }
    };

    bool cmd_active = scene.command_toolbox.IsSketchCommandActive();

    if (!cmd_active) {

        CLAY({
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                .childGap = 4,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
        })
        {
            BeginToolGroup(CLAY_STRING("Draw"));
            if (ToolSelectButton("Line", IconId::Unknown) && !cmd_active) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Line);
            }
            if (ToolSelectButton("Arc", IconId::Unknown) && !cmd_active) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Arc);
            }
            if (ToolSelectButton("Circle", IconId::Unknown) && !cmd_active) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Circle);
            }
            EndToolGroup();

            BeginToolGroup(CLAY_STRING("Dimensions"));
            if (ToolSelectButton("Dimension", IconId::Unknown) && !cmd_active) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Dimension);
            }
            EndToolGroup();

            BeginToolGroup(CLAY_STRING("Constraints"));
            ToolSelectButton("Coincident", IconId::Unknown);
            EndToolGroup();
        };
    } else {
        auto command_optional = scene.command_toolbox.GetActiveSketchCommand();
        assert(command_optional.has_value() &&);

        switch (command.type)
    }
}
