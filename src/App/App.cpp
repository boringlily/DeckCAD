#include "App.h"
#include "Graphics.h"
#include "Gui/Workbench.cpp" // unity hub: shared Canvas helpers.
#include "Gui/Explorer.h" // UiExplorer: the command tree + ParameterTable.
#include "Gui/ExpressionField.h"
#include "Gui/Storage.h" // save / load / auto-save (.dcad).
#include "Gui/UiStyles.h"
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

using AppUi::UiExplorer;

// ── Toolbox ───────────────────────────────────────────────────────────────────
// The right-hand panel. It renders exactly what the current context reports from
// AvailableTools(), grouped by the ToolInfo table.
//
// There is no tab bar and no TabContext visibility enum any more, and — the point of
// the exercise — no switch. "Only sketch tools are legal while sketching" is not
// special-cased here; it falls out of which context is on top of the stack. Adding a
// tool is a row in kToolTable plus its Finish() case, with nothing to change in the UI.

// Draw one group box of tools, given the subset of `tools` belonging to it.
void UiToolGroup(std::string_view group, const std::vector<ToolId>& tools, Scene& scene)
{
    Workbench& wb = scene.workbench;

    Ui::LayoutConfig g {};
    g.sizing = { Ui::Grow(), Ui::Fit() };
    g.padding = UiStyle::PaddingAll(8);
    g.gap = 6;
    g.direction = Ui::Direction::TopToBottom;
    g.align = Ui::AlignCross::Stretch;
    g.cornerRadius = 10;
    g.border = { 2, 2, 2, 2 };
    g.borderColor = Ui::Colors().borderBase;
    Ui::OpenElement(g, Ui::NameId("ToolGroup", Ui::NameId(group.data(), static_cast<u32>(group.size()))));

    UiStyle::Subtitle(group, Ui::NameId("ToolGroupTitle", Ui::NameId(group.data(), static_cast<u32>(group.size()))));

    for (ToolId id : tools) {
        const ToolInfo* info = FindTool(id);
        if (!info || info->group != group) {
            continue;
        }
        bool active = wb.ActiveTool().Active() && wb.ActiveTool().Id() == id;
        if (UiStyle::Button(info->name, Ui::NameId("Tool", static_cast<u32>(id)),
                { .icon = IconId::Unknown, .sizing = { Ui::Grow(), Ui::Fit() }, .gap = 4, .active = active })) {
            wb.StartTool(id);
            scene.toolbox.ClearValue();
        }
    }

    Ui::CloseElement();
}

