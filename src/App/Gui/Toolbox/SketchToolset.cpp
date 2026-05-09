#include "Scene.h"
#include "Toolset.h"
#include "CreateSketchCommand.h"
#include "GeometryEngine.h"
#include <string>
#include <cstring>
#include <optional>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// Helpers

static bool PlaneSelectButton(const char* label, Clay_ElementId id)
{
    bool clicked = false;
    CLAY({ .id = id,
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
            .padding = CLAY_PADDING_ALL(6),
            .childAlignment = ALIGN_CENTER,
        },
        .backgroundColor = Clay_Hovered() ? GuiTheme.BgLight : GuiTheme.BgBase,
        .cornerRadius = CLAY_CORNER_RADIUS(6),
        .border = (Clay_BorderElementConfig) { .color = GuiTheme.AccentPrimary, .width = { 1, 1, 1, 1, 0 } } })
    {
        static Clay_String lbl {};
        lbl = { true, (s32)strlen(label), label };
        CLAY_TEXT(lbl, &TextStyle.body);
        if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left))
            clicked = true;
    };
    return clicked;
}

// ──────────────────────────────────────────────────────────────────────────────

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
            auto& part_opt = scene.command_toolbox.GetActivePartCommand();
            if (part_opt.has_value()) {
                if (auto* create_sketch = part_opt.value().As<CreateSketchCommand>()) {
                    if (create_sketch->IsValid())
                        scene.geometry.push_back(GeometryEngine::Evaluate(*create_sketch));
                }
            }
            scene.command_toolbox.FinishPartCommand();
        }
    };

    // ── Plane selection ───────────────────────────────────────────────────────
    // Shown as soon as a CreateSketch command exists but before a plane is set.
    // Retrieves the command and sets its plane directly (explicit modify pattern).
    auto& part_opt = scene.command_toolbox.GetActivePartCommand();
    auto* create_sketch = part_opt.has_value()
        ? part_opt.value().As<CreateSketchCommand>()
        : nullptr;

    if (create_sketch && !create_sketch->plane.has_value()) {
        CLAY({
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                .padding = CLAY_PADDING_ALL(4),
                .childGap = 6,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
        })
        {
            CLAY_TEXT(CLAY_STRING("Select Sketch Plane"), &TextStyle.body);

            if (PlaneSelectButton("XY Plane", CLAY_ID("PlaneXY")))
                create_sketch->plane = Geometry::SketchPlane::XY;
            if (PlaneSelectButton("XZ Plane", CLAY_ID("PlaneXZ")))
                create_sketch->plane = Geometry::SketchPlane::XZ;
            if (PlaneSelectButton("YZ Plane", CLAY_ID("PlaneYZ")))
                create_sketch->plane = Geometry::SketchPlane::YZ;
        };
        return; // nothing else until plane is selected
    }

    // ── Sketch tools ──────────────────────────────────────────────────────────
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
            if (ToolSelectButton("Line", IconId::Unknown)) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Line);
            }
            if (ToolSelectButton("Arc", IconId::Unknown)) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Arc);
            }
            if (ToolSelectButton("Circle", IconId::Unknown)) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Circle);
            }
            EndToolGroup();

            BeginToolGroup(CLAY_STRING("Dimensions"));
            if (ToolSelectButton("Dimension", IconId::Unknown)) {
                scene.command_toolbox.StartSketchCommand(SketchCommandType::Dimension);
            }
            EndToolGroup();

            BeginToolGroup(CLAY_STRING("Constraints"));
            ToolSelectButton("Coincident", IconId::Unknown);
            EndToolGroup();
        };

    } else {

        auto& cmd_opt = scene.command_toolbox.GetActiveSketchCommand();
        if (!cmd_opt.has_value())
            return;

        if (cmd_opt.value().IsType(SketchCommandType::Line)) {

            // Build per-row label strings that outlive the Clay layout pass.
            static std::vector<std::string> s_line_labels;
            s_line_labels.clear();
            if (create_sketch) {
                size_t line_num = 1;
                for (auto& f : create_sketch->history) {
                    if (f.IsType(SketchCommandType::Line))
                        s_line_labels.push_back("Line " + std::to_string(line_num++));
                    else
                        s_line_labels.emplace_back();
                }
            }

            // Line list — to_delete uses CommandId for ID-based deletion.
            std::optional<CommandId> to_delete;

            CLAY({
                .layout = {
                    .sizing = LAYOUT_EXPAND,
                    .childGap = 4,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
            })
            {
                CLAY({
                    .id = CLAY_ID("LineList"),
                    .layout = {
                        .sizing = LAYOUT_EXPAND,
                        .childGap = 2,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .clip = { .vertical = true },
                })
                {
                    if (create_sketch) {
                        for (size_t i = 0; i < create_sketch->history.size(); i++) {
                            if (!create_sketch->history[i].IsType(SketchCommandType::Line))
                                continue;

                            bool row_hovered = Clay_PointerOver(CLAY_IDI("LineItem", (int)i));

                            CLAY({ .id = CLAY_IDI("LineItem", (int)i),
                                .layout = {
                                    .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                                    .padding = CLAY_PADDING_ALL(4),
                                    .childGap = 4,
                                    .childAlignment = ALIGN_CENTER,
                                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                },
                                .backgroundColor = row_hovered ? GuiTheme.BgLight : GuiTheme.BgBase,
                                .cornerRadius = CLAY_CORNER_RADIUS(4) })
                            {
                                CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() } } })
                                {
                                    static Clay_String row_label {};
                                    row_label = { true, (s32)s_line_labels[i].size(), s_line_labels[i].c_str() };
                                    CLAY_TEXT(row_label, &TextStyle.body);
                                };

                                if (row_hovered) {
                                    CLAY({ .id = CLAY_IDI("LineDeleteBtn", (int)i),
                                        .layout = {
                                            .padding = { .left = 6, .right = 6, .top = 2, .bottom = 2 },
                                            .childAlignment = ALIGN_CENTER,
                                        },
                                        .backgroundColor = Clay_Hovered() ? (Clay_Color) { 200, 50, 50, 255 } : (Clay_Color) { 160, 40, 40, 255 },
                                        .cornerRadius = CLAY_CORNER_RADIUS(4) })
                                    {
                                        CLAY_TEXT(CLAY_STRING("Del"), &TextStyle.body);
                                        if (!to_delete.has_value() && Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                                            to_delete = create_sketch->history[i].GetId();
                                        }
                                    };
                                }
                            };
                        }
                    }
                };

                if (to_delete.has_value()) {
                    scene.command_toolbox.DeleteSketchCommand(*to_delete);
                }

                // Finish Line exits line drawing mode, discarding any partial in-progress line.
                CLAY({ .id = CLAY_ID("FinishLineButton"),
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
                    CLAY_TEXT(CLAY_STRING("Finish Line"), &TextStyle.body);
                    if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                        scene.command_toolbox.CancelSketchCommand();
                    }
                };
            };

        } else {

            // Generic active-command view for Arc, Circle, Dimension, etc.
            const char* name = "Unknown";
            switch (cmd_opt.value().GetType()) {
            case SketchCommandType::Line:
                name = "Line";
                break;
            case SketchCommandType::Arc:
                name = "Arc";
                break;
            case SketchCommandType::Circle:
                name = "Circle";
                break;
            case SketchCommandType::Dimension:
                name = "Dimension";
                break;
            }
            static Clay_String label {};
            label = { true, (s32)strlen(name), name };
            CLAY_TEXT(label, &TextStyle.body);

            CLAY({ .id = CLAY_ID("SketchCancelButton"),
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
                CLAY_TEXT(CLAY_STRING("Cancel"), &TextStyle.body);
                if (Inputs::MouseHoveredAndPressed(Inputs::Mouse::Left)) {
                    scene.command_toolbox.CancelSketchCommand();
                }
            };
        }
    }
}
