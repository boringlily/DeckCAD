#include "App.h"
#include "Graphics.h"
#include "Gui/Workbench.cpp" // unity hub: shared Canvas helpers + toolset registry.
#include "Gui/UiStyles.h"
#include "GeometryEngine.h" // UiSketchToolset evaluates sketches on Finish.
#include <print>
#include <string>
#include <vector>

#include <tracy/Tracy.hpp>

// ── Ui tree ───────────────────────────────────────────────────────────────────
// DeckCAD's entire UI, built with the in-repo Ui flexbox framework (part of the
// GRAPHICS module; Clay was removed in the Phase 7 teardown — this is now the
// only UI path). BuildUiTree is declared unconditionally each frame from AppUpdate.
namespace {

// A centered single-title page — the Home/Settings placeholders (App.cpp Clay side).
void UiCenteredPage(Ui::UiId id, std::string_view title)
{
    Ui::LayoutConfig c {};
    c.sizing = UiStyle::Expand();
    c.padding = UiStyle::PaddingAll(8);
    c.gap = 8;
    c.direction = Ui::Direction::LeftToRight;
    c.justify = Ui::Justify::Center;
    c.align = Ui::AlignCross::Center;
    c.background = Ui::Colors().bgBase;
    Ui::OpenElement(c, id);
    UiStyle::Title(title, Ui::HashChild(id, 1));
    Ui::CloseElement();
}

// Phase 4: the AppHeader — Home button, dynamic scene-tab loop (with the active
// tab's close chip), and the New-Scene button. Interaction via IsClicked/IsHovered
// on content-stable NameIds. Mirrors LayoutAppHeader (the Clay version).
void UiAppHeader(AppState& app)
{
    const Ui::ColorScheme& colors = Ui::Colors();

    Ui::LayoutConfig header {};
    header.sizing = { Ui::Grow(), Ui::Fit() };
    header.padding = UiStyle::PaddingAll(4);
    header.gap = 8;
    header.align = Ui::AlignCross::Center;
    header.background = colors.bgDark;
    Ui::OpenElement(header, Ui::NameId("Header"));

    // Home button: highlighted while hovered OR the Home layer is active.
    if (UiStyle::Button("", Ui::NameId("Header::ButtonHome"),
            { .icon = IconId::Home, .active = app.IsHomeLayerActive(), .bg = colors.bgDark })) {
        app.ActivateHomeLayer();
    }

    // One tab per scene. filename is a scene-owned std::string that outlives the
    // frame, so string_view into it is safe (no static Clay_String shim needed).
    SceneList& scenes = app.GetSceneList();
    u32 scene_id = 0;
    for (auto& scene : scenes) {
        bool active = static_cast<s32>(scene_id) == app.GetActiveSceneId();
        Ui::UiId tabId = Ui::NameId("SceneTab", scene_id);

        Ui::LayoutConfig tab {};
        tab.sizing = { Ui::Fit(), Ui::Grow() };
        tab.padding = UiStyle::PaddingAll(4);
        tab.gap = 4;
        tab.justify = Ui::Justify::Center;
        tab.align = Ui::AlignCross::Center;
        // Subtree hover so the tab stays lit while the pointer is over its close chip.
        tab.background = (Ui::IsHoverWithin(tabId) || active) ? colors.bgLight : colors.bgDark;
        tab.cornerRadius = 4;
        Ui::OpenElement(tab, tabId);

        UiStyle::DecorText(scene.filename, FontId::Regular, 16,
            active ? colors.textBase : colors.textMuted, Ui::HashChild(tabId, 1));

        // Close chip on the active tab: hovers red, but closing scenes is not yet
        // implemented (matches Clay — no handler; it just sits on the active tab).
        if (active) {
            Ui::UiId closeId = Ui::NameId("SceneClose", scene_id);
            bool chipHover = Ui::IsHovered(closeId);
            Ui::LayoutConfig chip {};
            chip.sizing = { Ui::Fit(), Ui::Fit() };
            chip.justify = Ui::Justify::Center;
            chip.align = Ui::AlignCross::Center;
            chip.background = chipHover ? colors.bgBase : colors.bgLight;
            chip.cornerRadius = 4;
            Ui::OpenElement(chip, closeId);
            UiStyle::DrawIcon(IconId::Exit, chipHover ? colors.alertDanger : colors.textBase);
            Ui::CloseElement();
        }

        Ui::CloseElement(); // tab

        if (Ui::IsClicked(tabId)) {
            if (app.TryActivateScene(scene_id)) {
                app.ActivateSceneLayer();
            }
            // else: TODO surface "failed to activate scene" (unimplemented in Clay too).
        }
        scene_id++;
    }

    // New Scene button.
    if (UiStyle::Button("", Ui::NameId("Header::ButtonNewScene"),
            { .icon = IconId::Plus, .bg = colors.bgDark })) {
        app.CreateNewScene();
    }

    Ui::CloseElement(); // Header
}

// Phase 2: Explorer — the grow(min,max)-width panel the workbench row depends on.
// Body is intentionally empty (matches the current Clay Explorer).
void UiExplorer(Scene& scene)
{
    (void)scene;
    Ui::LayoutConfig c {};
    c.sizing = UiStyle::ExpandMinMaxWidth(100, 350);
    c.padding = UiStyle::PaddingAll(4);
    c.gap = 8;
    c.direction = Ui::Direction::TopToBottom;
    c.align = Ui::AlignCross::Center;
    c.background = Ui::Colors().bgBase;
    Ui::OpenElement(c, Ui::NameId("WorkbenchExplorer"));
    Ui::CloseElement();
}

// ── Phase 5: Toolbox + toolsets ───────────────────────────────────────────────

// A full-width tool button (icon + left-aligned label) — the ToolSelectButton idiom.
bool UiToolButton(std::string_view label, Ui::UiId id)
{
    return UiStyle::Button(label, id,
        { .icon = IconId::Unknown, .sizing = { Ui::Grow(), Ui::Fit() }, .gap = 4 });
}

// A bordered, titled group of tools (BeginToolGroup/EndToolGroup). Must be balanced.
void UiBeginToolGroup(std::string_view name, Ui::UiId id)
{
    Ui::LayoutConfig g {};
    g.sizing = { Ui::Grow(), Ui::Fit() };
    g.padding = UiStyle::PaddingAll(8);
    g.gap = 8;
    g.direction = Ui::Direction::TopToBottom;
    g.align = Ui::AlignCross::Stretch; // tool buttons fill the group width.
    g.cornerRadius = 10;
    g.border = { 2, 2, 2, 2 };
    g.borderColor = Ui::Colors().borderBase;
    Ui::OpenElement(g, id);
    UiStyle::Subtitle(name, Ui::HashChild(id, 1));
}
void UiEndToolGroup() { Ui::CloseElement(); }

// The Line command view: a real scroll container of line rows (each with a
// hover-only Del button) + a Finish Line button. Deferred delete after the loop.
void UiLineToolView(Scene& scene, CreateSketchCommand* create_sketch)
{
    const Ui::ColorScheme& colors = Ui::Colors();

    // Per-row labels that must outlive the frame (drawn as string_view into these).
    static std::vector<std::string> s_labels;
    s_labels.clear();
    if (create_sketch) {
        size_t n = 1;
        for (auto& f : create_sketch->history) {
            if (f.IsType(SketchCommandType::Line))
                s_labels.push_back("Line " + std::to_string(n++));
            else
                s_labels.emplace_back();
        }
    }

    std::optional<CommandId> to_delete;

    Ui::LayoutConfig outer {};
    outer.sizing = UiStyle::Expand();
    outer.gap = 4;
    outer.direction = Ui::Direction::TopToBottom;
    outer.align = Ui::AlignCross::Stretch; // list + finish button fill the width.
    Ui::OpenElement(outer, Ui::NameId("LineToolView"));

    // scroll=true delivers the real scrolling the Clay LineList never wired up.
    Ui::LayoutConfig listCfg {};
    listCfg.sizing = UiStyle::Expand();
    listCfg.gap = 2;
    listCfg.direction = Ui::Direction::TopToBottom;
    listCfg.align = Ui::AlignCross::Stretch; // rows fill the width.
    listCfg.scroll = true;
    Ui::OpenElement(listCfg, Ui::NameId("LineList"));

    if (create_sketch) {
        for (size_t i = 0; i < create_sketch->history.size(); i++) {
            if (!create_sketch->history[i].IsType(SketchCommandType::Line))
                continue;

            Ui::UiId rowId = Ui::NameId("LineItem", static_cast<u32>(i));
            // Subtree hover: the row stays hovered while the pointer is over its own
            // revealed Del button, so the button (a hit-testable child) doesn't steal
            // the row's hover and flip-flop itself in/out of existence every frame.
            bool row_hovered = Ui::IsHoverWithin(rowId);

            Ui::LayoutConfig row {};
            row.sizing = { Ui::Grow(), Ui::Fit() };
            row.padding = UiStyle::PaddingAll(4);
            row.gap = 4;
            row.align = Ui::AlignCross::Center;
            row.background = row_hovered ? colors.bgLight : colors.bgBase;
            row.cornerRadius = 4;
            Ui::OpenElement(row, rowId);

            // Growing label cell (decorative — lets the row capture hover/click).
            Ui::LayoutConfig cell {};
            cell.sizing = { Ui::Grow(), Ui::Fit() };
            cell.hitTestable = false;
            Ui::OpenElement(cell, Ui::HashChild(rowId, 1));
            UiStyle::DecorText(s_labels[i], FontId::Regular, 16, colors.textBase, Ui::HashChild(rowId, 2));
            Ui::CloseElement();

            if (row_hovered) {
                Ui::UiId delId = Ui::NameId("LineDeleteBtn", static_cast<u32>(i));
                bool delHover = Ui::IsHovered(delId);
                Ui::LayoutConfig del {};
                del.padding = { 6, 6, 2, 2 };
                del.align = Ui::AlignCross::Center;
                del.background = delHover ? Ui::UiColor { 200, 50, 50, 255 } : Ui::UiColor { 160, 40, 40, 255 };
                del.cornerRadius = 4;
                Ui::OpenElement(del, delId);
                UiStyle::DecorText("Del", FontId::Regular, 16, colors.textBase, Ui::HashChild(delId, 1));
                Ui::CloseElement();
                if (!to_delete.has_value() && Ui::IsClicked(delId)) {
                    to_delete = create_sketch->history[i].GetId();
                }
            }

            Ui::CloseElement(); // row
        }
    }

    Ui::CloseElement(); // LineList

    if (to_delete.has_value()) {
        scene.command_toolbox.DeleteSketchCommand(*to_delete);
    }

    if (UiStyle::Button("Finish Line", Ui::NameId("FinishLineButton"),
            { .icon = IconId::Unknown, .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .gap = 4, .border = { 2, 2, 2, 2 }, .borderColor = colors.bgDark })) {
        scene.command_toolbox.CancelSketchCommand();
    }

    Ui::CloseElement(); // LineToolView
}

void UiPartToolset(Scene& scene)
{
    if (UiToolButton("Create Sketch", Ui::NameId("Tool::CreateSketch"))) {
        scene.command_toolbox.StartCreateSketch();
    }
}

void UiInspectToolset(Scene& scene)
{
    (void)scene;
    UiStyle::Body("Inspector coming soon.", Ui::NameId("Inspect::soon"));
}

void UiSketchToolset(Scene& scene)
{
    const Ui::ColorScheme& colors = Ui::Colors();

    // Finish Sketch: end the CreateSketch part command, evaluating valid geometry.
    if (UiStyle::Button("Finish Sketch", Ui::NameId("SketchFinishButton"),
            { .icon = IconId::Unknown, .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .gap = 4, .border = { 2, 2, 2, 2 }, .borderColor = colors.bgDark })) {
        auto& part_opt = scene.command_toolbox.GetActivePartCommand();
        if (part_opt.has_value()) {
            if (auto* cs = part_opt.value().As<CreateSketchCommand>()) {
                if (cs->IsValid())
                    scene.geometry.push_back(GeometryEngine::Evaluate(*cs));
            }
        }
        scene.command_toolbox.FinishPartCommand();
    }

    auto& part_opt = scene.command_toolbox.GetActivePartCommand();
    auto* create_sketch = part_opt.has_value() ? part_opt.value().As<CreateSketchCommand>() : nullptr;

    // Plane selection — shown until the CreateSketch command has a plane.
    if (create_sketch && !create_sketch->plane.has_value()) {
        Ui::LayoutConfig group {};
        group.sizing = { Ui::Grow(), Ui::Fit() };
        group.padding = UiStyle::PaddingAll(4);
        group.gap = 6;
        group.direction = Ui::Direction::TopToBottom;
        group.align = Ui::AlignCross::Stretch; // plane buttons fill the width.
        Ui::OpenElement(group, Ui::NameId("PlaneSelect"));

        UiStyle::Body("Select Sketch Plane", Ui::NameId("PlaneSelect::title"));

        UiStyle::ButtonStyle planeStyle {};
        planeStyle.sizing = { Ui::Grow(), Ui::Fit() };
        planeStyle.justify = Ui::Justify::Center;
        planeStyle.padding = UiStyle::PaddingAll(6);
        planeStyle.cornerRadius = 6;
        planeStyle.border = { 1, 1, 1, 1 };
        planeStyle.borderColor = colors.accentPrimary;

        if (UiStyle::Button("XY Plane", Ui::NameId("PlaneXY"), planeStyle))
            create_sketch->plane = Geometry::SketchPlane::XY;
        if (UiStyle::Button("XZ Plane", Ui::NameId("PlaneXZ"), planeStyle))
            create_sketch->plane = Geometry::SketchPlane::XZ;
        if (UiStyle::Button("YZ Plane", Ui::NameId("PlaneYZ"), planeStyle))
            create_sketch->plane = Geometry::SketchPlane::YZ;

        Ui::CloseElement(); // PlaneSelect
        return;
    }

    // Sketch tools (no active command) vs the active-command view.
    if (!scene.command_toolbox.IsSketchCommandActive()) {
        Ui::LayoutConfig list {};
        list.sizing = { Ui::Grow(), Ui::Fit() };
        list.gap = 4;
        list.direction = Ui::Direction::TopToBottom;
        list.align = Ui::AlignCross::Stretch; // tool groups fill the width.
        Ui::OpenElement(list, Ui::NameId("SketchTools"));

        UiBeginToolGroup("Draw", Ui::NameId("Group::Draw"));
        if (UiToolButton("Line", Ui::NameId("Tool::Line")))
            scene.command_toolbox.StartSketchCommand(SketchCommandType::Line);
        if (UiToolButton("Arc", Ui::NameId("Tool::Arc")))
            scene.command_toolbox.StartSketchCommand(SketchCommandType::Arc);
        if (UiToolButton("Circle", Ui::NameId("Tool::Circle")))
            scene.command_toolbox.StartSketchCommand(SketchCommandType::Circle);
        UiEndToolGroup();

        UiBeginToolGroup("Dimensions", Ui::NameId("Group::Dimensions"));
        if (UiToolButton("Dimension", Ui::NameId("Tool::Dimension")))
            scene.command_toolbox.StartSketchCommand(SketchCommandType::Dimension);
        UiEndToolGroup();

        UiBeginToolGroup("Constraints", Ui::NameId("Group::Constraints"));
        UiToolButton("Coincident", Ui::NameId("Tool::Coincident"));
        UiEndToolGroup();

        Ui::CloseElement(); // SketchTools
        return;
    }

    auto& cmd_opt = scene.command_toolbox.GetActiveSketchCommand();
    if (!cmd_opt.has_value())
        return;

    if (cmd_opt.value().IsType(SketchCommandType::Line)) {
        UiLineToolView(scene, create_sketch);
    } else {
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
        UiStyle::Body(name, Ui::NameId("ActiveCmd::name"));
        if (UiStyle::Button("Cancel", Ui::NameId("SketchCancelButton"),
                { .icon = IconId::Unknown, .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .gap = 4, .border = { 2, 2, 2, 2 }, .borderColor = colors.bgDark })) {
            scene.command_toolbox.CancelSketchCommand();
        }
    }
}

// The Toolbox panel: context-filtered tab bar (active tab gets a 2px border) + the
// active toolset's content. Reuses the shared toolset_list for names/visibility.
void UiToolbox(Scene& scene)
{
    const Ui::ColorScheme& colors = Ui::Colors();
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

    // If the active tab is hidden in this context, clamp to the first visible one.
    if (!tab_visible(toolset_list[scene.toolbox.active_toolset])) {
        for (u32 i = 0; i < toolset_list.size(); ++i) {
            if (tab_visible(toolset_list[i])) {
                scene.toolbox.active_toolset = i;
                break;
            }
        }
    }

    Ui::LayoutConfig box {};
    box.sizing = UiStyle::ExpandMinMaxWidth(200, 300);
    box.direction = Ui::Direction::TopToBottom;
    box.align = Ui::AlignCross::Stretch; // tab bar + content fill the toolbox width.
    box.background = colors.bgBase;
    Ui::OpenElement(box, Ui::NameId("Toolbox"));

    Ui::LayoutConfig tabsCfg {};
    tabsCfg.sizing = { Ui::Fit(), Ui::Fit() };
    tabsCfg.padding = UiStyle::PaddingAll(4);
    tabsCfg.gap = 8;
    tabsCfg.background = colors.bgBase;
    Ui::OpenElement(tabsCfg, Ui::NameId("ToolsetTabs"));

    for (u32 i = 0; i < toolset_list.size(); ++i) {
        const Toolset& toolset = toolset_list[i];
        if (!tab_visible(toolset))
            continue;
        bool tab_active = (i == scene.toolbox.active_toolset);
        Ui::UiId tabId = Ui::NameId("ToolsetTab", i);

        UiStyle::ButtonStyle st {};
        st.sizing = { Ui::Grow(), Ui::Fit() };
        st.justify = Ui::Justify::Center;
        st.padding = { 16, 16, 4, 0 }; // sides + top (no bottom)
        st.active = tab_active;
        st.border = tab_active ? Ui::Edges { 2, 2, 2, 2 } : Ui::Edges {};
        st.borderColor = colors.accentPrimary;
        st.labelColor = tab_active ? colors.textBase : colors.textMuted;
        if (UiStyle::Button(toolset.name, tabId, st)) {
            scene.toolbox.active_toolset = i;
        }
    }

    Ui::CloseElement(); // ToolsetTabs

    Ui::LayoutConfig content {};
    content.sizing = UiStyle::Expand();
    content.padding = UiStyle::PaddingAll(4);
    content.gap = 16;
    content.direction = Ui::Direction::TopToBottom;
    content.align = Ui::AlignCross::Stretch; // tool buttons/groups fill the width.
    content.background = colors.bgBase;
    Ui::OpenElement(content, Ui::NameId("Toolset"));

    switch (scene.toolbox.active_toolset) {
    case 0:
        UiPartToolset(scene);
        break;
    case 1:
        UiSketchToolset(scene);
        break;
    case 2:
        UiInspectToolset(scene);
        break;
    default:
        break;
    }

    Ui::CloseElement(); // Toolset
    Ui::CloseElement(); // Toolbox
}

// ── Phase 6: Canvas (3D viewport via Ui::Raylib::Canvas3D) ────────────────────
// The 3D scene renders at DISPATCH — Canvas3D composites into the final layout rect
// with no frame lag. All interaction runs here at BUILD time, gated on the viewport
// being hovered (IsHovered, last frame) and ray-picking through canvas.lastRect (the
// canvas sub-viewport, so picking is correct even though the 3D view doesn't fill the
// window — the Clay path used full-window coords and was imprecise). Right/middle mouse
// + wheel are read raw from raylib (the Ui PointerState only tracks the left button).
// Reuses the Clay canvas helpers (ComputeHoveredOriginPlane / SketchPointToWorld /
// UI::DrawGrid / plane colors) — same translation unit via Workbench.cpp -> Canvas.cpp.
class SceneViewport : public Ui::Raylib::Canvas3D {
public:
    Scene* scene { nullptr };
    bool sketchValid { false };
    bool sketchActive { false };
    std::optional<Geometry::SketchPlane> activePlane {};
    std::optional<Geometry::SketchPlane> hoveredPlane {};
    Geometry::Point2 cursorOnPlane {};