// The active gesture: what it still wants, its value field if it needs one, and Cancel.
void UiActiveToolView(Scene& scene)
{
    Workbench& wb = scene.workbench;
    Tool& tool = wb.ActiveTool();
    const ToolInfo* info = tool.Info();
    if (!info) {
        return;
    }

    const Ui::ColorScheme& colors = Ui::Colors();

    Ui::LayoutConfig box {};
    box.sizing = { Ui::Grow(), Ui::Fit() };
    box.padding = UiStyle::PaddingAll(8);
    box.gap = 6;
    box.direction = Ui::Direction::TopToBottom;
    box.align = Ui::AlignCross::Stretch;
    box.cornerRadius = 10;
    box.border = { 2, 2, 2, 2 };
    box.borderColor = colors.accentPrimary;
    Ui::OpenElement(box, Ui::NameId("ActiveTool"));

    UiStyle::Subtitle(info->name, Ui::NameId("ActiveTool::Name"));

    // Progress hint, generated from the tool's declared inputs — no per-tool text.
    static std::string hint;
    hint.clear();
    if (u32 remaining = tool.PointsRemaining()) {
        hint = "Click " + std::to_string(remaining) + " more point" + (remaining > 1 ? "s" : "");
    } else if (info->picks && tool.Picks().size() < info->picks) {
        hint = "Select " + std::to_string(info->picks - tool.Picks().size()) + " more";
    } else if (info->plane && !tool.Plane().has_value()) {
        hint = "Pick a plane in the canvas";
    } else if (info->value && tool.Value().empty()) {
        hint = "Enter a value";
    }
    if (!hint.empty()) {
        UiStyle::Muted(hint, Ui::NameId("ActiveTool::Hint"));
    }

    // The value field — the toolbox's first editable input. A dimension's value is an
    // expression, so it gets the same parser-backed field as the ParameterTable.
    if (info->value) {
        static ExprField::ParserBinding binding {};
        binding.engine = &wb.Params();
        if (ExprField::Field(scene.toolbox.value, scene.toolbox.valueLen, kToolValueCap,
                "e.g. 100mm or $w * 2", Ui::NameId("ActiveTool::Value"), binding)) {
            tool.SetValue(std::string_view { scene.toolbox.value, scene.toolbox.valueLen });
        }
        // Keep the tool in sync even on frames the text didn't change (e.g. the field
        // was filled before the pick landed).
        tool.SetValue(std::string_view { scene.toolbox.value, scene.toolbox.valueLen });
    }

    if (tool.Ready()) {
        if (UiStyle::Button("Apply", Ui::NameId("ActiveTool::Apply"),
                { .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .border = { 2, 2, 2, 2 }, .borderColor = colors.accentPrimary })) {
            wb.FinishTool();
            scene.toolbox.ClearValue();
        }
    }

    if (UiStyle::Button("Cancel", Ui::NameId("ActiveTool::Cancel"),
            { .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .border = { 2, 2, 2, 2 }, .borderColor = colors.bgDark })) {
        wb.CancelTool();
        scene.toolbox.ClearValue();
    }

    Ui::CloseElement();
}

void UiToolbox(Scene& scene)
{
    const Ui::ColorScheme& colors = Ui::Colors();
    Workbench& wb = scene.workbench;

    Ui::LayoutConfig box {};
    box.sizing = UiStyle::ExpandMinMaxWidth(220, 320);
    box.direction = Ui::Direction::TopToBottom;
    box.align = Ui::AlignCross::Stretch;
    box.padding = UiStyle::PaddingAll(6);
    box.gap = 8;
    box.background = colors.bgBase;
    Ui::OpenElement(box, Ui::NameId("Toolbox"));

    // Undo / redo.
    Ui::LayoutConfig histRow {};
    histRow.direction = Ui::Direction::LeftToRight;
    histRow.sizing = { Ui::Grow(), Ui::Fit() };
    histRow.gap = 6;
    Ui::OpenElement(histRow, Ui::NameId("Toolbox::History"));
    if (UiStyle::Button("Undo", Ui::NameId("Toolbox::Undo"),
            { .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .labelColor = wb.CanUndo() ? colors.textBase : colors.textMuted })) {
        wb.Undo();
    }
    if (UiStyle::Button("Redo", Ui::NameId("Toolbox::Redo"),
            { .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .labelColor = wb.CanRedo() ? colors.textBase : colors.textMuted })) {
        wb.Redo();
    }
    Ui::CloseElement();

    // Explicit save / open. Save writes a durable <exeDir>/<name>.dcad (the auto-save
    // cache under cache/ tracks history automatically); Open reloads that same file.
    Ui::LayoutConfig fileRow {};
    fileRow.direction = Ui::Direction::LeftToRight;
    fileRow.sizing = { Ui::Grow(), Ui::Fit() };
    fileRow.gap = 6;
    Ui::OpenElement(fileRow, Ui::NameId("Toolbox::File"));
    if (UiStyle::Button("Save", Ui::NameId("Toolbox::Save"),
            { .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center })) {
        AppStorage::SaveScene(scene, AppStorage::JoinExe(Serialize::DcadFileName(scene.filename)));
    }
    {
        std::string dcad = AppStorage::JoinExe(Serialize::DcadFileName(scene.filename));
        bool exists = AppStorage::FileExists(dcad);
        if (UiStyle::Button("Open", Ui::NameId("Toolbox::Open"),
                { .sizing = { Ui::Grow(), Ui::Fit() }, .justify = Ui::Justify::Center, .labelColor = exists ? colors.textBase : colors.textMuted })
            && exists) {
            Serialize::SerError err;
            AppStorage::LoadScene(scene, dcad, err);
        }
    }
    Ui::CloseElement();

    // While a gesture is running the toolbox shows ONLY that gesture: its inputs, its
    // value field, Apply/Cancel. Offering the tool list at the same time invites a click
    // that silently abandons half-placed work, and the tool's own options are what the
    // user is actually looking for at that moment.
    if (wb.ActiveTool().Active()) {
        UiActiveToolView(scene);
        Ui::CloseElement(); // Toolbox
        return;
    }

    // THE line that replaces both switches.
    std::vector<ToolId> tools = wb.AvailableTools();

    // Group in kToolTable order so the layout is stable regardless of the order a
    // context happens to list its tools in.
    static std::vector<std::string_view> groups;
    groups.clear();
    for (const ToolInfo& info : kToolTable) {
        bool available = false;
        for (ToolId t : tools) {
            if (t == info.id) {
                available = true;
                break;
            }
        }
        if (!available) {
            continue;
        }
        bool seen = false;
        for (std::string_view g : groups) {
            if (g == info.group) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            groups.push_back(info.group);
        }
    }

    for (std::string_view g : groups) {
        UiToolGroup(g, tools, scene);
    }

    Ui::CloseElement(); // Toolbox
}

