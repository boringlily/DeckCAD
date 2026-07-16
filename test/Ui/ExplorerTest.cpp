// The Explorer, driven headlessly.
//
// Runs the REAL panel — AppUi::UiExplorer, the same function App.cpp calls — through
// real BeginFrame/build/EndFrame cycles against a no-op draw backend, exactly like
// HoverTest does for the framework itself. No window, no GL, no assets.
//
// What that buys: the tree, the parameter table, and the parser-backed fields are
// exercised for real (layout solve + hit-test + input), so a broken Open/Close balance
// or an id collision fails here rather than at runtime.

#include "AppState.h"
#include "Gui/Explorer.h"

#include "Backend/IBackend.h"
#include "Core/UiContext.h"

#include <gtest/gtest.h>
#include <array>
#include <string>
#include <vector>

using namespace Ui;

namespace {

void NoopFill(void*, Rect, UiColor, f32) { }
void NoopBorder(void*, Rect, Edges, UiColor, f32) { }
void NoopScissorStart(void*, Rect) { }
void NoopScissorEnd(void*) { }

// Minimal text backend. The tree and table are almost all text, and a null TextBackend
// would leave the layout unable to size any label — so this measures with a simple
// fixed-advance model (enough for deterministic geometry) and draws nothing.
constexpr f32 kAdvance = 0.5f; // glyph width as a fraction of font size

TextMetrics MeasureText(void*, const char*, u32 len, u16, u16 size)
{
    return TextMetrics { static_cast<f32>(len) * size * kAdvance, static_cast<f32>(size) };
}
TextMetrics MeasureWrapped(void* u, const char* t, u32 len, u16 f, u16 s, f32) { return MeasureText(u, t, len, f, s); }
void NoopDrawText(void*, const char*, u32, Vec2, u16, u16, UiColor) { }
void NoopDrawWrapped(void*, const char*, u32, Rect, u16, u16, UiColor, s32, u32, u32, UiColor) { }
void NoopDrawStyled(void*, const char*, u32, Rect, u16, u16, UiColor, const TextStyleRun*, u32, s32, u32, u32, UiColor) { }

// Map a click x to a byte index using the same fixed-advance model as Measure, so
// click-to-position lands where the test expects.
u32 CaretAt(void*, const char*, u32 len, Rect box, u16, u16 size, s32, Vec2 point, bool)
{
    if (size == 0) {
        return len;
    }
    f32 rel = point.x - box.x;
    if (rel <= 0) {
        return 0;
    }
    u32 idx = static_cast<u32>(rel / (size * kAdvance) + 0.5f);
    return idx > len ? len : idx;
}

} // namespace

class ExplorerTest : public ::testing::Test {
protected:
    // The Explorer builds a lot of nodes (tree rows + a table row per parameter), so
    // this is sized well past the default demo budget.
    std::vector<unsigned char> buffer;
    Context ctx {};
    AppState app {};
    PointerState pointer {};
    KeyboardState keyboard {};

    void SetUp() override
    {
        buffer.assign(1u << 22, 0); // 4 MiB, matching Graphics.cpp's real arena

        UiInitDesc desc {};
        desc.buffer = buffer.data();
        desc.bufferBytes = buffer.size();
        desc.maxNodes = 4096;
        desc.maxCommands = 8192;
        desc.maxScrollStates = 256;
        desc.backend.draw.FillRect = &NoopFill;
        desc.backend.draw.Border = &NoopBorder;
        desc.backend.draw.ScissorStart = &NoopScissorStart;
        desc.backend.draw.ScissorEnd = &NoopScissorEnd;
        desc.backend.text.Measure = &MeasureText;
        desc.backend.text.MeasureWrapped = &MeasureWrapped;
        desc.backend.text.Draw = &NoopDrawText;
        desc.backend.text.DrawWrapped = &NoopDrawWrapped;
        desc.backend.text.DrawStyled = &NoopDrawStyled;
        desc.backend.text.CaretIndexAt = &CaretAt;

        ASSERT_TRUE(ctx.Init(desc));
        SetCurrent(&ctx);
    }

