#pragma once
#include "AppState.h"
#include "ExplorerState.h"
#include "ExpressionField.h"
#include "Scene.h"
#include "Ui.h"
#include "UiStyles.h"
#include <cstdio>
#include <cstdlib>
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

// What a tree row's hover-revealed chips reported this frame.
struct RowActions {
    bool del { false };
    bool edit { false };
};

// One tree row: indent, optional expand/collapse chevron, label, and hover-revealed chips
// (Edit, then Delete). Reports which chip, if any, was clicked.
RowActions UiTreeRow(std::string_view label, u32 depth, Ui::UiId rowId, bool selected,
    bool hasChildren, bool collapsed, bool& toggled, bool deletable, bool editable)
{
    const Ui::ColorScheme& colors = Ui::Colors();
    // Subtree hover, not exact: the row reveals chips, and exact hover would let a chip
    // steal the row's hover and make the chips flicker in and out.
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

    RowActions actions {};
    if ((deletable || editable) && hovered) {
        Ui::LayoutConfig spacer {};
        spacer.sizing = { Ui::Grow(), Ui::Fit() };
        spacer.hitTestable = false; // let the row keep the click
        Ui::OpenElement(spacer, Ui::HashChild(rowId, 2));
        Ui::CloseElement();

        // Edit chip — re-enters the finished sketch to edit it.
        if (editable) {
            Ui::UiId editId = Ui::HashChild(rowId, 4);
            if (UiStyle::Button("edit", editId,
                    { .sizing = { Ui::Fit(), Ui::Fixed(18) }, .justify = Ui::Justify::Center, .padding = { 6, 6, 0, 0 }, .cornerRadius = 3 })) {
                actions.edit = true;
            }
        }

        // Delete chip.
        if (deletable) {
            Ui::UiId delId = Ui::HashChild(rowId, 3);
            if (UiStyle::Button("x", delId,
                    { .sizing = { Ui::Fixed(18), Ui::Fixed(18) }, .justify = Ui::Justify::Center, .padding = {}, .cornerRadius = 3 })) {
                actions.del = true;
            }
        }
    }

    Ui::CloseElement(); // row
    return actions;
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
                hasChildren, collapsed, toggled, /*deletable*/ false, /*editable*/ false)
                .del) {
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
    std::optional<FeatureId> pending_edit;

    // Editing re-enters a committed sketch; it is only offered when nothing else is being
    // authored (a provisional edit context can only be pushed from Root).
    bool canEditNow = wb.Contexts().ActiveSketch() == nullptr;

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
        // Only sketches can be re-edited, and not while they're undone (the cursor is behind
        // them) or while another edit is open.
        bool editable = sf != nullptr && !undone && canEditNow;
        RowActions act = UiTreeRow(label, 0, rowId, scene.explorer.selected == base.id,
            hasChildren, collapsed, toggled, /*deletable*/ true, editable);
        if (act.del) {
            pending_delete = base.id;
        }
        if (act.edit) {
            pending_edit = base.id;
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
        UiTreeRow(label, 0, rowId, false, !sketch->children.empty(), collapsed, toggled,
            /*deletable*/ false, /*editable*/ false);
        if (toggled) {
            pending_toggle = sketch->featureId;
        }
        if (!collapsed) {
            UiSketchCmdTree(sketch->children, scene, 1, pending_delete, pending_toggle);
        }
    }

    Ui::EndScrollPanel();

    // Deferred: applied only once the tree is fully built. Edit takes precedence over
    // delete (a single click can only trigger one, but keep the intent explicit).
    if (pending_toggle.has_value()) {
        scene.explorer.ToggleCollapsed(*pending_toggle);
    }
    if (pending_edit.has_value()) {
        wb.EditFeature(*pending_edit);
    } else if (pending_delete.has_value()) {
        wb.DeleteFeature(*pending_delete);
    }
}