// ── Canvas (3D viewport via Ui::Raylib::Canvas3D) ─────────────────────────────
// The 3D scene renders at DISPATCH — Canvas3D composites into the final layout rect
// with no frame lag. All interaction runs here at BUILD time, gated on the viewport
// being hovered (IsHovered, last frame) and ray-picking through canvas.lastRect (the
// canvas sub-viewport, so picking is correct even though the 3D view doesn't fill the
// window). Right/middle mouse + wheel are read raw from raylib (the Ui PointerState
// only tracks the left button).
//
// The canvas is GENERIC over tools: a click feeds Tool::AddPoint / AddPick and the tool
// decides what that means. It does not know what a line is, which is what let the
// hand-written line-chaining state machine go away.
//
// It draws SOLVED geometry, never raw commands. The sketch being authored is replayed +
// solved every frame (Workbench::BuildSketchPreview), so applying a dimension moves the
// geometry immediately rather than at Finish Sketch — and so picking hits the line where
// the user can actually see it.
#include "Gui/DimensionVisual.h" // DimensionVisual + BuildDimensionVisuals
using AppUi::BuildDimensionVisuals;
using AppUi::DIM_OFFSET;
using AppUi::DIM_TEXT_SIZE;
using AppUi::DimensionVisual;

class SceneViewport : public Ui::Raylib::Canvas3D {
public:
    Scene* scene { nullptr };
    bool sketchValid { false };
    bool sketchActive { false };
    std::optional<Geometry::SketchPlane> activePlane {};
    std::optional<Geometry::SketchPlane> hoveredPlane {};
    Geometry::Point2 cursorOnPlane {};

    // Solved geometry for the sketch being authored, rebuilt each frame at build time.
    SketchDocument preview;
    bool hasPreview { false };
    std::vector<DimensionVisual> dims;
    FeatureId highlighted { kNullFeature };

    // The hovered line (idle mode) whose endpoint handles are shown, and whether each
    // endpoint has freedom to be dragged.
    FeatureId hoverLine { kNullFeature };
    bool hoverStartFree { false };
    bool hoverEndFree { false };