    void TearDown() override { SetCurrent(nullptr); }

    Scene& scene()
    {
        Scene* s = app.GetActiveScene();
        EXPECT_NE(s, nullptr);
        return *s;
    }

    // Select an Explorer tab. Only the active tab's table is built, so a test that wants
    // the parameter rows has to be on the Parameters tab first — same as the user.
    void ShowTab(ExplorerTab t) { scene().explorer.tab = t; }

    // One full frame through the real panel.
    void Frame()
    {
        BeginFrame({ 1280, 800 }, pointer, keyboard);
        LayoutConfig root {};
        root.sizing = { Fixed(1280), Fixed(800) };
        OpenElement(root, NameId("TestRoot"));
        AppUi::UiExplorer(scene());
        CloseElement();
        EndFrame();
        keyboard = {}; // typed input is per-frame
    }

    // Hover an id, then click it. Hit-testing resolves at EndFrame, so a click needs a
    // frame with the pointer over the target before the press frame — this is the same
    // 1-frame lag the real app has.
    void ClickAt(Vec2 p)
    {
        pointer = {};
        pointer.pos = p;
        Frame(); // resolve hover
        pointer.down = true;
        pointer.pressed = true;
        Frame(); // press
        pointer.down = false;
        pointer.pressed = false;
        pointer.released = true;
        Frame(); // release -> IsClicked
        pointer = {};
    }

    // Find a node's rect by id from the last resolved frame.
    DTL::Optional<Rect> RectOf(UiId id)
    {
        for (u32 k = 0; k < ctx.nodeCount; ++k) {
            if (ctx.nodes[k].id == id) {
                return ctx.nodes[k].rect;
            }
        }
        return std::nullopt;
    }

    bool HasNode(UiId id) { return RectOf(id).has_value(); }

    void Type(std::string_view text)
    {
        for (char c : text) {
            keyboard = {};
            keyboard.typed[0] = static_cast<u32>(c);
            keyboard.typedCount = 1;
            Frame();
        }
    }

    FeatureId AuthorSketch(u32 lines = 2)
    {
        Workbench& wb = scene().workbench;
        wb.StartTool(ToolId::CreateSketch);
        wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
        wb.FinishTool();
        FeatureId id = wb.Contexts().ActiveSketch()->featureId;
        for (u32 k = 0; k < lines; ++k) {
            wb.StartTool(ToolId::Line);
            wb.ActiveTool().AddPoint({ 0, static_cast<f64>(k) });
            wb.ActiveTool().AddPoint({ 10, static_cast<f64>(k) });
            wb.FinishTool();
        }
        wb.StartTool(ToolId::FinishSketch);
        return id;
    }
};

// ── it builds at all ─────────────────────────────────────────────────────────

TEST_F(ExplorerTest, BuildsWithAnEmptyScene)
{
    Frame();
    EXPECT_TRUE(HasNode(NameId("WorkbenchExplorer")));
    EXPECT_TRUE(HasNode(NameId("Explorer::Tabs")));
    EXPECT_TRUE(HasNode(NameId("Explorer::Tree"))); // Model is the default tab
    EXPECT_FALSE(ctx.overflowed);
}

TEST_F(ExplorerTest, EveryTabBuildsWithoutOverflowing)
{
    AuthorSketch(3);
    scene().workbench.Params().Create("w", "100mm");

    for (const ExplorerTabInfo& t : kExplorerTabs) {
        ShowTab(t.tab);
        Frame();
        EXPECT_FALSE(ctx.overflowed) << "overflowed on tab " << t.name;
        EXPECT_EQ(ctx.openDepth, 1u) << "unbalanced on tab " << t.name;
    }
}

TEST_F(ExplorerTest, OpenAndCloseElementsStayBalanced)
{
    // BeginFrame opens an implicit root element and EndFrame leaves it open, so a
    // perfectly balanced user tree settles at depth 1. Anything else means the Explorer
    // leaked an OpenElement (or over-closed), which silently reparents everything built
    // afterwards.
    Frame();
    EXPECT_EQ(ctx.openDepth, 1u);
    Frame();
    EXPECT_EQ(ctx.openDepth, 1u);

    // Same with content, where the nesting is real.
    AuthorSketch(2);
    scene().workbench.Params().Create("w", "100mm");
    Frame();
    EXPECT_EQ(ctx.openDepth, 1u);
}

