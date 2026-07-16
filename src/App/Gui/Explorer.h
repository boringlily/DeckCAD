#pragma once
#include "ExplorerState.h"
#include "ExpressionField.h"
#include "Scene.h"
#include "Ui.h"
#include "UiStyles.h"
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The Explorer panel, in its own header so the headless Ui tests can drive it exactly
// as App.cpp does (see test/Ui/ExplorerTest.cpp). App.cpp is a unity TU and everything
// in it is file-local, which would otherwise put this out of a test's reach.
namespace AppUi {

// ── Explorer ──────────────────────────────────────────────────────────────────
// Two stacked views: a hierarchical command tree, and the ParameterTable.
//
// Both are built from the Ui primitives — the framework has no tree and no grid, so
// indentation is per-depth padding and columns are Fixed/Grow cells in a row. The
// idioms (content-stable NameIds, IsHoverWithin for hover-revealed buttons, deferred
// mutation after the loop, frame-outliving label storage) follow UiLineToolView.

// Row labels drawn as string_views must outlive the frame: Ui nodes store const char* +
// length, not owning strings. Cleared and refilled each frame.
std::vector<std::string>& TreeLabelArena()
{
    static std::vector<std::string> labels;
    return labels;
}

// One tree row: indent, optional expand/collapse chevron, label, hover-revealed delete.
// Returns true if the row's delete was clicked.
bool UiTreeRow(std::string_view label, u32 depth, Ui::UiId rowId, bool selected,
    bool hasChildren, bool collapsed, bool& toggled, bool deletable)
{
    const Ui::ColorScheme& colors = Ui::Colors();
    // Subtree hover, not exact: the row reveals a Del button, and exact hover would let
    // that child steal the row's hover and make the button flicker in and out.
    bool hovered = Ui::IsHoverWithin(rowId);

    Ui::LayoutConfig row {};
    row.direction = Ui::Direction::LeftToRight;
    row.align = Ui::AlignCross::Center;
    row.sizing = { Ui::Grow(), Ui::Fit() };
    row.padding = { 4.0f + depth * 14.0f, 4, 3, 3 }; // left padding == the indent
    row.gap = 4;
    row.cornerRadius = 4;
    row.background = selected ? colors.accentPrimary : (hovered ? colors.bgLight : Ui::UiColor { 0, 0, 0, 0 });
    Ui::OpenElement(row, rowId);

    if (hasChildren) {
        Ui::UiId chevId = Ui::HashChild(rowId, 7);
        if (UiStyle::Button(collapsed ? "+" : "-", chevId,
                { .sizing = { Ui::Fixed(18), Ui::Fixed(18) }, .justify = Ui::Justify::Center, .padding = {}, .cornerRadius = 3 })) {
            toggled = true;
        }
    } else {
        // Keep the label column aligned with its siblings that do have a chevron.
        Ui::LayoutConfig gap {};
        gap.sizing = { Ui::Fixed(18), Ui::Fixed(18) };
        gap.hitTestable = false;
        Ui::OpenElement(gap, Ui::HashChild(rowId, 8));
        Ui::CloseElement();
    }

    UiStyle::DecorText(label, ::FontId::Regular, 15, colors.textBase, Ui::HashChild(rowId, 1));

    bool del = false;
    if (deletable && hovered) {
        Ui::LayoutConfig spacer {};
        spacer.sizing = { Ui::Grow(), Ui::Fit() };
        spacer.hitTestable = false; // let the row keep the click
        Ui::OpenElement(spacer, Ui::HashChild(rowId, 2));
        Ui::CloseElement();

        Ui::UiId delId = Ui::HashChild(rowId, 3);
        if (UiStyle::Button("x", delId,
                { .sizing = { Ui::Fixed(18), Ui::Fixed(18) }, .justify = Ui::Justify::Center, .padding = {}, .cornerRadius = 3 })) {
            del = true;
        }
    }

    Ui::CloseElement(); // row
    return del;
}

// Recursively draw a sketch command subtree. `pending_delete` is filled rather than
// acted on — mutating the vector mid-iteration would invalidate it.
void UiSketchCmdTree(const std::vector<SketchCmd>& cmds, Scene& scene, u32 depth,
    std::optional<FeatureId>& pending_delete, std::optional<FeatureId>& pending_toggle)
{
    for (const SketchCmd& c : cmds) {
        const SketchCmdBase& base = c.Get();
        const CompoundSketchCmd* group = c.As<CompoundSketchCmd>();
        bool hasChildren = group != nullptr && !group->children.empty();
        bool collapsed = scene.explorer.IsCollapsed(base.id);

        std::string& label = TreeLabelArena().emplace_back(base.TypeName());
        if (group) {
            label += " (" + std::to_string(group->children.size()) + ")";
        }

        Ui::UiId rowId = Ui::NameId("ExplorerNode", base.id);
        bool toggled = false;
        if (UiTreeRow(label, depth, rowId, scene.explorer.selected == base.id,
                hasChildren, collapsed, toggled, /*deletable*/ false)) {
            pending_delete = base.id;
        }
        if (toggled) {
            pending_toggle = base.id;
        }
        if (Ui::IsClicked(rowId)) {
            scene.explorer.selected = base.id;
        }

        if (hasChildren && !collapsed) {
            UiSketchCmdTree(group->children, scene, depth + 1, pending_delete, pending_toggle);
        }
    }
}

// The command tree: committed features, plus the sketch currently being authored.
void UiCommandTree(Scene& scene)
{
    Workbench& wb = scene.workbench;
    // The arena is cleared once per panel build by UiExplorer — the tables share it.

    Ui::BeginScrollPanel(Ui::NameId("Explorer::Tree"), 460);

    std::optional<FeatureId> pending_delete;
    std::optional<FeatureId> pending_toggle;

    const std::vector<Command>& history = wb.Doc().History();
    if (history.empty() && wb.Contexts().ActiveSketch() == nullptr) {
        UiStyle::Muted("No features yet.", Ui::NameId("Explorer::Empty"));
    }

    // Committed features. Anything past the undo cursor is history that has been undone:
    // still stored, not applied — show it muted rather than hiding it.
    for (u32 i = 0; i < history.size(); ++i) {
        const PartCmdBase& base = history[i].Get();
        const SketchFeatureCommand* sf = history[i].As<SketchFeatureCommand>();
        bool undone = i >= wb.Doc().Cursor();
        bool hasChildren = sf != nullptr && !sf->children.empty();
        bool collapsed = scene.explorer.IsCollapsed(base.id);

        std::string& label = TreeLabelArena().emplace_back(base.TypeName());
        label += " " + std::to_string(i + 1);
        if (undone) {
            label += "  (undone)";
        }

        Ui::UiId rowId = Ui::NameId("ExplorerFeature", base.id);
        bool toggled = false;
        if (UiTreeRow(label, 0, rowId, scene.explorer.selected == base.id,
                hasChildren, collapsed, toggled, /*deletable*/ true)) {
            pending_delete = base.id;
        }
        if (toggled) {
            pending_toggle = base.id;
        }
        if (Ui::IsClicked(rowId)) {
            scene.explorer.selected = base.id;
        }

        if (sf && hasChildren && !collapsed) {
            UiSketchCmdTree(sf->children, scene, 1, pending_delete, pending_toggle);
        }
    }

    // The in-progress sketch. It is NOT in document history yet — a provisional context
    // commits nothing until confirm — so it is drawn from the context itself.
    if (const SketchContext* sketch = wb.Contexts().ActiveSketch()) {
        std::string& label = TreeLabelArena().emplace_back("Sketch (editing)");
        Ui::UiId rowId = Ui::NameId("ExplorerActiveSketch");
        bool toggled = false;
        bool collapsed = scene.explorer.IsCollapsed(sketch->featureId);
        UiTreeRow(label, 0, rowId, false, !sketch->children.empty(), collapsed, toggled, false);
        if (toggled) {
            pending_toggle = sketch->featureId;
        }
        if (!collapsed) {
            UiSketchCmdTree(sketch->children, scene, 1, pending_delete, pending_toggle);
        }
    }

    Ui::EndScrollPanel();

    // Deferred: applied only once the tree is fully built.
    if (pending_toggle.has_value()) {
        scene.explorer.ToggleCollapsed(*pending_toggle);
    }
    if (pending_delete.has_value()) {
        wb.DeleteFeature(*pending_delete);
    }
}

// ── ParameterTable ────────────────────────────────────────────────────────────
// Name | Expression | Value | Unit. The expression cell is a real parser-backed field,
// so errors highlight inline as you type.

void UiParamHeader()
{
    const Ui::ColorScheme& colors = Ui::Colors();
    Ui::LayoutConfig row {};
    row.direction = Ui::Direction::LeftToRight;
    row.sizing = { Ui::Grow(), Ui::Fit() };
    row.padding = { 4, 4, 2, 2 };
    row.gap = 4;
    Ui::OpenElement(row, Ui::NameId("ParamTable::Header"));

    auto cell = [&](std::string_view t, Ui::Sizing s, u32 k) {
        Ui::LayoutConfig c {};
        c.sizing = s;
        c.hitTestable = false;
        Ui::OpenElement(c, Ui::NameId("ParamTable::H", k));
        UiStyle::DecorText(t, ::FontId::Semibold, 13, colors.textMuted);
        Ui::CloseElement();
    };
    cell("Name", { Ui::Percent(0.26f), Ui::Fit() }, 0);
    cell("Expression", { Ui::Grow(), Ui::Fit() }, 1);
    cell("Value", { Ui::Fixed(72), Ui::Fit() }, 2);
    cell("Unit", { Ui::Fixed(34), Ui::Fit() }, 3);

    Ui::CloseElement();
}

void UiParameterTable(Scene& scene)
{
    Workbench& wb = scene.workbench;
    Param::ParameterEngine& params = wb.Params();
    scene.explorer.SyncRows(params);

    // Rebound each frame: the binding is a non-owning view, and Workbench may have moved.
    static ExprField::ParserBinding binding {};
    binding.engine = &params;

    UiParamHeader();

    Ui::BeginScrollPanel(Ui::NameId("Explorer::Params"), 420);

    if (scene.explorer.rows.empty()) {
        UiStyle::Muted("No parameters.", Ui::NameId("Explorer::NoParams"));
    }

    std::optional<Param::UPID> pending_remove;

    for (ParamRow& row : scene.explorer.rows) {
        const Param::ParametricExpression* p = params.Get(row.id);
        if (!p) {
            continue;
        }

        Ui::UiId rowId = Ui::NameId("ParamRow", row.id);
        bool hovered = Ui::IsHoverWithin(rowId);

        Ui::LayoutConfig r {};
        r.direction = Ui::Direction::LeftToRight;
        r.align = Ui::AlignCross::Center;
        r.sizing = { Ui::Grow(), Ui::Fit() };
        r.padding = { 2, 2, 2, 2 };
        r.gap = 4;
        r.cornerRadius = 3;
        r.background = hovered ? Ui::Colors().bgBase : Ui::UiColor { 0, 0, 0, 0 };
        Ui::OpenElement(r, rowId);

        // Name — renaming can fail (a taken name); reject by restoring the stored name
        // rather than letting the field drift out of sync with the engine.
        Ui::LayoutConfig nameCell {};
        nameCell.sizing = { Ui::Percent(0.26f), Ui::Fit() };
        Ui::OpenElement(nameCell, Ui::HashChild(rowId, 1));
        if (Ui::InputLabel(row.name, row.nameLen, kParamNameCap, "name", Ui::HashChild(rowId, 2))) {
            if (!params.Rename(row.id, std::string_view { row.name, row.nameLen })) {
                SetBuf(row.name, row.nameLen, kParamNameCap, p->Name());
            }
        }
        Ui::CloseElement();

        // Expression — parser-backed: highlights and reports errors as you type.
        Ui::LayoutConfig exprCell {};
        exprCell.sizing = { Ui::Grow(), Ui::Fit() };
        Ui::OpenElement(exprCell, Ui::HashChild(rowId, 3));
        if (ExprField::Field(row.expr, row.exprLen, kParamExprCap, "expression",
                Ui::HashChild(rowId, 4), binding)) {
            params.SetExpression(row.id, std::string_view { row.expr, row.exprLen });
        }
        Ui::CloseElement();

        // Value + unit, resolved live.
        Param::EvalResult ev = params.Value(row.id);
        Param::Unit display = p->DisplayUnit() != Param::Unit::None
            ? p->DisplayUnit()
            : (ev.Ok() ? Param::BaseUnitOf(ev.value.kind) : Param::Unit::None);

        std::string& valueText = TreeLabelArena().emplace_back();
        valueText = ev.Ok() ? Param::FormatValue(Param::Display(ev.value, display)) : "—";

        Ui::LayoutConfig valCell {};
        valCell.sizing = { Ui::Fixed(72), Ui::Fit() };
        valCell.hitTestable = false;
        Ui::OpenElement(valCell, Ui::HashChild(rowId, 5));
        UiStyle::DecorText(valueText, ::FontId::Regular, 14,
            ev.Ok() ? Ui::Colors().textBase : Ui::UiColor { 220, 64, 64, 255 });
        Ui::CloseElement();

        Ui::LayoutConfig unitCell {};
        unitCell.sizing = { Ui::Fixed(34), Ui::Fit() };
        unitCell.hitTestable = false;
        Ui::OpenElement(unitCell, Ui::HashChild(rowId, 6));
        UiStyle::DecorText(Param::UnitSuffix(display), ::FontId::Regular, 13, Ui::Colors().textMuted);
        Ui::CloseElement();

        if (hovered) {
            Ui::UiId delId = Ui::HashChild(rowId, 9);
            if (UiStyle::Button("x", delId,
                    { .sizing = { Ui::Fixed(18), Ui::Fixed(18) }, .justify = Ui::Justify::Center, .padding = {}, .cornerRadius = 3 })) {
                pending_remove = row.id;
            }
        }

        Ui::CloseElement(); // row
    }

    Ui::EndScrollPanel();

    // Add-parameter row.
    Ui::LayoutConfig add {};
    add.direction = Ui::Direction::LeftToRight;
    add.align = Ui::AlignCross::Center;
    add.sizing = { Ui::Grow(), Ui::Fit() };
    add.padding = { 2, 2, 4, 2 };
    add.gap = 4;
    Ui::OpenElement(add, Ui::NameId("ParamAddRow"));

    Ui::LayoutConfig nameCell {};
    nameCell.sizing = { Ui::Percent(0.26f), Ui::Fit() };
    Ui::OpenElement(nameCell, Ui::NameId("ParamAdd::NameCell"));
    Ui::InputLabel(scene.explorer.newName, scene.explorer.newNameLen, kParamNameCap,
        "name", Ui::NameId("ParamAdd::Name"));
    Ui::CloseElement();

    Ui::LayoutConfig exprCell {};
    exprCell.sizing = { Ui::Grow(), Ui::Fit() };
    Ui::OpenElement(exprCell, Ui::NameId("ParamAdd::ExprCell"));
    ExprField::Field(scene.explorer.newExpr, scene.explorer.newExprLen, kParamExprCap,
        "expression", Ui::NameId("ParamAdd::Expr"), binding);
    Ui::CloseElement();

    bool named = scene.explorer.newNameLen > 0 && scene.explorer.newExprLen > 0;
    if (UiStyle::Button("Add", Ui::NameId("ParamAdd::Button"),
            { .sizing = { Ui::Fixed(48), Ui::Fit() }, .justify = Ui::Justify::Center })
        && named) {
        Param::UPID created = params.Create(
            std::string_view { scene.explorer.newName, scene.explorer.newNameLen },
            std::string_view { scene.explorer.newExpr, scene.explorer.newExprLen });
        if (created != Param::kNullUpid) {
            scene.explorer.ClearNewRow();
        }
        // A duplicate name returns kNullUpid; keep the text so it can be corrected.
    }

    Ui::CloseElement(); // ParamAddRow

    if (pending_remove.has_value()) {
        params.Remove(*pending_remove);
    }
}

// ── Geometry table ────────────────────────────────────────────────────────────
// The generated geometry: what the command list actually produced once replayed and
// solved. This is the OUTPUT view — read-only by construction, because editing here
// would be editing a cache that the next recompute throws away.

std::string_view EntityKindName(EntityKind k)
{
    switch (k) {
    case EntityKind::Line:
        return "Line";
    case EntityKind::Arc:
        return "Arc";
    case EntityKind::Circle:
        return "Circle";
    }
    return "?";
}

// The solver's verdict, as a short label + a colour. Green = pinned down, blue = still
// has freedom, orange = redundant/conflicting, red = the solve failed. Standard CAD cue.
struct SolveBadge {
    std::string_view text;
    Ui::UiColor color;
};

SolveBadge SolveStatusBadge(SketchSolveStatus s)
{
    switch (s) {
    case SketchSolveStatus::WellConstrained:
        return { "fully constrained", Ui::UiColor { 90, 190, 110, 255 } };
    case SketchSolveStatus::UnderConstrained:
        return { "under-constrained", Ui::UiColor { 90, 150, 220, 255 } };
    case SketchSolveStatus::OverConstrained:
        return { "over-constrained", Ui::UiColor { 220, 150, 60, 255 } };
    case SketchSolveStatus::DidNotConverge:
        return { "unsolved", Ui::UiColor { 220, 70, 70, 255 } };
    }
    return { "", Ui::Colors().textMuted };
}

void UiGeometryHeader()
{
    const Ui::ColorScheme& colors = Ui::Colors();
    Ui::LayoutConfig row {};
    row.direction = Ui::Direction::LeftToRight;
    row.sizing = { Ui::Grow(), Ui::Fit() };
    row.padding = { 4, 4, 2, 2 };
    row.gap = 4;
    Ui::OpenElement(row, Ui::NameId("GeomTable::Header"));

    auto cell = [&](std::string_view t, Ui::Sizing s, u32 k) {
        Ui::LayoutConfig c {};
        c.sizing = s;
        c.hitTestable = false;
        Ui::OpenElement(c, Ui::NameId("GeomTable::H", k));
        UiStyle::DecorText(t, ::FontId::Semibold, 13, colors.textMuted);
        Ui::CloseElement();
    };
    cell("Entity", { Ui::Grow(), Ui::Fit() }, 0);
    cell("Size", { Ui::Fixed(78), Ui::Fit() }, 1);
    cell("Driven", { Ui::Fixed(52), Ui::Fit() }, 2);

    Ui::CloseElement();
}

void UiGeometryTable(Scene& scene)
{
    Workbench& wb = scene.workbench;
    const Ui::ColorScheme& colors = Ui::Colors();

    // The evaluated model, plus the sketch being authored (which has no committed form
    // yet but is exactly what the user is looking at in the canvas).
    const PartDocument& part = wb.Evaluated();
    SketchDocument live;
    bool hasLive = wb.BuildSketchPreview(live);

    UiGeometryHeader();
    Ui::BeginScrollPanel(Ui::NameId("Explorer::Geometry"), 460);

    u32 total = 0;
    for (const SketchDocument& doc : part.sketches) {
        total += static_cast<u32>(doc.entities.size());
    }
    if (hasLive) {
        total += static_cast<u32>(live.entities.size());
    }
    if (total == 0) {
        UiStyle::Muted("No geometry generated.", Ui::NameId("Explorer::NoGeom"));
    }

    auto drawSketch = [&](const SketchDocument& doc, std::string_view title, u32 salt) {
        std::string& header = TreeLabelArena().emplace_back(title);
        header += "  (" + std::to_string(doc.entities.size()) + ")";
        Ui::LayoutConfig h {};
        h.direction = Ui::Direction::LeftToRight;
        h.align = Ui::AlignCross::Center;
        h.sizing = { Ui::Grow(), Ui::Fit() };
        h.padding = { 2, 2, 3, 1 };
        h.gap = 6;
        h.hitTestable = false;
        Ui::OpenElement(h, Ui::NameId("GeomSketch", salt));
        UiStyle::DecorText(header, ::FontId::Semibold, 13, colors.textMuted);
        // The solve verdict, when a constraint solve actually ran (the direct fast path
        // reports the neutral default, which we don't badge).
        if (!doc.constraints.empty()) {
            SolveBadge badge = SolveStatusBadge(doc.lastSolve.status);
            UiStyle::DecorText(badge.text, ::FontId::MediumItalic, 12, badge.color,
                Ui::NameId("GeomStatus", salt));
        }
        Ui::CloseElement();

        for (const SketchEntity& e : doc.entities) {
            Ui::UiId rowId = Ui::NameId("GeomRow", e.id);

            Ui::LayoutConfig r {};
            r.direction = Ui::Direction::LeftToRight;
            r.align = Ui::AlignCross::Center;
            r.sizing = { Ui::Grow(), Ui::Fit() };
            r.padding = { 10, 2, 2, 2 };
            r.gap = 4;
            r.cornerRadius = 3;
            bool selected = scene.explorer.selected == e.id;
            r.background = selected ? colors.accentPrimary
                                    : (Ui::IsHoverWithin(rowId) ? colors.bgLight : Ui::UiColor { 0, 0, 0, 0 });
            Ui::OpenElement(r, rowId);

            std::string& name = TreeLabelArena().emplace_back(EntityKindName(e.kind));
            name += " #" + std::to_string(e.id);
            if (e.construction) {
                name += " (construction)";
            }
            Ui::LayoutConfig nameCell {};
            nameCell.sizing = { Ui::Grow(), Ui::Fit() };
            nameCell.hitTestable = false;
            Ui::OpenElement(nameCell, Ui::HashChild(rowId, 1));
            UiStyle::DecorText(name, ::FontId::Regular, 14, colors.textBase);
            Ui::CloseElement();

            // The measured size. Every entity can be measured; whether anything DICTATES
            // it is the separate "Driven" column — that distinction is the whole point.
            std::string& size = TreeLabelArena().emplace_back();
            {
                DTL::Optional<f64> m = e.kind == EntityKind::Line ? doc.MeasureLength(e.id)
                                                                  : doc.MeasureRadius(e.id);
                size = m.has_value()
                    ? Param::FormatQuantity({ *m, Param::QuantityKind::Length }, scene.display_unit)
                    : "—";
            }
            Ui::LayoutConfig sizeCell {};
            sizeCell.sizing = { Ui::Fixed(78), Ui::Fit() };
            sizeCell.hitTestable = false;
            Ui::OpenElement(sizeCell, Ui::HashChild(rowId, 2));
            UiStyle::DecorText(size, ::FontId::Regular, 14, colors.textBase);
            Ui::CloseElement();

            bool driven = doc.IsDriven(e.id);
            Ui::LayoutConfig drvCell {};
            drvCell.sizing = { Ui::Fixed(52), Ui::Fit() };
            drvCell.hitTestable = false;
            Ui::OpenElement(drvCell, Ui::HashChild(rowId, 3));
            UiStyle::DecorText(driven ? "yes" : "free", ::FontId::Regular, 13,
                driven ? colors.textBase : colors.textMuted);
            Ui::CloseElement();

            Ui::CloseElement(); // row

            if (Ui::IsClicked(rowId)) {
                scene.explorer.selected = e.id;
            }
        }
    };

    u32 salt = 0;
    for (const SketchDocument& doc : part.sketches) {
        std::string& t = TreeLabelArena().emplace_back("Sketch #");
        t += std::to_string(doc.id);
        drawSketch(doc, t, salt++);
    }
    if (hasLive) {
        drawSketch(live, "Sketch (editing)", 9999);
    }

    Ui::EndScrollPanel();
}

// ── the panel ─────────────────────────────────────────────────────────────────

void UiExplorerTabs(Scene& scene)
{
    const Ui::ColorScheme& colors = Ui::Colors();

    Ui::LayoutConfig tabs {};
    tabs.direction = Ui::Direction::LeftToRight;
    tabs.sizing = { Ui::Grow(), Ui::Fit() };
    tabs.gap = 4;
    Ui::OpenElement(tabs, Ui::NameId("Explorer::Tabs"));

    for (const ExplorerTabInfo& t : kExplorerTabs) {
        bool active = scene.explorer.tab == t.tab;
        UiStyle::ButtonStyle st {};
        st.sizing = { Ui::Grow(), Ui::Fit() };
        st.justify = Ui::Justify::Center;
        st.padding = { 6, 6, 4, 4 };
        st.active = active;
        st.border = active ? Ui::Edges { 2, 2, 2, 2 } : Ui::Edges {};
        st.borderColor = colors.accentPrimary;
        st.labelColor = active ? colors.textBase : colors.textMuted;
        if (UiStyle::Button(t.name, Ui::NameId("Explorer::Tab", static_cast<u32>(t.tab)), st)) {
            scene.explorer.tab = t.tab;
        }
    }

    Ui::CloseElement();
}

// The Explorer panel: a tab bar over three independent tables.
void UiExplorer(Scene& scene)
{
    Ui::LayoutConfig c {};
    c.sizing = UiStyle::ExpandMinMaxWidth(240, 380);
    c.padding = UiStyle::PaddingAll(6);
    c.gap = 8;
    c.direction = Ui::Direction::TopToBottom;
    c.align = Ui::AlignCross::Stretch;
    c.background = Ui::Colors().bgBase;
    Ui::OpenElement(c, Ui::NameId("WorkbenchExplorer"));

    // One label arena per panel build, shared by whichever table is showing — Ui nodes
    // hold const char* + length, so every label must outlive the frame.
    TreeLabelArena().clear();

    UiExplorerTabs(scene);

    switch (scene.explorer.tab) {
    case ExplorerTab::Model:
        UiCommandTree(scene);
        break;
    case ExplorerTab::Parameters:
        UiParameterTable(scene);
        break;
    case ExplorerTab::Geometry:
        UiGeometryTable(scene);
        break;
    }

    Ui::CloseElement();
}
} // namespace AppUi