    void Draw3D(Ui::Rect) override
    {
        if (!scene) {
            return;
        }

        if (sketchValid && activePlane.has_value()) {
            Geometry::SketchPlane sp = *activePlane;
            UI::DrawGrid(SketchPlaneToOriginPlane(sp), 100, 1.0f);

            if (hasPreview) {
                DrawEntities(preview, sp);
                DrawDimensionLines(sp);
                DrawHoverHandles(sp);
            }
            DrawToolPreview(sp);
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
            for (const SketchDocument& doc : scene->workbench.Evaluated().sketches) {
                DrawEntities(doc, doc.plane);
            }
        }
    }

    // Dimension text. raylib has no 3D text primitive, so the label is projected to the
    // texture's 2D space here, after the 3D pass.
    void Draw2D(Ui::Rect rect) override
    {
        if (!scene || !sketchValid || !activePlane.has_value() || !hasPreview) {
            return;
        }
        Geometry::SketchPlane sp = *activePlane;
        int w = static_cast<int>(rect.w + 0.5f);
        int h = static_cast<int>(rect.h + 0.5f);

        for (const DimensionVisual& d : dims) {
            if (d.label.empty()) {
                continue;
            }

            Vector2 sa = GetWorldToScreenEx(SketchPointToWorld(d.a, sp), camera, w, h);
            Vector2 sb = GetWorldToScreenEx(SketchPointToWorld(d.b, sp), camera, w, h);

            // "if it fits into the dimension line": measure the label against the
            // dimension line's on-screen length and draw only when it actually fits.
            // A dimension line shorter than its own text would otherwise render an
            // unreadable overlap across the geometry it is annotating.
            f32 lineLen = std::sqrt((sb.x - sa.x) * (sb.x - sa.x) + (sb.y - sa.y) * (sb.y - sa.y));
            Vector2 textSize = MeasureTextEx(GetFontDefault(), d.label.c_str(), DIM_TEXT_SIZE, 1.0f);
            if (textSize.x > lineLen) {
                continue;
            }

            Vector2 mid { (sa.x + sb.x) * 0.5f, (sa.y + sb.y) * 0.5f };
            Vector2 pos { mid.x - textSize.x * 0.5f, mid.y - textSize.y * 0.5f };

            // A pad behind the text so it stays legible where it crosses the grid.
            DrawRectangle(static_cast<int>(pos.x) - 2, static_cast<int>(pos.y) - 1,
                static_cast<int>(textSize.x) + 4, static_cast<int>(textSize.y) + 2,
                Color { 255, 255, 255, 220 });
            DrawTextEx(GetFontDefault(), d.label.c_str(), pos, DIM_TEXT_SIZE, 1.0f,
                d.ok ? DIM_COLOR : DIM_ERROR_COLOR);
        }
    }

private:
    static constexpr Color DIM_COLOR { 40, 90, 180, 255 };
    static constexpr Color DIM_ERROR_COLOR { 200, 50, 50, 255 };

    void DrawEntities(const SketchDocument& doc, Geometry::SketchPlane sp) const
    {
        for (const SketchEntity& e : doc.entities) {
            Color tint = e.construction ? GRAY : BLACK;
            if (e.id == highlighted) {
                tint = ORANGE;
            }
            switch (e.kind) {
            case EntityKind::Line:
                DrawLine3D(SketchPointToWorld(e.a, sp), SketchPointToWorld(e.b, sp), tint);
                break;
            case EntityKind::Circle:
                DrawCircleOnPlane(e.a, e.radius, 0.0, 2.0 * Param::kPi, sp, tint);
                break;
            case EntityKind::Arc:
                DrawCircleOnPlane(e.a, e.radius, e.startAngle, e.endAngle, sp, tint);
                break;
            }
        }
    }