TEST_F(ExplorerTest, RebuildingIsStableAcrossManyFrames)
{
    AuthorSketch(3);
    scene().workbench.Params().Create("w", "100mm");

    u32 first = 0;
    for (u32 k = 0; k < 10; ++k) {
        Frame();
        if (k == 0) {
            first = ctx.nodeCount;
        } else {
            EXPECT_EQ(ctx.nodeCount, first) << "node count drifted on frame " << k;
        }
    }
    EXPECT_FALSE(ctx.overflowed);
}

// ── the command tree ─────────────────────────────────────────────────────────

TEST_F(ExplorerTest, EmptySceneShowsThePlaceholder)
{
    Frame();
    EXPECT_TRUE(HasNode(NameId("Explorer::Empty")));
}

TEST_F(ExplorerTest, CommittedFeatureAppearsAsARow)
{
    FeatureId sketch = AuthorSketch(2);
    Frame();
    EXPECT_FALSE(HasNode(NameId("Explorer::Empty")));
    EXPECT_TRUE(HasNode(NameId("ExplorerFeature", sketch)));
}

TEST_F(ExplorerTest, ChildCommandsRenderNestedAndIndented)
{
    AuthorSketch(2);
    Frame();

    const std::vector<Command>& history = scene().workbench.Doc().History();
    ASSERT_EQ(history.size(), 1u);
    const SketchFeatureCommand* sf = history[0].As<SketchFeatureCommand>();
    ASSERT_NE(sf, nullptr);
    ASSERT_EQ(sf->children.size(), 2u);

    Ui::UiId parentRow = NameId("ExplorerFeature", history[0].Get().id);
    Ui::UiId childRow = NameId("ExplorerNode", sf->children[0].Get().id);

    DTL::Optional<Rect> parent = RectOf(parentRow);
    DTL::Optional<Rect> child = RectOf(childRow);
    ASSERT_TRUE(parent.has_value());
    ASSERT_TRUE(child.has_value());

    // Rows span the full panel width, so the row rects share an x. Indentation is the
    // row's left padding, which moves its CONTENT — so compare the labels.
    DTL::Optional<Rect> parentLabel = RectOf(HashChild(parentRow, 1));
    DTL::Optional<Rect> childLabel = RectOf(HashChild(childRow, 1));
    ASSERT_TRUE(parentLabel.has_value());
    ASSERT_TRUE(childLabel.has_value());

    EXPECT_GT(childLabel->x, parentLabel->x) << "child row is not indented";
    EXPECT_GT(child->y, parent->y) << "child row is not below its parent";
}

TEST_F(ExplorerTest, CollapsingHidesChildren)
{
    AuthorSketch(2);
    const std::vector<Command>& history = scene().workbench.Doc().History();
    FeatureId sketch = history[0].Get().id;
    const SketchFeatureCommand* sf = history[0].As<SketchFeatureCommand>();
    FeatureId child = sf->children[0].Get().id;

    Frame();
    ASSERT_TRUE(HasNode(NameId("ExplorerNode", child)));

    scene().explorer.ToggleCollapsed(sketch);
    Frame();
    EXPECT_FALSE(HasNode(NameId("ExplorerNode", child)));
    EXPECT_TRUE(HasNode(NameId("ExplorerFeature", sketch))); // parent still there

    scene().explorer.ToggleCollapsed(sketch);
    Frame();
    EXPECT_TRUE(HasNode(NameId("ExplorerNode", child)));
}

TEST_F(ExplorerTest, ClickingTheChevronTogglesCollapse)
{
    // Drives collapse through the real chevron button rather than the state directly.
    AuthorSketch(2);
    FeatureId sketch = scene().workbench.Doc().History()[0].Get().id;
    Frame();

    Ui::UiId rowId = NameId("ExplorerFeature", sketch);
    DTL::Optional<Rect> chev = RectOf(HashChild(rowId, 7));
    ASSERT_TRUE(chev.has_value()) << "expand/collapse chevron not found";

    ClickAt({ chev->x + chev->w * 0.5f, chev->y + chev->h * 0.5f });
    EXPECT_TRUE(scene().explorer.IsCollapsed(sketch));
}