    // Rendering half of the Clay CanvasRenderToTexture, reading state cached at build.
    void Draw3D(Ui::Rect) override
    {
        if (sketchValid && activePlane.has_value()) {
            Geometry::SketchPlane sp = *activePlane;
            UI::DrawGrid(SketchPlaneToOriginPlane(sp), 100, 1.0f);

            auto& part_opt = scene->command_toolbox.GetActivePartCommand();
            auto* create_sketch = part_opt.has_value() ? part_opt.value().As<CreateSketchCommand>() : nullptr;
            if (create_sketch) {
                for (auto& feature : create_sketch->history) {
                    if (!feature.IsType(SketchCommandType::Line))
                        continue;
                    auto* line = feature.As<SketchLineCommand>();
                    if (!line || !line->start.has_value() || !line->end.has_value())
                        continue;
                    DrawLine3D(SketchPointToWorld(*line->start, sp), SketchPointToWorld(*line->end, sp), BLACK);
                }

                // Preview: sphere at placed start, rubber-band line, sphere at cursor.
                auto& active_sketch = scene->command_toolbox.GetActiveSketchCommand();
                if (active_sketch.has_value() && active_sketch.value().IsType(SketchCommandType::Line)) {
                    if (auto* line_cmd = active_sketch.value().As<SketchLineCommand>()) {
                        Vector3 cursor_world = SketchPointToWorld(cursorOnPlane, sp);
                        if (line_cmd->start.has_value()) {
                            Vector3 start_world = SketchPointToWorld(*line_cmd->start, sp);
                            DrawSphereEx(start_world, 0.1f, 6, 8, BLUE);
                            DrawLine3D(start_world, cursor_world, GRAY);
                        }
                        DrawSphereEx(cursor_world, 0.07f, 6, 8, SKYBLUE);
                    }
                }
            }
        } else if (sketchActive) {
            // Plane-selection: all three origin planes; hovered highlighted, rest dimmed.
            UI::DrawOriginPlane(UI::OriginPlane::XY, { 0, 0, 0 }, ORIGIN_PLANE_SIZE,
                hoveredPlane == Geometry::SketchPlane::XY ? PLANE_COLOR_HOVER : PLANE_COLOR_XY_DIM);
            UI::DrawOriginPlane(UI::OriginPlane::XZ, { 0, 0, 0 }, ORIGIN_PLANE_SIZE,
                hoveredPlane == Geometry::SketchPlane::XZ ? PLANE_COLOR_HOVER : PLANE_COLOR_XZ_DIM);
            UI::DrawOriginPlane(UI::OriginPlane::YZ, { 0, 0, 0 }, ORIGIN_PLANE_SIZE,
                hoveredPlane == Geometry::SketchPlane::YZ ? PLANE_COLOR_HOVER : PLANE_COLOR_YZ_DIM);
            UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);
        } else {
            UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);
            for (auto& sketch : scene->geometry) {
                for (auto& line : sketch.lines) {
                    DrawLine3D(SketchPointToWorld(line.start, sketch.plane),
                        SketchPointToWorld(line.end, sketch.plane), BLACK);
                }
            }
        }
    }
};