    // Extension lines from the entity out to the dimension line, plus the dimension
    // line itself. The label is drawn separately in Draw2D.
    void DrawDimensionLines(Geometry::SketchPlane sp) const
    {
        for (const DimensionVisual& d : dims) {
            Color c = d.ok ? DIM_COLOR : DIM_ERROR_COLOR;
            DrawLine3D(SketchPointToWorld(d.extA, sp), SketchPointToWorld(d.a, sp), c);
            DrawLine3D(SketchPointToWorld(d.extB, sp), SketchPointToWorld(d.b, sp), c);
            DrawLine3D(SketchPointToWorld(d.a, sp), SketchPointToWorld(d.b, sp), c);
            DrawSphereEx(SketchPointToWorld(d.a, sp), 0.06f, 4, 6, c);
            DrawSphereEx(SketchPointToWorld(d.b, sp), 0.06f, 4, 6, c);
        }
    }

    // Endpoint handles for the hovered line (idle mode). A filled dot marks a draggable
    // point; a dimmed hollow-ish dot marks one the constraints have pinned. The line
    // itself is tinted so it's clear what's grabbable.
    void DrawHoverHandles(Geometry::SketchPlane sp) const
    {
        if (hoverLine == kNullFeature) {
            return;
        }
        const SketchEntity* e = preview.Find(hoverLine);
        if (!e || e->kind != EntityKind::Line) {
            return;
        }
        DrawLine3D(SketchPointToWorld(e->a, sp), SketchPointToWorld(e->b, sp), ORANGE);
        DrawHandle(e->a, sp, hoverStartFree);
        DrawHandle(e->b, sp, hoverEndFree);
    }

    static void DrawHandle(Geometry::Point2 p, Geometry::SketchPlane sp, bool free)
    {
        Vector3 w = SketchPointToWorld(p, sp);
        // Free = a solid orange grip; pinned = a smaller grey dot (not draggable).
        Color c = free ? Color { 255, 140, 30, 255 } : Color { 150, 150, 150, 255 };
        DrawSphereEx(w, free ? 0.16f : 0.10f, 8, 8, c);
    }

    // Rubber-band preview: the points placed so far, plus a trail to the cursor.
    void DrawToolPreview(Geometry::SketchPlane sp)
    {
        const Tool& tool = scene->workbench.ActiveTool();
        if (!tool.Active()) {
            return;
        }

        Vector3 cursor = SketchPointToWorld(cursorOnPlane, sp);
        const std::vector<Geometry::Point2>& pts = tool.Points();

        for (const Geometry::Point2& p : pts) {
            DrawSphereEx(SketchPointToWorld(p, sp), 0.1f, 6, 8, BLUE);
        }
        if (!pts.empty() && tool.PointsRemaining() > 0) {
            DrawLine3D(SketchPointToWorld(pts.back(), sp), cursor, GRAY);
        }
        if (tool.PointsRemaining() > 0) {
            DrawSphereEx(cursor, 0.07f, 6, 8, SKYBLUE);
        }
    }