TEST_F(ExplorerTest, ClickingARowSelectsIt)
{
    FeatureId sketch = AuthorSketch(1);
    Frame();

    DTL::Optional<Rect> row = RectOf(NameId("ExplorerFeature", sketch));
    ASSERT_TRUE(row.has_value());
    // Click the label area, clear of the chevron on the left.
    ClickAt({ row->x + row->w * 0.6f, row->y + row->h * 0.5f });

    EXPECT_EQ(scene().explorer.selected, sketch);
}

TEST_F(ExplorerTest, InProgressSketchIsShownBeforeItIsCommitted)
{
    Workbench& wb = scene().workbench;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();

    Frame();
    // Nothing is in document history yet, but the tree still shows the live sketch.
    EXPECT_EQ(wb.Doc().Size(), 0u);
    EXPECT_TRUE(HasNode(NameId("ExplorerActiveSketch")));
}

TEST_F(ExplorerTest, MirrorGroupRendersAsANestedSubtree)
{
    Workbench& wb = scene().workbench;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = wb.Contexts().ActiveSketch()->children.back().Get().id;

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    ASSERT_TRUE(wb.FinishTool());

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 8, 2 });
    ASSERT_TRUE(wb.FinishTool());

    Frame();

    const SketchContext* sketch = wb.Contexts().ActiveSketch();
    ASSERT_NE(sketch, nullptr);
    const CompoundSketchCmd* group = sketch->children[1].As<CompoundSketchCmd>();
    ASSERT_NE(group, nullptr);

    // The group row and all three of its children are present, and indented under it.
    // Indentation is left padding, so compare labels rather than the full-width rows.
    Ui::UiId groupRow = NameId("ExplorerNode", group->id);
    DTL::Optional<Rect> groupLabel = RectOf(HashChild(groupRow, 1));
    ASSERT_TRUE(groupLabel.has_value());
    for (const SketchCmd& c : group->children) {
        Ui::UiId childRow = NameId("ExplorerNode", c.Get().id);
        DTL::Optional<Rect> childLabel = RectOf(HashChild(childRow, 1));
        ASSERT_TRUE(childLabel.has_value()) << "missing child row for " << c.Get().TypeName();
        EXPECT_GT(childLabel->x, groupLabel->x);
    }
}

TEST_F(ExplorerTest, UndoneFeaturesStillAppear)
{
    FeatureId sketch = AuthorSketch(1);
    ASSERT_TRUE(scene().workbench.Undo());
    Frame();
    // Undo moves the cursor; the command is still in history and still listed.
    EXPECT_TRUE(HasNode(NameId("ExplorerFeature", sketch)));
}

TEST_F(ExplorerTest, DeletingAFeatureFromTheTreeRemovesItsRow)
{
    FeatureId sketch = AuthorSketch(1);
    Frame();
    Ui::UiId rowId = NameId("ExplorerFeature", sketch);

    // The delete chip only exists while the row is hovered.
    DTL::Optional<Rect> row = RectOf(rowId);
    ASSERT_TRUE(row.has_value());
    pointer = {};
    pointer.pos = { row->x + row->w * 0.6f, row->y + row->h * 0.5f };
    Frame(); // hover resolves
    Frame(); // the chip is built this frame

    DTL::Optional<Rect> del = RectOf(HashChild(rowId, 3));
    ASSERT_TRUE(del.has_value()) << "hover-revealed delete button never appeared";

    ClickAt({ del->x + del->w * 0.5f, del->y + del->h * 0.5f });

    EXPECT_EQ(scene().workbench.Doc().Size(), 0u);
    Frame();
    EXPECT_FALSE(HasNode(rowId));
}

// ── tabs ─────────────────────────────────────────────────────────────────────