// Ray through the mouse, corrected for the canvas sub-viewport (local mouse + canvas
// dims). Falls back to the window ray on the first frame before lastRect is known.
Ray CanvasRayFromMouse(const Camera3D& cam, Ui::Rect rect)
{
    Vector2 m = GetMousePosition();
    if (rect.w < 1.0f || rect.h < 1.0f) {
        return GetScreenToWorldRay(m, cam);
    }
    Vector2 local { m.x - rect.x, m.y - rect.y };
    return GetScreenToWorldRayEx(local, cam, static_cast<int>(rect.w + 0.5f), static_cast<int>(rect.h + 0.5f));
}

// The single canvas viewport (one canvas at a time). Held here so AppShutdown() can
// free its RenderTexture while the GL context is still live (see AppShutdown).
SceneViewport* g_viewport { nullptr };

// Phase 6: the Canvas. Interaction (camera / plane pick / line placement) at build
// time; the 3D render is deferred to dispatch by Canvas3D. Mirrors the Clay
// CanvasRenderToTexture + LayoutCanvas.
//
// Timing note: both the hover gate (viewport.Hovered()) and the pick rect
// (viewport.lastRect) reflect the PREVIOUS frame's layout/hit-test — the standard
// 1-frame lag of this immediate-mode framework (every IsHovered is last-frame). It
// differs slightly from the Clay path, which gated on the current-frame pointer
// position: on the single frame the cursor crosses the canvas edge with a button
// down-edge, a click may be honored/ignored one frame later, and during a live
// window resize the preview cursor can sit one frame stale. Both are self-correcting
// and consistent with the rest of the Ui path; Hovered() is also occlusion-aware
// (a floating panel over the canvas correctly suppresses interaction), which a raw
// rect test would not be — so the lag is kept deliberately.
void UiCanvas(Scene& scene)
{
    static SceneViewport viewport;
    g_viewport = &viewport; // let AppShutdown() reach it for GL-live teardown.
    viewport.scene = &scene;

    bool is_sketch_active = scene.command_toolbox.IsSketchContext();
    auto active_plane = scene.command_toolbox.GetActiveSketchPlane();
    bool sketch_valid = active_plane.has_value();
    bool hovered = viewport.Hovered();

    // Camera snap to the confirmed sketch plane; restore isometric when sketch exits.
    if (sketch_valid && !scene.was_sketch_valid) {
        scene.camera.SetOrientation(CanvasCamera::OrientationForSketchPlane(*active_plane));
    }
    if (!is_sketch_active && scene.was_sketch_active) {
        scene.camera.SetOrientation(CanvasCamera::CameraOrientation::Isometric_XYZ);
    }
    scene.was_sketch_active = is_sketch_active;
    scene.was_sketch_valid = sketch_valid;

    // Camera input (deltas + wheel, viewport-independent): 2D lock once the plane is set.
    if (hovered) {
        if (sketch_valid) {
            scene.camera.ProcessPan2D();
        } else {
            scene.camera.ProcessPanTilt();
        }
    }

    Ray ray = CanvasRayFromMouse(scene.camera.raylib_camera, viewport.lastRect);

    // Plane selection: hover highlight + click-to-confirm.
    scene.hovered_plane = std::nullopt;
    if (is_sketch_active && !sketch_valid && hovered) {
        scene.hovered_plane = ComputeHoveredOriginPlane(ray, ORIGIN_PLANE_EXTENT);
        if (scene.hovered_plane.has_value() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            auto& part_opt = scene.command_toolbox.GetActivePartCommand();
            if (part_opt.has_value()) {
                if (auto* cmd = part_opt.value().As<CreateSketchCommand>()) {
                    cmd->plane = *scene.hovered_plane;
                }
            }
        }
    }

    // Line placement: right-click resets the start; left-click places (with chaining).
    if (sketch_valid && hovered && scene.command_toolbox.IsSketchCommandActive()) {
        auto& sketch_opt = scene.command_toolbox.GetActiveSketchCommand();
        if (sketch_opt.has_value() && sketch_opt.value().IsType(SketchCommandType::Line)) {
            auto* line_cmd = sketch_opt.value().As<SketchLineCommand>();
            if (line_cmd && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                line_cmd->start = std::nullopt;
            } else if (line_cmd && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                auto hit = scene.camera.GetMouseOnSketchPlane(*active_plane, ray);
                if (!line_cmd->start.has_value()) {
                    line_cmd->start = hit;
                } else {
                    line_cmd->end = hit;
                    Geometry::Point2 chain_start = hit;
                    scene.command_toolbox.FinishSketchCommand();
                    scene.command_toolbox.StartSketchCommand(SketchCommandType::Line);
                    auto& next_opt = scene.command_toolbox.GetActiveSketchCommand();
                    if (next_opt.has_value() && next_opt.value().IsType(SketchCommandType::Line)) {
                        if (auto* next_line = next_opt.value().As<SketchLineCommand>()) {
                            next_line->start = chain_start;
                        }
                    }
                }
            }
        }
    }

    // Cache state for Draw3D (dispatch) and hand the live camera to the viewport.
    viewport.camera = scene.camera.raylib_camera;
    viewport.background = Ui::UiColor { 255, 255, 255, 255 }; // white canvas (matches Clay)
    viewport.sketchValid = sketch_valid;
    viewport.sketchActive = is_sketch_active;
    viewport.activePlane = active_plane;
    viewport.hoveredPlane = scene.hovered_plane;
    viewport.cursorOnPlane = sketch_valid ? scene.camera.GetMouseOnSketchPlane(*active_plane, ray) : Geometry::Point2 {};

    viewport.Render(Ui::NameId("CanvasPanel"), UiStyle::ExpandMinMaxWidth(500));
}