    // raylib has no "circle on an arbitrary sketch plane" primitive; step it manually so
    // circles and arcs render on XY/XZ/YZ alike.
    static void DrawCircleOnPlane(Geometry::Point2 c, f64 r, f64 a0, f64 a1,
        Geometry::SketchPlane sp, Color tint)
    {
        constexpr int kSegments = 48;
        f64 sweep = a1 - a0;
        if (sweep <= 0.0) {
            sweep += 2.0 * Param::kPi; // normalize a backwards arc
        }
        Vector3 prev {};
        for (int i = 0; i <= kSegments; ++i) {
            f64 t = a0 + sweep * (static_cast<f64>(i) / kSegments);
            Geometry::Point2 p { c.x + r * std::cos(t), c.y + r * std::sin(t) };
            Vector3 w = SketchPointToWorld(p, sp);
            if (i > 0) {
                DrawLine3D(prev, w, tint);
            }
            prev = w;
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

// Pick the sketch entity nearest the cursor, for tools that need a selection.
//
// Picks against SOLVED entities, not the raw commands: once a line carries a dimension
// its as-drawn coordinates are not where it is rendered, and picking the raw geometry
// would mean clicking a line and hitting nothing (or hitting a different one).
//
// Distance is in sketch units against a generous threshold — a stand-in until real
// screen-space picking lands, which is what makes the threshold zoom-independent.
std::optional<FeatureId> PickEntityNear(const SketchDocument& doc, Geometry::Point2 at, f64 maxDist)
{
    std::optional<FeatureId> best;
    f64 bestDist = maxDist;

    for (const SketchEntity& e : doc.entities) {
        f64 d = 0;
        if (e.kind == EntityKind::Line) {
            // Point-to-segment distance.
            f64 dx = e.b.x - e.a.x;
            f64 dy = e.b.y - e.a.y;
            f64 lenSq = dx * dx + dy * dy;
            f64 t = lenSq > 0.0 ? ((at.x - e.a.x) * dx + (at.y - e.a.y) * dy) / lenSq : 0.0;
            t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
            f64 px = e.a.x + t * dx;
            f64 py = e.a.y + t * dy;
            d = std::sqrt((at.x - px) * (at.x - px) + (at.y - py) * (at.y - py));
        } else {
            // Circle/arc: distance to the rim, not the centre.
            f64 dc = std::sqrt((at.x - e.a.x) * (at.x - e.a.x) + (at.y - e.a.y) * (at.y - e.a.y));
            d = std::fabs(dc - e.radius);
        }

        if (d < bestDist) {
            bestDist = d;
            best = e.id;
        }
    }

    return best;
}

// The Canvas. Interaction at build time; the 3D render is deferred to dispatch.
//
// Timing note: both the hover gate (viewport.Hovered()) and the pick rect
// (viewport.lastRect) reflect the PREVIOUS frame's layout/hit-test — the standard
// 1-frame lag of this immediate-mode framework. Hovered() is occlusion-aware (a
// floating panel over the canvas correctly suppresses interaction), which a raw rect
// test would not be, so the lag is kept deliberately.
void UiCanvas(Scene& scene)
{
    static SceneViewport viewport;
    g_viewport = &viewport; // let AppShutdown() reach it for GL-live teardown.
    viewport.scene = &scene;

    Workbench& wb = scene.workbench;
    Tool& tool = wb.ActiveTool();

    const SketchContext* sketch = wb.Contexts().ActiveSketch();
    bool is_sketch_active = sketch != nullptr || (tool.Active() && tool.Id() == ToolId::CreateSketch);
    std::optional<Geometry::SketchPlane> active_plane;
    if (sketch) {
        active_plane = sketch->plane;
    }
    bool sketch_valid = active_plane.has_value();
    bool hovered = viewport.Hovered();

    // Replay + solve the in-progress sketch, BEFORE interaction: this frame's picking
    // and this frame's rendering must agree about where the geometry is. Rebuilt every
    // frame, so a dimension (or a parameter edit) shows up immediately without waiting
    // for Finish Sketch.
    viewport.hasPreview = wb.BuildSketchPreview(viewport.preview);

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

    // Plane selection feeds the tool, exactly like a point would.
    scene.hovered_plane = std::nullopt;
    if (!sketch_valid && tool.Active() && tool.Info() && tool.Info()->plane && hovered) {
        scene.hovered_plane = ComputeHoveredOriginPlane(ray, ORIGIN_PLANE_EXTENT);
        if (scene.hovered_plane.has_value() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            tool.SetPlane(*scene.hovered_plane);
            wb.FinishTool(); // CreateSketch needs nothing else
        }
    }

    // Generic tool input. The canvas knows about points and picks; it does NOT know
    // what tool is running or what a line is.
    if (sketch_valid && hovered && tool.Active()) {
        Geometry::Point2 hit = scene.camera.GetMouseOnSketchPlane(*active_plane, ray);
        const ToolInfo* info = tool.Info();

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            wb.CancelTool();
            scene.toolbox.ClearValue();
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && info) {
            if (tool.Points().size() < info->points) {
                tool.AddPoint(hit);
            } else if (tool.Picks().size() < info->picks) {
                // Against solved geometry — where the line actually is on screen.
                if (auto picked = PickEntityNear(viewport.preview, hit, 0.5)) {
                    tool.AddPick(*picked);
                }
            }

            if (tool.Ready()) {
                ToolId finished = tool.Id();
                Geometry::Point2 last = tool.Points().empty() ? hit : tool.Points().back();
                if (wb.FinishTool()) {
                    scene.toolbox.ClearValue();
                    // Chain: a finished Line immediately starts the next one from its
                    // end point, so a polyline is a run of clicks. SetChain marks the new
                    // segment so the commit path auto-adds a Coincident joining it to the
                    // line just placed — the polyline becomes a connected chain, not just
                    // touching points.
                    if (finished == ToolId::Line && wb.StartTool(ToolId::Line)) {
                        wb.ActiveTool().SeedPoint(last);
                        wb.ActiveTool().SetChain(true);
                    }
                }
            }
        }
    }

    // Idle direct-manipulation: with a sketch open and NO drawing tool running, hovering
    // a line shows its endpoint handles and lets the user drag a point or the whole line —
    // but only where the constraints leave freedom. Dragging edits the line's command; the
    // preview re-solves each frame, so a constrained line follows the cursor only as far
    // as it can. Left-drag doesn't collide with the camera (2D pan is the middle button).
    scene.hover_entity.reset();
    if (sketch_valid && !tool.Active()) {
        constexpr f64 kGrab = 0.6; // sketch-space grab radius for an endpoint / the line
        Geometry::Point2 cursor = scene.camera.GetMouseOnSketchPlane(*active_plane, ray);

        if (scene.drag.Active()) {
            // A drag is under way: follow the cursor until the button releases.
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (scene.drag.mode == DragMode::Point) {
                    wb.MoveLinePoint(scene.drag.entity, scene.drag.point, cursor);
                } else if (scene.drag.mode == DragMode::Body) {
                    Geometry::Point2 d { cursor.x - scene.drag.lastCursor.x, cursor.y - scene.drag.lastCursor.y };
                    wb.TranslateLine(scene.drag.entity, d);
                }
                scene.drag.lastCursor = cursor;
                scene.hover_entity = scene.drag.entity;
            } else {
                scene.drag = {}; // released
            }
        } else if (hovered) {
            // Not dragging: reveal the hovered line and, on press, grab a point or body.
            if (auto picked = PickEntityNear(viewport.preview, cursor, kGrab)) {
                const SketchEntity* e = viewport.preview.Find(*picked);
                if (e && e->kind == EntityKind::Line) {
                    scene.hover_entity = *picked;
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        f64 dStart = std::hypot(cursor.x - e->a.x, cursor.y - e->a.y);
                        f64 dEnd = std::hypot(cursor.x - e->b.x, cursor.y - e->b.y);
                        bool startFree = SketchPointFreedom(viewport.preview, *picked, PointRef::Start) > 0;
                        bool endFree = SketchPointFreedom(viewport.preview, *picked, PointRef::End) > 0;
                        // Prefer the nearer endpoint if it's within grab range and free;
                        // otherwise translate the body (needs both ends free).
                        if (dStart <= dEnd && dStart < kGrab && startFree) {
                            scene.drag = { *picked, DragMode::Point, PointRef::Start, cursor };
                        } else if (dEnd < kGrab && endFree) {
                            scene.drag = { *picked, DragMode::Point, PointRef::End, cursor };
                        } else if (startFree && endFree) {
                            scene.drag = { *picked, DragMode::Body, PointRef::Start, cursor };
                        }
                    }
                }
            }
        }
    } else {
        scene.drag = {}; // a tool started or the sketch closed — abandon any drag
    }