// ── feature properties (the selected sketch command) ──────────────────────────
// While a sketch is open for editing, selecting a feature in the tree shows its editable
// parameters here — a point's coordinates, a dimension's expression, a curve's construction
// flag. This is what makes a finished sketch's individual features editable, not just the
// sketch as a whole. Points hold the coordinates, so numeric editing lives on point rows.
void UiFeatureProperties(Scene& scene)
{
    Workbench& wb = scene.workbench;
    const Ui::ColorScheme& colors = Ui::Colors();

    // Only meaningful while a sketch is being authored/edited (its commands are live in the
    // context). Nothing to inspect otherwise.
    if (wb.Contexts().ActiveSketch() == nullptr) {
        return;
    }

    Ui::LayoutConfig c {};
    c.direction = Ui::Direction::TopToBottom;
    c.sizing = { Ui::Grow(), Ui::Fit() };
    c.padding = UiStyle::PaddingAll(6);
    c.gap = 6;
    c.cornerRadius = 4;
    c.background = colors.bgDark;
    Ui::OpenElement(c, Ui::NameId("Explorer::Properties"));

    UiStyle::Muted("Properties", Ui::NameId("Properties::Title"));

    ExplorerState& es = scene.explorer;
    FeatureId sel = es.selected;
    const SketchCmd* cmd = sel != kNullFeature ? wb.FindActiveSketchCmd(sel) : nullptr;
    if (!cmd) {
        UiStyle::Muted("Select a feature to edit its parameters.", Ui::NameId("Properties::Empty"));
        Ui::CloseElement();
        return;
    }

    // Re-seed the edit buffers when the selection changes — never mid-typing, so the field
    // pushes to the model, not the other way round (same rule as the parameter table).
    if (es.propFor != sel) {
        es.propFor = sel;
        es.propXLen = es.propYLen = es.propExprLen = 0;
        es.propX[0] = es.propY[0] = es.propExpr[0] = '\0';
        if (const SketchPointCmd* p = cmd->As<SketchPointCmd>()) {
            char b[ExplorerState::kPropNumCap];
            int n = std::snprintf(b, sizeof b, "%g", p->initial.x);
            SetBuf(es.propX, es.propXLen, ExplorerState::kPropNumCap,
                std::string_view { b, static_cast<size_t>(n > 0 ? n : 0) });
            n = std::snprintf(b, sizeof b, "%g", p->initial.y);
            SetBuf(es.propY, es.propYLen, ExplorerState::kPropNumCap,
                std::string_view { b, static_cast<size_t>(n > 0 ? n : 0) });
        } else if (const SketchDimensionCmd* d = cmd->As<SketchDimensionCmd>()) {
            if (const Param::ParametricExpression* pe = wb.Params().Get(d->value)) {
                SetBuf(es.propExpr, es.propExprLen, kParamExprCap, pe->Text());
            }
        }
    }

    std::string& title = TreeLabelArena().emplace_back(cmd->Get().TypeName());
    title += " #" + std::to_string(sel);
    UiStyle::DecorText(title, ::FontId::Semibold, 14, colors.textBase, Ui::NameId("Properties::Type"));

    // A labelled cell + input row. Returns true on commit.
    auto labelledInput = [&](std::string_view lbl, char* buf, u32& len, u32 cap, Ui::UiId id) -> bool {
        Ui::LayoutConfig r {};
        r.direction = Ui::Direction::LeftToRight;
        r.align = Ui::AlignCross::Center;
        r.sizing = { Ui::Grow(), Ui::Fit() };
        r.gap = 6;
        Ui::OpenElement(r, Ui::HashChild(id, 90));

        Ui::LayoutConfig lc {};
        lc.sizing = { Ui::Fixed(58), Ui::Fit() };
        lc.hitTestable = false;
        Ui::OpenElement(lc, Ui::HashChild(id, 91));
        UiStyle::DecorText(lbl, ::FontId::Regular, 14, colors.textMuted);
        Ui::CloseElement();

        Ui::LayoutConfig ic {};
        ic.sizing = { Ui::Grow(), Ui::Fit() };
        Ui::OpenElement(ic, Ui::HashChild(id, 92));
        bool changed = Ui::InputLabel(buf, len, cap, lbl, id);
        Ui::CloseElement();

        Ui::CloseElement();
        return changed;
    };

    if (const SketchPointCmd* p = cmd->As<SketchPointCmd>()) {
        if (labelledInput("X", es.propX, es.propXLen, ExplorerState::kPropNumCap, Ui::NameId("Properties::X"))) {
            wb.SetSketchPointInitial(sel, { std::strtod(es.propX, nullptr), p->initial.y });
        }
        if (labelledInput("Y", es.propY, es.propYLen, ExplorerState::kPropNumCap, Ui::NameId("Properties::Y"))) {
            wb.SetSketchPointInitial(sel, { p->initial.x, std::strtod(es.propY, nullptr) });
        }
    } else if (const SketchDimensionCmd* d = cmd->As<SketchDimensionCmd>()) {
        static ExprField::ParserBinding binding {};
        binding.engine = &wb.Params();

        Ui::LayoutConfig r {};
        r.direction = Ui::Direction::LeftToRight;
        r.align = Ui::AlignCross::Center;
        r.sizing = { Ui::Grow(), Ui::Fit() };
        r.gap = 6;
        Ui::OpenElement(r, Ui::NameId("Properties::ValueRow"));

        Ui::LayoutConfig lc {};
        lc.sizing = { Ui::Fixed(58), Ui::Fit() };
        lc.hitTestable = false;
        Ui::OpenElement(lc, Ui::NameId("Properties::ValueLabel"));
        UiStyle::DecorText("Value", ::FontId::Regular, 14, colors.textMuted);
        Ui::CloseElement();

        Ui::LayoutConfig ic {};
        ic.sizing = { Ui::Grow(), Ui::Fit() };
        Ui::OpenElement(ic, Ui::NameId("Properties::ValueCell"));
        if (ExprField::Field(es.propExpr, es.propExprLen, kParamExprCap, "expression",
                Ui::NameId("Properties::Expr"), binding)) {
            wb.Params().SetExpression(d->value, std::string_view { es.propExpr, es.propExprLen });
        }
        Ui::CloseElement();

        Ui::CloseElement();
    } else if (cmd->Is<SketchLine>() || cmd->Is<SketchCircle>() || cmd->Is<SketchArc>()) {
        bool construction = false;
        if (const SketchLine* l = cmd->As<SketchLine>()) {
            construction = l->construction;
        } else if (const SketchCircle* ci = cmd->As<SketchCircle>()) {
            construction = ci->construction;
        } else if (const SketchArc* a = cmd->As<SketchArc>()) {
            construction = a->construction;
        }
        if (Ui::Checkbox(construction, "Construction", Ui::NameId("Properties::Construction"))) {
            wb.SetSketchConstruction(sel, construction);
        }
    } else {
        UiStyle::Muted("No editable parameters.", Ui::NameId("Properties::None"));
    }

    Ui::CloseElement(); // Explorer::Properties
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

// ── project settings ─────────────────────────────────────────────────────────
// Scene-level settings: the scene's display name and its default unit — what a bare
// number in an expression means, and what dimensions display in (Scene::display_unit).

inline constexpr Param::Unit kProjectSettingsUnits[] = {
    Param::Unit::Millimeter,
    Param::Unit::Centimeter,
    Param::Unit::Meter,
    Param::Unit::Inch,
    Param::Unit::Foot,
};

void UiProjectSettingsPanel(Scene& scene)
{
    const Ui::ColorScheme& colors = Ui::Colors();

    if (!scene.explorer.settingsSeeded) {
        SetBuf(scene.explorer.settingsName, scene.explorer.settingsNameLen, kParamNameCap, scene.filename);
        scene.explorer.settingsSeeded = true;
    }

    Ui::LayoutConfig c {};
    c.sizing = UiStyle::ExpandMinMaxWidth(240, 380);
    c.padding = UiStyle::PaddingAll(12);
    c.gap = 12;
    c.direction = Ui::Direction::TopToBottom;
    c.align = Ui::AlignCross::Stretch;
    c.background = colors.bgBase;
    Ui::OpenElement(c, Ui::NameId("ProjectSettings"));

    UiStyle::Subtitle("Project Settings", Ui::NameId("ProjectSettings::Title"));

    UiStyle::Muted("Name", Ui::NameId("ProjectSettings::NameLabel"));
    if (Ui::InputLabel(scene.explorer.settingsName, scene.explorer.settingsNameLen, kParamNameCap,
            "Scene name", Ui::NameId("ProjectSettings::Name"))) {
        scene.filename.assign(scene.explorer.settingsName, scene.explorer.settingsNameLen);
    }

    UiStyle::Muted("Default Unit", Ui::NameId("ProjectSettings::UnitLabel"));
    Ui::LayoutConfig unitRow {};
    unitRow.direction = Ui::Direction::LeftToRight;
    unitRow.sizing = { Ui::Grow(), Ui::Fit() };
    unitRow.gap = 4;
    Ui::OpenElement(unitRow, Ui::NameId("ProjectSettings::Units"));
    for (Param::Unit u : kProjectSettingsUnits) {
        bool active = scene.display_unit == u;
        UiStyle::ButtonStyle st {};
        st.sizing = { Ui::Grow(), Ui::Fit() };
        st.justify = Ui::Justify::Center;
        st.active = active;
        st.labelColor = active ? colors.textBase : colors.textMuted;
        if (UiStyle::Button(Param::UnitSuffix(u), Ui::NameId("ProjectSettings::Unit", static_cast<u32>(u)), st)) {
            scene.display_unit = u;
        }
    }
    Ui::CloseElement();

    Ui::CloseElement(); // ProjectSettings
}

// ── the sidebar rail ─────────────────────────────────────────────────────────
// A narrow icon column, always to the left of whichever panel is showing: jump to the
// Explorer tree, straight to the Parameter table, or this scene's Project Settings.
// App-wide Settings is pinned to the bottom, separate from the per-scene sections above
// it.

void UiSidebarRail(Scene& scene, AppState& app)
{
    const Ui::ColorScheme& colors = Ui::Colors();

    Ui::LayoutConfig col {};
    col.direction = Ui::Direction::TopToBottom;
    col.sizing = { Ui::Fixed(48), Ui::Grow() };
    col.padding = UiStyle::PaddingAll(6);
    col.gap = 8;
    col.align = Ui::AlignCross::Center;
    col.background = colors.bgDark;
    Ui::OpenElement(col, Ui::NameId("SidebarRail"));

    auto railButton = [&](IconId icon, SidebarSection section, Ui::UiId id) {
        bool active = scene.explorer.section == section;
        if (UiStyle::Button("", id,
                { .icon = static_cast<int>(icon), .sizing = { Ui::Fixed(36), Ui::Fixed(36) }, .justify = Ui::Justify::Center, .active = active, .bg = colors.bgDark })) {
            scene.explorer.section = section;
            // Parameters is a rail-only view carried by `tab`. Entering it selects it;
            // leaving it (to the tabbed Explorer) must fall back to a real tab-bar view,
            // or the tabless Parameters table would linger under the Explorer button.
            if (section == SidebarSection::Parameters) {
                scene.explorer.tab = ExplorerTab::Parameters;
            } else if (section == SidebarSection::Explorer
                && scene.explorer.tab == ExplorerTab::Parameters) {
                scene.explorer.tab = ExplorerTab::Model;
            }
        }
    };

    railButton(IconId::Project, SidebarSection::Explorer, Ui::NameId("SidebarRail::Explorer"));
    railButton(IconId::Parameters, SidebarSection::Parameters, Ui::NameId("SidebarRail::Parameters"));
    railButton(IconId::ProjectSettings, SidebarSection::ProjectSettings, Ui::NameId("SidebarRail::ProjectSettings"));

    Ui::Spacer();

    if (UiStyle::Button("", Ui::NameId("SidebarRail::AppSettings"),
            { .icon = IconId::Settings, .sizing = { Ui::Fixed(36), Ui::Fixed(36) }, .justify = Ui::Justify::Center, .bg = colors.bgDark })) {
        app.ActivateSettingsLayer();
    }

    Ui::CloseElement(); // SidebarRail
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

    // The tab bar carries Model/Geometry only. The Parameters view is rail-only, so it shows
    // no tab bar (nothing in the bar would be active for it).
    if (scene.explorer.tab != ExplorerTab::Parameters) {
        UiExplorerTabs(scene);
    }

    switch (scene.explorer.tab) {
    case ExplorerTab::Model:
        UiCommandTree(scene);
        UiFeatureProperties(scene); // editable parameters of the selected feature
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

// The left panel as a whole: the icon rail plus whichever section it selected. This is
// what App.cpp mounts in WorkbenchInner — App.cpp never reaches for UiExplorer directly.
void UiLeftPanel(Scene& scene, AppState& app)
{
    UiSidebarRail(scene, app);

    switch (scene.explorer.section) {
    case SidebarSection::Explorer:
    case SidebarSection::Parameters:
        UiExplorer(scene);
        break;
    case SidebarSection::ProjectSettings:
        UiProjectSettingsPanel(scene);
        break;
    }
}
} // namespace AppUi