// Phase 3: the Workbench frame — inner three-sibling grow row + footer. Explorer,
// Canvas (Phase 6, real), and Toolbox (Phase 5, real) are all ported.
void UiWorkbench(Scene& scene)
{
    Ui::LayoutConfig wb {};
    wb.sizing = UiStyle::Expand();
    wb.direction = Ui::Direction::TopToBottom;
    wb.align = Ui::AlignCross::Stretch;
    Ui::OpenElement(wb, Ui::NameId("Workbench"));

    Ui::LayoutConfig inner {};
    inner.sizing = UiStyle::Expand();
    inner.direction = Ui::Direction::LeftToRight;
    inner.align = Ui::AlignCross::Stretch;
    Ui::OpenElement(inner, Ui::NameId("WorkbenchInner"));

    UiExplorer(scene);
    UiCanvas(scene);
    UiToolbox(scene);

    Ui::CloseElement(); // WorkbenchInner

    Ui::LayoutConfig footer {};
    footer.sizing = { Ui::Grow(), Ui::Fit() };
    footer.padding = UiStyle::PaddingAll(8);
    footer.direction = Ui::Direction::LeftToRight;
    footer.align = Ui::AlignCross::Center;
    footer.background = Ui::Colors().bgDark;
    Ui::OpenElement(footer, Ui::NameId("WorkbenchFooter"));
    UiStyle::Body("Active Mode: {UNKNOWN}", Ui::NameId("WorkbenchFooter::text"));
    Ui::CloseElement();

    Ui::CloseElement(); // Workbench
}