    // Cache state for Draw3D/Draw2D (dispatch) and hand the live camera to the viewport.
    viewport.camera = scene.camera.raylib_camera;
    viewport.background = Ui::UiColor { 255, 255, 255, 255 };
    viewport.sketchValid = sketch_valid;
    viewport.sketchActive = is_sketch_active;
    viewport.activePlane = active_plane;
    viewport.hoveredPlane = scene.hovered_plane;
    viewport.cursorOnPlane = sketch_valid ? scene.camera.GetMouseOnSketchPlane(*active_plane, ray) : Geometry::Point2 {};

    // Highlight whatever the tool has already picked, so a selection is visible.
    viewport.highlighted = tool.Active() && !tool.Picks().empty() ? tool.Picks().back() : kNullFeature;

    // The hovered line + its endpoint freedom, so Draw3D can show drag handles (filled
    // where draggable, dimmed where pinned). Computed here at build time (the freedom
    // query needs the parameter table); Draw3D just reads it.
    viewport.hoverLine = scene.hover_entity.value_or(kNullFeature);
    if (viewport.hoverLine != kNullFeature && viewport.hasPreview) {
        viewport.hoverStartFree = SketchPointFreedom(viewport.preview, viewport.hoverLine, PointRef::Start) > 0;
        viewport.hoverEndFree = SketchPointFreedom(viewport.preview, viewport.hoverLine, PointRef::End) > 0;
    }

