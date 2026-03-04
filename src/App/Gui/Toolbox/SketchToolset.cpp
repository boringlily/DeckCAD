#include "Scene.h"
#include "Toolset.h"

/// @brief Used as a placeholder for tool buttons that aren't implemented.
void LineToolFunction(Scene& scene)
{
    CLAY({ .layout = {
               .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
               .padding = CLAY_PADDING_ALL(4),
               .childGap = 8,
               .layoutDirection = CLAY_TOP_TO_BOTTOM,
           },
        .backgroundColor = GuiTheme.BgBase })
    {
        // ideas
        // step 0) before this function
        // sketch = scene.command_manager.GetSketchContext();

        // sketch.parameter1;
        // sketch.parameter2;

        // if(sketch.isValid());
    }
}

void SketchToolSet(Scene& scene)
{
    bool is_sketch_context { scene.toolbox.context == Toolbox::Context::Sketch };

    CLAY({ .id = CLAY_ID("SketchToolsetControlButton"),
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
        DrawIcon(is_sketch_context ? IconId::Check : IconId::Plus, GuiTheme.TextBase);
        CLAY_TEXT(is_sketch_context ? CLAY_STRING("Finish Sketch") : CLAY_STRING("Create Sketch"), &TextStyle.body);
        if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
            scene.toolbox.context = is_sketch_context ? Toolbox::Context::Solid : Toolbox::Context::Sketch;
        }
    };

    if (!is_sketch_context)
        return;

    // Sketch Group List
    CLAY({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
            .childGap = 4,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    })
    {
        BeginToolGroup(CLAY_STRING("Draw"));
        ToolSelectButton(scene, "Line", Unknown, &ToolPlaceholderFunction);
        EndToolGroup();

        BeginToolGroup(CLAY_STRING("Dimensions"));
        ToolSelectButton(scene, "Length", Unknown, &ToolPlaceholderFunction);
        ToolSelectButton(scene, "Angle", Unknown, &ToolPlaceholderFunction);
        EndToolGroup();

        BeginToolGroup(CLAY_STRING("Constraints"));
        ToolSelectButton(scene, "Coincident", Unknown, &ToolPlaceholderFunction);
        EndToolGroup();
    };
}