TEST_F(ExplorerTest, TabsShowOnlyTheirOwnTable)
{
    scene().workbench.Params().Create("w", "100mm");
    AuthorSketch(1);

    ShowTab(ExplorerTab::Model);
    Frame();
    EXPECT_TRUE(HasNode(NameId("Explorer::Tree")));
    EXPECT_FALSE(HasNode(NameId("Explorer::Params")));
    EXPECT_FALSE(HasNode(NameId("Explorer::Geometry")));

    ShowTab(ExplorerTab::Parameters);
    Frame();
    EXPECT_FALSE(HasNode(NameId("Explorer::Tree")));
    EXPECT_TRUE(HasNode(NameId("Explorer::Params")));
    EXPECT_FALSE(HasNode(NameId("Explorer::Geometry")));

    ShowTab(ExplorerTab::Geometry);
    Frame();
    EXPECT_FALSE(HasNode(NameId("Explorer::Tree")));
    EXPECT_FALSE(HasNode(NameId("Explorer::Params")));
    EXPECT_TRUE(HasNode(NameId("Explorer::Geometry")));
}

TEST_F(ExplorerTest, ClickingATabSwitchesIt)
{
    Frame();
    ASSERT_EQ(scene().explorer.tab, ExplorerTab::Model);

    Ui::UiId paramTab = NameId("Explorer::Tab", static_cast<u32>(ExplorerTab::Parameters));
    DTL::Optional<Rect> r = RectOf(paramTab);
    ASSERT_TRUE(r.has_value());
    ClickAt({ r->x + r->w * 0.5f, r->y + r->h * 0.5f });

    EXPECT_EQ(scene().explorer.tab, ExplorerTab::Parameters);
}

// ── the parameter table ──────────────────────────────────────────────────────

TEST_F(ExplorerTest, EmptyTableShowsThePlaceholder)
{
    ShowTab(ExplorerTab::Parameters);
    Frame();
    EXPECT_TRUE(HasNode(NameId("Explorer::NoParams")));
}

TEST_F(ExplorerTest, ARowAppearsForEachParameter)
{
    ShowTab(ExplorerTab::Parameters);
    Param::UPID w = scene().workbench.Params().Create("w", "100mm");
    Param::UPID h = scene().workbench.Params().Create("h", "$w / 2");
    Frame();

    EXPECT_FALSE(HasNode(NameId("Explorer::NoParams")));
    EXPECT_TRUE(HasNode(NameId("ParamRow", w)));
    EXPECT_TRUE(HasNode(NameId("ParamRow", h)));
}

TEST_F(ExplorerTest, RowBuffersAreSeededFromTheEngine)
{
    ShowTab(ExplorerTab::Parameters);
    Param::UPID w = scene().workbench.Params().Create("width", "100mm");
    Frame();

    ParamRow* row = scene().explorer.FindRow(w);
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(std::string(row->name, row->nameLen), "width");
    EXPECT_EQ(std::string(row->expr, row->exprLen), "100mm");
}

TEST_F(ExplorerTest, RowsAreDroppedWhenTheParameterIsRemoved)
{
    ShowTab(ExplorerTab::Parameters);
    Param::UPID w = scene().workbench.Params().Create("w", "1");
    Frame();
    ASSERT_NE(scene().explorer.FindRow(w), nullptr);

    scene().workbench.Params().Remove(w);
    Frame();

    EXPECT_EQ(scene().explorer.FindRow(w), nullptr);
    EXPECT_FALSE(HasNode(NameId("ParamRow", w)));
}

TEST_F(ExplorerTest, RowsAreKeyedByUpidNotIndex)
{
    ShowTab(ExplorerTab::Parameters);
    // Removing the first parameter must not re-point the second row's buffer at it.
    Param::ParameterEngine& params = scene().workbench.Params();
    Param::UPID a = params.Create("a", "1");
    Param::UPID b = params.Create("b", "2");
    Frame();

    params.Remove(a);
    Frame();

    ParamRow* rowB = scene().explorer.FindRow(b);
    ASSERT_NE(rowB, nullptr);
    EXPECT_EQ(rowB->id, b);
    EXPECT_EQ(std::string(rowB->name, rowB->nameLen), "b");
    EXPECT_EQ(scene().explorer.rows.size(), 1u);
}