    // Dimension visuals need the parameter table, which is a build-time concern — Draw2D
    // runs at dispatch and only consumes what is cached here.
    if (viewport.hasPreview) {
        viewport.dims = BuildDimensionVisuals(viewport.preview, wb.Params(), scene.display_unit);
    } else {
        viewport.dims.clear();
    }

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

// ── auto-save ─────────────────────────────────────────────────────────────────
// Seconds of no further change before the cache is written. A burst of edits keeps
// restarting the window, so continuous editing writes nothing until the user pauses —
// coalescing many commits into a single disk write.
constexpr f64 kAutoSaveDebounce { 0.75 };

// Set each frame so AppShutdown (which takes no AppState) can flush pending saves — same
// pattern as g_viewport.
AppState* g_app { nullptr };

// Per-frame auto-save for one scene. Watches the combined (history revision, parameter
// generation) pair — the two counters that together cover every persistable change — and
// writes the cache once edits have been quiet for the debounce.
void AutoSaveTick(Scene& scene)
{
    u32 rev = scene.workbench.Doc().Revision();
    u32 gen = scene.workbench.Params().Generation();

    if (rev != scene.saved_doc_rev || gen != scene.saved_param_gen) {
        scene.saved_doc_rev = rev;
        scene.saved_param_gen = gen;
        scene.save_pending = true;
        scene.pending_since = GetTime(); // (re)start the debounce on every change
    }

    if (scene.save_pending && (GetTime() - scene.pending_since) >= kAutoSaveDebounce) {
        AppStorage::AutoSave(scene);
        scene.save_pending = false;
    }
}

// Write any scene with an unflushed pending change — called on shutdown so a debounce in
// flight at exit is not lost.
void FlushPendingSaves(AppState& app)
{
    for (Scene& scene : app.GetSceneList()) {
        if (scene.save_pending) {
            AppStorage::AutoSave(scene);
            scene.save_pending = false;
        }
    }
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
            AutoSaveTick(*scene);
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
    if (g_app != nullptr) {
        FlushPendingSaves(*g_app); // don't lose a debounce that was in flight at exit
    }
    if (g_viewport != nullptr) {
        g_viewport->Unload();
    }
}

APP_API
void AppUpdate(AppState& app)
{
    ZoneScoped;

    g_app = &app; // let AppShutdown reach the scenes to flush pending saves

    Graphics::BeginFrame();
    BuildUiTree(app);
    Graphics::EndFrame();
}

#ifdef __cplusplus
}
#endif
