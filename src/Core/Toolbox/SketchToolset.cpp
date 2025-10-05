#include "AppMemory.h"
#include "Toolset.h"

void SketchToolSet()
{
    Scene& scene = app_global->GetCurrentScene();
    bool is_sketch_context { scene.toolbox.context == Toolbox::Context::Sketch };

    Clay__OpenElement();

    Clay_ElementDeclaration sketch_create_finish_button_config = {
        .id = CLAY_ID("SketchToolsetControlButton"),
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(4),
            .childGap = 4,
            .childAlignment = ALIGN_CENTER,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgBase,
        .cornerRadius = CLAY_CORNER_RADIUS(8),
        .border = (Clay_BorderElementConfig) { .color = GuiTheme.BgDark, .width = { 2, 2, 2, 2, 0 } }
    };

    Clay__ConfigureOpenElement(sketch_create_finish_button_config);

    if (is_sketch_context) {
        DrawIcon(IconId::Check, GuiTheme.AlertSuccess);
        CLAY_TEXT(CLAY_STRING("Finish Sketch"), &TextStyle.body);
        auto finish_sketch_button = [](Clay_ElementId element_id, Clay_PointerData pointer_data, intptr_t user_data) -> void {
            if (pointer_data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
                app_global->GetCurrentScene().toolbox.context = Toolbox::Context::Solid;
            }
        };
        Clay_OnHover(finish_sketch_button, 0u);
    } else {
        DrawIcon(IconId::Plus, GuiTheme.TextBase);
        CLAY_TEXT(CLAY_STRING("Create Sketch"), &TextStyle.body);
        auto start_sketch_button = [](Clay_ElementId element_id, Clay_PointerData pointer_data, intptr_t user_data) -> void {
            if (pointer_data.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
                app_global->GetCurrentScene().toolbox.context = Toolbox::Context::Sketch;
            }
        };
        Clay_OnHover(start_sketch_button, 0u);
    }

    Clay__CloseElement();

    if (!is_sketch_context)
        return;

    // Sketch Group List
    CLAY({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
            // .padding = CLAY_PADDING_ALL(4),
            .childGap = 4,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    })
    {
        BeginToolGroup(CLAY_STRING("Draw"));
        ToolSelectButton("Line", Unknown, &ToolPlaceholderFunction);
        EndToolGroup();

        BeginToolGroup(CLAY_STRING("Dimensions"));
        ToolSelectButton("Length", Unknown, &ToolPlaceholderFunction);
        ToolSelectButton("Angle", Unknown, &ToolPlaceholderFunction);
        EndToolGroup();

        BeginToolGroup(CLAY_STRING("Constraints"));
        ToolSelectButton("Coincident", Unknown, &ToolPlaceholderFunction);
        EndToolGroup();
    };
}