TEST_F(ExplorerTest, TypingIntoAnExpressionFieldUpdatesTheEngine)
{
    ShowTab(ExplorerTab::Parameters);
    Param::ParameterEngine& params = scene().workbench.Params();
    Param::UPID w = params.Create("w", "10");
    Frame();

    Ui::UiId rowId = NameId("ParamRow", w);
    Ui::UiId fieldId = HashChild(rowId, 4);
    DTL::Optional<Rect> field = RectOf(fieldId);
    ASSERT_TRUE(field.has_value());

    // Focus the field, put the caret at the end, then type.
    ClickAt({ field->x + field->w * 0.9f, field->y + field->h * 0.5f });
    ASSERT_TRUE(IsFocused(fieldId));
    SetCaretPos(scene().explorer.FindRow(w)->exprLen);

    Type("0"); // "10" -> "100"

    EXPECT_EQ(params.Get(w)->Text(), "100");
    EXPECT_NEAR(params.Value(w).value.value, 100.0, 1e-9);
}

TEST_F(ExplorerTest, EditingAParameterReDrivesDimensionedGeometry)
{
    // The whole point of the panel, end to end through the real UI.
    Workbench& wb = scene().workbench;
    Param::UPID w = wb.Params().Create("w", "10");

    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 5, 0 });
    wb.FinishTool();
    FeatureId line = wb.Contexts().ActiveSketch()->children.back().Get().id;

    wb.StartTool(ToolId::Dimension);
    wb.ActiveTool().AddPick(line);
    wb.ActiveTool().SetValue("$w");
    ASSERT_TRUE(wb.FinishTool());
    wb.StartTool(ToolId::FinishSketch);

    auto lengthOf = [&]() {
        const SketchEntity* e = wb.Evaluated().sketches.at(0).Find(line);
        return e ? std::sqrt((e->b.x - e->a.x) * (e->b.x - e->a.x) + (e->b.y - e->a.y) * (e->b.y - e->a.y)) : -1.0;
    };
    ASSERT_NEAR(lengthOf(), 10.0, 1e-6);

    ShowTab(ExplorerTab::Parameters);
    Frame();
    Ui::UiId fieldId = HashChild(NameId("ParamRow", w), 4);
    DTL::Optional<Rect> field = RectOf(fieldId);
    ASSERT_TRUE(field.has_value());
    ClickAt({ field->x + field->w * 0.9f, field->y + field->h * 0.5f });
    SetCaretPos(scene().explorer.FindRow(w)->exprLen);

    Type("0"); // "10" -> "100"

    EXPECT_NEAR(lengthOf(), 100.0, 1e-6);
}

TEST_F(ExplorerTest, AddingAParameterThroughTheAddRow)
{
    ShowTab(ExplorerTab::Parameters);
    Frame();

    DTL::Optional<Rect> nameField = RectOf(NameId("ParamAdd::Name"));
    ASSERT_TRUE(nameField.has_value());
    ClickAt({ nameField->x + nameField->w * 0.5f, nameField->y + nameField->h * 0.5f });
    Type("depth");

    DTL::Optional<Rect> exprField = RectOf(NameId("ParamAdd::Expr"));
    ASSERT_TRUE(exprField.has_value());
    ClickAt({ exprField->x + exprField->w * 0.5f, exprField->y + exprField->h * 0.5f });
    Type("25mm");

    DTL::Optional<Rect> addBtn = RectOf(NameId("ParamAdd::Button"));
    ASSERT_TRUE(addBtn.has_value());
    ClickAt({ addBtn->x + addBtn->w * 0.5f, addBtn->y + addBtn->h * 0.5f });

    Param::ParameterEngine& params = scene().workbench.Params();
    Param::UPID created = params.FindByName("depth");
    ASSERT_NE(created, Param::kNullUpid);
    EXPECT_NEAR(params.Value(created).value.value, 25.0, 1e-9);

    // The add row is cleared so the next entry starts blank.
    EXPECT_EQ(scene().explorer.newNameLen, 0u);
    EXPECT_EQ(scene().explorer.newExprLen, 0u);
}