void BuildUiTree(AppState& app)
{
    Ui::LayoutConfig outer {};
    outer.sizing = UiStyle::Expand();
    outer.direction = Ui::Direction::TopToBottom;
    outer.align = Ui::AlignCross::Stretch; // children fill the window width.
    Ui::OpenElement(outer, Ui::NameId("OuterContainer"));

    UiAppHeader(app);

    switch (app.GetActiveLayer()) {
    case AppLayer::Home_Layer:
        UiCenteredPage(Ui::NameId("HomePage"), "This is going to be the homepage.");
        break;
    case AppLayer::Settings_Layer:
        UiCenteredPage(Ui::NameId("SettingsPage"), "This is going to be the global settings page.");
        break;
    case AppLayer::Scene_Layer: {
        Scene* scene = app.GetActiveScene();
        if (scene != nullptr) {
            UiWorkbench(*scene);
        }
        break;
    }
    }

    Ui::CloseElement(); // OuterContainer
}

} // namespace

#ifdef __cplusplus
extern "C" {
#endif

// Free App-owned GPU resources before the window/GL context is torn down. Called
// from main() after the loop, before CloseWindow. Idempotent: Canvas3D::Unload
// zeroes the texture id, so the later ~SceneViewport (at DLL detach / atexit, after
// the context is gone) becomes a no-op instead of a context-less GL delete.
APP_API
void AppShutdown()
{
    if (g_viewport != nullptr) {
        g_viewport->Unload();
    }
}

APP_API
void AppUpdate(AppState& app)
{
    ZoneScoped;

    Graphics::BeginFrame();
    BuildUiTree(app);
    Graphics::EndFrame();
}

#ifdef __cplusplus
}
#endif