TEST_F(ExplorerTest, DimensionValuesAppearInTheTableAlongsideUserParameters)
{
    ShowTab(ExplorerTab::Parameters);
    Workbench& wb = scene().workbench;
    wb.Params().Create("w", "50mm");

    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 5, 0 });
    wb.FinishTool();
    FeatureId line = wb.Contexts().ActiveSketch()->children.back().Get().id;
    wb.StartTool(ToolId::Dimension);
    wb.ActiveTool().AddPick(line);
    wb.ActiveTool().SetValue("$w * 2");
    ASSERT_TRUE(wb.FinishTool());

    Frame();

    // Two rows: the user parameter and the sketch's dimension value.
    EXPECT_EQ(scene().explorer.rows.size(), 2u);
    for (const Param::ParametricExpression& p : wb.Params().Parameters()) {
        EXPECT_TRUE(HasNode(NameId("ParamRow", p.Id())));
    }
}

// ── the geometry table ───────────────────────────────────────────────────────

TEST_F(ExplorerTest, GeometryTabIsEmptyBeforeAnythingIsGenerated)
{
    ShowTab(ExplorerTab::Geometry);
    Frame();
    EXPECT_TRUE(HasNode(NameId("Explorer::NoGeom")));
}

TEST_F(ExplorerTest, GeometryTabListsGeneratedEntities)
{
    AuthorSketch(3);
    ShowTab(ExplorerTab::Geometry);
    Frame();

    EXPECT_FALSE(HasNode(NameId("Explorer::NoGeom")));
    const SketchDocument& doc = scene().workbench.Evaluated().sketches.at(0);
    ASSERT_EQ(doc.entities.size(), 3u);
    for (const SketchEntity& e : doc.entities) {
        EXPECT_TRUE(HasNode(NameId("GeomRow", e.id))) << "no row for entity " << e.id;
    }
}

TEST_F(ExplorerTest, GeometryTabShowsTheSketchBeingAuthoredBeforeItIsCommitted)
{
    Workbench& wb = scene().workbench;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();
    FeatureId line = wb.Contexts().ActiveSketch()->children.back().Get().id;

    ShowTab(ExplorerTab::Geometry);
    Frame();

    // Nothing is committed, but the geometry the canvas is showing is listed.
    EXPECT_EQ(wb.Doc().Size(), 0u);
    EXPECT_TRUE(HasNode(NameId("GeomRow", line)));
}

TEST_F(ExplorerTest, GeometryTabReflectsAnAppliedDimensionImmediately)
{
    Workbench& wb = scene().workbench;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();
    FeatureId line = wb.Contexts().ActiveSketch()->children.back().Get().id;

    SketchDocument before;
    ASSERT_TRUE(wb.BuildSketchPreview(before));
    EXPECT_FALSE(before.IsDriven(line));
    EXPECT_NEAR(*before.MeasureLength(line), 10.0, 1e-6);

    wb.StartTool(ToolId::Dimension);
    wb.ActiveTool().AddPick(line);
    wb.ActiveTool().SetValue("100mm");
    ASSERT_TRUE(wb.FinishTool());

    ShowTab(ExplorerTab::Geometry);
    Frame();

    // Still inside the sketch — no Finish Sketch — and the geometry has already moved.
    EXPECT_NE(wb.Contexts().ActiveSketch(), nullptr);
    EXPECT_EQ(wb.Doc().Size(), 0u);

    SketchDocument after;
    ASSERT_TRUE(wb.BuildSketchPreview(after));
    EXPECT_TRUE(after.IsDriven(line));
    EXPECT_NEAR(*after.MeasureLength(line), 100.0, 1e-6);
}

TEST_F(ExplorerTest, ClickingAGeometryRowSelectsIt)
{
    AuthorSketch(1);
    ShowTab(ExplorerTab::Geometry);
    Frame();

    FeatureId id = scene().workbench.Evaluated().sketches.at(0).entities.at(0).id;
    DTL::Optional<Rect> row = RectOf(NameId("GeomRow", id));
    ASSERT_TRUE(row.has_value());
    ClickAt({ row->x + row->w * 0.4f, row->y + row->h * 0.5f });

    EXPECT_EQ(scene().explorer.selected, id);
}

// ── the parser-backed field ──────────────────────────────────────────────────

TEST_F(ExplorerTest, ExpressionParserProducesHighlightRuns)
{
    ExprField::ParserBinding binding {};
    binding.engine = &scene().workbench.Params();
    // A Length, so the whole expression is dimensionally consistent — mixing a Length
    // and a bare Number here would (correctly) be a UnitMismatch.
    scene().workbench.Params().Create("w", "10mm");

    std::array<TextStyleRun, 32> runs {};
    TextParseResult out {};
    out.runs = runs.data();
    out.runCap = static_cast<u32>(runs.size());

    const char* text = "$w + @Min(1mm, 2mm)";
    TextParser parser = ExprField::MakeParser(binding);
    parser.Parse(parser.user, text, static_cast<u32>(std::strlen(text)), &out);

    EXPECT_FALSE(out.hasError);
    EXPECT_GT(out.runCount, 0u);

    // Runs must be sorted, non-overlapping, and in-bounds — the framework relies on it.
    u32 len = static_cast<u32>(std::strlen(text));
    for (u32 k = 0; k < out.runCount; ++k) {
        EXPECT_LE(out.runs[k].start + out.runs[k].length, len);
        if (k > 0) {
            EXPECT_GE(out.runs[k].start, out.runs[k - 1].start + out.runs[k - 1].length);
        }
    }
}

TEST_F(ExplorerTest, ExpressionParserReportsAnUnknownParameterAgainstTheLiveTable)
{
    ExprField::ParserBinding binding {};
    binding.engine = &scene().workbench.Params();

    std::array<TextStyleRun, 32> runs {};
    TextParseResult out {};
    out.runs = runs.data();
    out.runCap = static_cast<u32>(runs.size());

    const char* text = "$ghost + 1";
    TextParser parser = ExprField::MakeParser(binding);
    parser.Parse(parser.user, text, static_cast<u32>(std::strlen(text)), &out);

    EXPECT_TRUE(out.hasError);
    ASSERT_NE(out.message, nullptr);

    // The offending run is marked as an error rather than a new overlapping run.
    bool marked = false;
    for (u32 k = 0; k < out.runCount; ++k) {
        if (out.runs[k].decoration == TextDecoration::Error) {
            marked = true;
        }
    }
    EXPECT_TRUE(marked);

    // ...and it clears once the parameter exists.
    scene().workbench.Params().Create("ghost", "1");
    out.runCount = 0;
    out.hasError = false;
    parser.Parse(parser.user, text, static_cast<u32>(std::strlen(text)), &out);
    EXPECT_FALSE(out.hasError);
}

TEST_F(ExplorerTest, ExpressionParserHandlesAnEmptyFieldWithoutError)
{
    ExprField::ParserBinding binding {};
    binding.engine = &scene().workbench.Params();

    std::array<TextStyleRun, 8> runs {};
    TextParseResult out {};
    out.runs = runs.data();
    out.runCap = static_cast<u32>(runs.size());

    TextParser parser = ExprField::MakeParser(binding);
    parser.Parse(parser.user, "", 0, &out);

    EXPECT_FALSE(out.hasError); // an empty field is empty, not wrong
    EXPECT_EQ(out.runCount, 0u);
}

TEST_F(ExplorerTest, ExpressionParserRespectsRunCapacity)
{
    // A long expression must not write past the framework's scratch buffer.
    ExprField::ParserBinding binding {};
    binding.engine = &scene().workbench.Params();

    std::array<TextStyleRun, 4> runs {};
    TextParseResult out {};
    out.runs = runs.data();
    out.runCap = static_cast<u32>(runs.size());

    std::string text = "1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9";
    TextParser parser = ExprField::MakeParser(binding);
    parser.Parse(parser.user, text.c_str(), static_cast<u32>(text.size()), &out);

    EXPECT_LE(out.runCount, out.runCap);
}
