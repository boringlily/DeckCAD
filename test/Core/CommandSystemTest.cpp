// The command architecture's actual contract.
//
// These tests exist to pin the decisions that are easy to regress silently: that a
// composite is one undo step, that a pass-through context commits for real as it goes
// while a provisional one commits nothing until confirm, that tool availability is a
// property of the context stack, and that replay is deterministic.

#include "Context.h"
#include "Document.h"
#include "ParameterEngine.h"
#include "SketchCmd.h"
#include "Tool.h"
#include "Workbench.h"

#include <gtest/gtest.h>

namespace {

constexpr f64 kEps = 1e-9;

// A line references two points by id. The variant/composite tests below don't execute the
// command (or add the points), so any ids will do — the point references are what we assert
// the machinery preserves.
SketchLine MakeLine(FeatureId p0 = 1, FeatureId p1 = 2)
{
    SketchLine l {};
    l.p0 = p0;
    l.p1 = p1;
    return l;
}

// A curve gesture now commits a compound of points + a curve. These helpers dig the curve
// (and its points) back out of that structure, which is what every consumer references.
FeatureId LastCurveIdIn(const std::vector<SketchCmd>& cmds)
{
    FeatureId found = kNullFeature;
    for (const SketchCmd& c : cmds) {
        if (c.Is<SketchLine>() || c.Is<SketchCircle>() || c.Is<SketchArc>()) {
            found = c.Get().id;
        } else if (const CompoundSketchCmd* g = c.As<CompoundSketchCmd>()) {
            FeatureId nested = LastCurveIdIn(g->children);
            if (nested != kNullFeature) {
                found = nested;
            }
        }
    }
    return found;
}

FeatureId LastCurveId(const Workbench& wb)
{
    const SketchContext* s = wb.Contexts().ActiveSketch();
    return s ? LastCurveIdIn(s->children) : kNullFeature;
}

// Find a line command by id, recursing into gesture compounds.
const SketchLine* FindLineCmd(const std::vector<SketchCmd>& cmds, FeatureId id)
{
    for (const SketchCmd& c : cmds) {
        if (const SketchLine* l = c.As<SketchLine>()) {
            if (l->id == id) {
                return l;
            }
        }
        if (const CompoundSketchCmd* g = c.As<CompoundSketchCmd>()) {
            if (const SketchLine* nested = FindLineCmd(g->children, id)) {
                return nested;
            }
        }
    }
    return nullptr;
}

// The SketchLine directly inside a gesture compound (or the command itself if it is a bare
// line), and the seed position of a point declared in that same compound.
const SketchLine* LineIn(const SketchCmd& cmd)
{
    if (const SketchLine* l = cmd.As<SketchLine>()) {
        return l;
    }
    if (const CompoundSketchCmd* g = cmd.As<CompoundSketchCmd>()) {
        for (const SketchCmd& c : g->children) {
            if (const SketchLine* l = c.As<SketchLine>()) {
                return l;
            }
        }
    }
    return nullptr;
}

Geometry::Point2 PointInit(const SketchCmd& compound, FeatureId pid)
{
    if (const CompoundSketchCmd* g = compound.As<CompoundSketchCmd>()) {
        for (const SketchCmd& c : g->children) {
            if (const SketchPointCmd* p = c.As<SketchPointCmd>()) {
                if (p->id == pid) {
                    return p->initial;
                }
            }
        }
    }
    return {};
}

bool HasTool(const std::vector<ToolId>& tools, ToolId id)
{
    for (ToolId t : tools) {
        if (t == id) {
            return true;
        }
    }
    return false;
}

// Drive a sketch into `wb` and confirm it, returning the sketch's FeatureId.
FeatureId AuthorSketch(Workbench& wb, u32 lineCount)
{
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    const SketchContext* s = wb.Contexts().ActiveSketch();
    FeatureId id = s ? s->featureId : kNullFeature;

    for (u32 k = 0; k < lineCount; ++k) {
        wb.StartTool(ToolId::Line);
        wb.ActiveTool().AddPoint({ 0, static_cast<f64>(k) });
        wb.ActiveTool().AddPoint({ 10, static_cast<f64>(k) });
        wb.FinishTool();
    }

    wb.StartTool(ToolId::FinishSketch);
    return id;
}

} // namespace

// ── PolymorphicVariant ───────────────────────────────────────────────────────

TEST(PolymorphicVariant, NarrowsWithoutRtti)
{
    SketchCmd c { MakeLine(1, 2) };
    ASSERT_TRUE(c.Is<SketchLine>());
    ASSERT_NE(c.As<SketchLine>(), nullptr);
    EXPECT_EQ(c.As<SketchCircle>(), nullptr);
    EXPECT_EQ(c.As<SketchLine>()->p1, 2u);
}

TEST(PolymorphicVariant, GetReturnsTheLiveSubobjectNotACopy)
{
    // If Get() sliced or copied, mutating through the Base& would be lost.
    SketchCmd c { MakeLine(1, 2) };
    c.Get().id = 42;
    EXPECT_EQ(c.As<SketchLine>()->id, 42u);
    EXPECT_EQ(&c.Get(), static_cast<SketchCmdBase*>(c.As<SketchLine>()));
}

TEST(PolymorphicVariant, VirtualDispatchWorksThroughTheBase)
{
    SketchCmd line { MakeLine(1, 2) };
    SketchCmd circle { SketchCircle {} };
    EXPECT_EQ(line.Get().TypeName(), "Line");
    EXPECT_EQ(circle.Get().TypeName(), "Circle");
}

TEST(PolymorphicVariant, IndexIsStableAndDistinct)
{
    EXPECT_NE(SketchCmd { SketchLine {} }.Index(), SketchCmd { SketchCircle {} }.Index());
}

TEST(PolymorphicVariant, CopyIsADeepCopyWithNoCloneVirtual)
{
    // The payoff for values-over-unique_ptr: edit-mode deep copy is just `=`.
    CompoundSketchCmd g {};
    g.children.push_back(SketchCmd { MakeLine(1, 2) });

    SketchCmd original { g };
    SketchCmd copy = original;

    copy.As<CompoundSketchCmd>()->children[0].As<SketchLine>()->p1 = 99u;

    EXPECT_EQ(original.As<CompoundSketchCmd>()->children[0].As<SketchLine>()->p1, 2u);
    EXPECT_EQ(copy.As<CompoundSketchCmd>()->children[0].As<SketchLine>()->p1, 99u);
}

TEST(PolymorphicVariant, VectorOfCommandsCopiesDeeply)
{
    std::vector<SketchCmd> a;
    a.push_back(SketchCmd { MakeLine(1, 2) });
    std::vector<SketchCmd> b = a;
    b[0].As<SketchLine>()->p0 = 5u;
    EXPECT_EQ(a[0].As<SketchLine>()->p0, 1u);
}

// ── recursive composite ──────────────────────────────────────────────────────

TEST(CompositeCommand, NestsArbitrarilyDeep)
{
    CompoundSketchCmd inner {};
    inner.children.push_back(SketchCmd { MakeLine() });

    CompoundSketchCmd outer {};
    outer.children.push_back(SketchCmd { std::move(inner) });
    outer.children.push_back(SketchCmd { MakeLine() });

    std::vector<SketchCmd> history;
    history.push_back(SketchCmd { std::move(outer) });

    // outer + inner + inner's line + outer's line
    EXPECT_EQ(CountSketchCmds(history), 4u);
}

TEST(CompositeCommand, ExecutesEveryDescendant)
{
    // Lines reference points, so the group must create those points before the lines that
    // reference them (exactly as a real gesture compound does).
    CompoundSketchCmd g {};
    SketchPointCmd p1 {};
    p1.id = 1;
    p1.initial = { 0, 0 };
    SketchPointCmd p2 {};
    p2.id = 2;
    p2.initial = { 1, 0 };
    SketchPointCmd p3 {};
    p3.id = 3;
    p3.initial = { 0, 1 };
    SketchPointCmd p4 {};
    p4.id = 4;
    p4.initial = { 1, 1 };
    SketchLine a = MakeLine(1, 2);
    a.id = 5;
    SketchLine b = MakeLine(3, 4);
    b.id = 6;
    g.children.push_back(SketchCmd { p1 });
    g.children.push_back(SketchCmd { p2 });
    g.children.push_back(SketchCmd { p3 });
    g.children.push_back(SketchCmd { p4 });
    g.children.push_back(SketchCmd { a });
    g.children.push_back(SketchCmd { b });

    SketchDocument doc;
    ASSERT_TRUE(g.execute(doc).Ok());
    EXPECT_EQ(doc.entities.size(), 2u); // two curves (points live in doc.points)
    EXPECT_EQ(doc.points.size(), 4u);
}

TEST(CompositeCommand, AFailingChildFailsTheWholeGroup)
{
    CompoundSketchCmd g {};
    SketchDimensionCmd bad {}; // no target
    g.children.push_back(SketchCmd { bad });

    SketchDocument doc;
    EXPECT_FALSE(g.execute(doc).Ok());
}

TEST(CompositeCommand, WalkReportsDepth)
{
    CompoundSketchCmd inner {};
    inner.children.push_back(SketchCmd { MakeLine() });
    CompoundSketchCmd outer {};
    outer.children.push_back(SketchCmd { std::move(inner) });

    std::vector<SketchCmd> history;
    history.push_back(SketchCmd { std::move(outer) });

    std::vector<u32> depths;
    WalkSketchCmds(history, [&](const SketchCmdBase&, u32 d) { depths.push_back(d); });
    ASSERT_EQ(depths.size(), 3u);
    EXPECT_EQ(depths[0], 0u); // outer
    EXPECT_EQ(depths[1], 1u); // inner
    EXPECT_EQ(depths[2], 2u); // the line
}

// ── context scoping ──────────────────────────────────────────────────────────

TEST(ContextStack, RootOffersOnlyPartTools)
{
    Workbench wb;
    std::vector<ToolId> tools = wb.AvailableTools();
    EXPECT_TRUE(HasTool(tools, ToolId::CreateSketch));
    EXPECT_FALSE(HasTool(tools, ToolId::Line));
    EXPECT_FALSE(HasTool(tools, ToolId::FinishSketch));
}

TEST(ContextStack, SketchToolsAreIllegalAtRoot)
{
    Workbench wb;
    EXPECT_FALSE(wb.StartTool(ToolId::Line));
}

TEST(ContextStack, EnteringASketchSwapsTheAvailableTools)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    std::vector<ToolId> tools = wb.AvailableTools();
    EXPECT_TRUE(HasTool(tools, ToolId::Line));
    EXPECT_TRUE(HasTool(tools, ToolId::FinishSketch));
    // A part tool is illegal inside a sketch.
    EXPECT_FALSE(HasTool(tools, ToolId::CreateSketch));
    EXPECT_FALSE(wb.StartTool(ToolId::CreateSketch));
}

TEST(ContextStack, FinishSketchIsFilteredWhileAMirrorIsActive)
{
    // The documented gotcha: if both "Finish Sketch" and "Stop Mirror" were offered at
    // once, clicking Finish would be ambiguous. The nested exit must be explicit.
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    ASSERT_TRUE(HasTool(wb.AvailableTools(), ToolId::FinishSketch));

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    ASSERT_TRUE(wb.FinishTool());
    ASSERT_EQ(wb.Contexts().CurrentKind(), ContextKind::SymmetryGroup);

    std::vector<ToolId> tools = wb.AvailableTools();
    EXPECT_FALSE(HasTool(tools, ToolId::FinishSketch)); // <- the guard
    EXPECT_TRUE(HasTool(tools, ToolId::StopSymmetry));
    EXPECT_TRUE(HasTool(tools, ToolId::Line));

    // And it is genuinely refused, not merely hidden.
    EXPECT_FALSE(wb.StartTool(ToolId::FinishSketch));
}

TEST(ContextStack, EscapePopsExactlyOneLevelNeverStraightToRoot)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    wb.FinishTool();
    ASSERT_EQ(wb.Contexts().Depth(), 3u); // Root -> Sketch -> Symmetry

    EXPECT_TRUE(wb.Escape());
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Sketch);
    EXPECT_EQ(wb.Contexts().Depth(), 2u);

    EXPECT_TRUE(wb.Escape());
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Root);
}

TEST(ContextStack, EscapeDropsTheGestureBeforeThePopContext)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    ASSERT_TRUE(wb.ActiveTool().Active());

    EXPECT_TRUE(wb.Escape());
    EXPECT_FALSE(wb.ActiveTool().Active());
    // Still in the sketch: the first Escape only cancelled the half-drawn line.
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Sketch);
}

TEST(ContextStack, RootCannotBePopped)
{
    Workbench wb;
    EXPECT_FALSE(wb.Escape());
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Root);
}

TEST(ContextStack, MirrorNeedsAnAxisThatExists)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(9999); // no such line
    EXPECT_FALSE(wb.FinishTool());
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Sketch);
}

// ── composite (provisional) semantics: the Sketch context ────────────────────

TEST(SketchContext, CommitsNothingToDocumentHistoryUntilConfirm)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();

    // The line exists in the context, but the document knows nothing yet.
    EXPECT_EQ(wb.Contexts().ActiveSketch()->children.size(), 1u);
    EXPECT_EQ(wb.Doc().Size(), 0u);
    EXPECT_TRUE(wb.Evaluated().sketches.empty());

    wb.StartTool(ToolId::FinishSketch);

    EXPECT_EQ(wb.Doc().Size(), 1u);
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);
}

TEST(SketchContext, WholeSketchIsOneHistorySlotAndOneUndoStep)
{
    Workbench wb;
    AuthorSketch(wb, 3);

    EXPECT_EQ(wb.Doc().Size(), 1u); // three lines, ONE slot
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);
    EXPECT_EQ(wb.Evaluated().sketches[0].entities.size(), 3u);

    ASSERT_TRUE(wb.Undo());
    EXPECT_TRUE(wb.Evaluated().sketches.empty()); // the entire sketch, in one step
}

TEST(SketchContext, CancellingDiscardsEverythingLocal)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();

    ASSERT_TRUE(wb.Escape()); // provisional: nothing was ever real
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Root);
    EXPECT_EQ(wb.Doc().Size(), 0u);
}

// ── decorator (pass-through) semantics: SymmetryGroup ────────────────────────

TEST(SymmetryGroup, OneCommitProducesLineTwinAndConstraintInTheParent)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    // A vertical axis at x = 0.
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    ASSERT_TRUE(wb.FinishTool());

    ASSERT_EQ(wb.Contexts().ActiveSketch()->children.size(), 1u); // just the axis so far

    // Draw one line to the right of the axis.
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 8, 2 });
    ASSERT_TRUE(wb.FinishTool());

    // It went into the PARENT sketch immediately — the mirror owns no sub-document.
    const SketchContext* s = wb.Contexts().ActiveSketch();
    ASSERT_EQ(s->children.size(), 2u); // axis + one group

    const CompoundSketchCmd* g = s->children[1].As<CompoundSketchCmd>();
    ASSERT_NE(g, nullptr);
    // original gesture (a point+point+line compound) + twin gesture + symmetry constraint.
    ASSERT_EQ(g->children.size(), 3u);

    const SketchLine* original = LineIn(g->children[0]);
    const SketchLine* twin = LineIn(g->children[1]);
    const SketchConstraintCmd* link = g->children[2].As<SketchConstraintCmd>();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(twin, nullptr);
    ASSERT_NE(link, nullptr);

    // The twin's POINTs are the originals reflected across x = 0.
    Geometry::Point2 ta = PointInit(g->children[1], twin->p0);
    Geometry::Point2 tb = PointInit(g->children[1], twin->p1);
    EXPECT_NEAR(ta.x, -5.0, kEps);
    EXPECT_NEAR(ta.y, 1.0, kEps);
    EXPECT_NEAR(tb.x, -8.0, kEps);
    EXPECT_NEAR(tb.y, 2.0, kEps);

    // The symmetry constraint links the two CURVES (by id).
    EXPECT_EQ(link->kind, ConstraintKind::Symmetry);
    EXPECT_EQ(link->a, original->id);
    EXPECT_EQ(link->b, twin->id);
    EXPECT_EQ(link->axis, axis);
}

TEST(SymmetryGroup, TheGestureCollapsesToASingleUndoStep)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 8, 2 });
    wb.FinishTool();

    // Three commands' worth of effect, but ONE child slot in the sketch — because the
    // composite wraps the gesture before it is committed, not because undo groups it.
    EXPECT_EQ(wb.Contexts().ActiveSketch()->children.size(), 2u);
}

TEST(SymmetryGroup, ConfirmReturnsNothingBecauseItAlreadyCommitted)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 8, 2 });
    wb.FinishTool();

    u32 before = static_cast<u32>(wb.Contexts().ActiveSketch()->children.size());

    wb.StartTool(ToolId::StopSymmetry);

    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Sketch);
    // Stopping the mirror adds nothing: the effect was committed as it happened.
    EXPECT_EQ(wb.Contexts().ActiveSketch()->children.size(), before);
}

TEST(SymmetryGroup, CancellingLeavesAlreadyMirroredGeometryAlone)
{
    // Pass-through contexts have nothing to roll back — cancel only stops the auto
    // behaviour going forward. This mirrors how Dynamic Mirror behaves elsewhere.
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 8, 2 });
    wb.FinishTool();

    ASSERT_TRUE(wb.Escape()); // cancel the mirror

    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Sketch);
    EXPECT_EQ(wb.Contexts().ActiveSketch()->children.size(), 2u); // twin survives

    // And a line drawn afterwards is no longer mirrored: its gesture is a plain line
    // compound (a direct line child, no synthesized symmetry constraint), not a mirror
    // group (which nests the line + twin compounds and a symmetry link).
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 1, 1 });
    wb.ActiveTool().AddPoint({ 2, 2 });
    wb.FinishTool();
    const CompoundSketchCmd* last = wb.Contexts().ActiveSketch()->children.back().As<CompoundSketchCmd>();
    ASSERT_NE(last, nullptr);
    bool hasDirectLine = false;
    bool hasSymmetry = false;
    for (const SketchCmd& c : last->children) {
        if (c.Is<SketchLine>()) {
            hasDirectLine = true;
        }
        if (const auto* k = c.As<SketchConstraintCmd>()) {
            hasSymmetry = hasSymmetry || k->kind == ConstraintKind::Symmetry;
        }
    }
    EXPECT_TRUE(hasDirectLine);
    EXPECT_FALSE(hasSymmetry);
}

TEST(SymmetryGroup, MirroredSketchEvaluatesToBothLines)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = LastCurveId(wb);

    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 8, 2 });
    wb.FinishTool();

    wb.StartTool(ToolId::StopSymmetry);
    wb.StartTool(ToolId::FinishSketch);

    const PartDocument& part = wb.Evaluated();
    ASSERT_EQ(part.sketches.size(), 1u);
    EXPECT_EQ(part.sketches[0].entities.size(), 3u); // axis + original + twin
    EXPECT_EQ(part.sketches[0].constraints.size(), 1u); // the symmetry link
}

// ── undo / redo ──────────────────────────────────────────────────────────────

TEST(Undo, RoundTrips)
{
    Workbench wb;
    AuthorSketch(wb, 1);
    AuthorSketch(wb, 1);
    ASSERT_EQ(wb.Evaluated().sketches.size(), 2u);

    ASSERT_TRUE(wb.Undo());
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);

    ASSERT_TRUE(wb.Undo());
    EXPECT_EQ(wb.Evaluated().sketches.size(), 0u);

    ASSERT_TRUE(wb.Redo());
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);

    ASSERT_TRUE(wb.Redo());
    EXPECT_EQ(wb.Evaluated().sketches.size(), 2u);
}

TEST(Undo, StopsAtTheBeginningAndEnd)
{
    Workbench wb;
    EXPECT_FALSE(wb.CanUndo());
    EXPECT_FALSE(wb.Undo());

    AuthorSketch(wb, 1);
    EXPECT_FALSE(wb.CanRedo());
    EXPECT_FALSE(wb.Redo());

    ASSERT_TRUE(wb.Undo());
    EXPECT_TRUE(wb.CanRedo());
    EXPECT_FALSE(wb.CanUndo());
}

TEST(Undo, DoesNotDestroyHistoryOnlyTheCursor)
{
    // Recompute-based undo: the command is still there, it is just not applied.
    Workbench wb;
    AuthorSketch(wb, 1);
    ASSERT_TRUE(wb.Undo());
    EXPECT_EQ(wb.Doc().Size(), 1u);
    EXPECT_EQ(wb.Doc().Cursor(), 0u);
}

TEST(Undo, CommittingAfterAnUndoDiscardsTheRedoBranch)
{
    Workbench wb;
    AuthorSketch(wb, 1);
    AuthorSketch(wb, 1);
    ASSERT_TRUE(wb.Undo());
    ASSERT_TRUE(wb.CanRedo());

    AuthorSketch(wb, 1); // a new future

    EXPECT_FALSE(wb.CanRedo());
    EXPECT_EQ(wb.Doc().Size(), 2u);
    EXPECT_EQ(wb.Evaluated().sketches.size(), 2u);
}

// ── replay determinism ───────────────────────────────────────────────────────

TEST(Replay, IsDeterministic)
{
    // execute() regenerates rather than patching, so replaying twice must be identical.
    Workbench wb;
    AuthorSketch(wb, 3);

    const PartDocument& first = wb.Evaluated();
    ASSERT_EQ(first.sketches.size(), 1u);
    std::vector<SketchEntity> snapshot = first.sketches[0].entities;

    wb.Doc().MarkDirtyFrom(0); // force a full recompute
    const PartDocument& second = wb.Evaluated();

    ASSERT_EQ(second.sketches[0].entities.size(), snapshot.size());
    for (u32 k = 0; k < snapshot.size(); ++k) {
        EXPECT_EQ(second.sketches[0].entities[k].id, snapshot[k].id);
        EXPECT_NEAR(second.sketches[0].entities[k].a.x, snapshot[k].a.x, kEps);
        EXPECT_NEAR(second.sketches[0].entities[k].b.x, snapshot[k].b.x, kEps);
    }
}

TEST(Replay, DoesNotAccumulateAcrossRecomputes)
{
    // The classic regeneration bug: append instead of rebuild, and entities double.
    Workbench wb;
    AuthorSketch(wb, 2);
    u32 n = static_cast<u32>(wb.Evaluated().sketches[0].entities.size());

    wb.Doc().MarkDirtyFrom(0);
    EXPECT_EQ(wb.Evaluated().sketches[0].entities.size(), n);
    wb.Doc().MarkDirtyFrom(0);
    EXPECT_EQ(wb.Evaluated().sketches[0].entities.size(), n);
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);
}

TEST(Replay, FeatureIdsAreStableAcrossRecompute)
{
    Workbench wb;
    AuthorSketch(wb, 2);
    FeatureId first = wb.Evaluated().sketches[0].entities[0].id;

    wb.Doc().MarkDirtyFrom(0);
    EXPECT_EQ(wb.Evaluated().sketches[0].entities[0].id, first);
}

// ── re-editing existing history ──────────────────────────────────────────────

TEST(EditMode, ReEnteringASketchDeepCopiesIt)
{
    Workbench wb;
    FeatureId sketch = AuthorSketch(wb, 2);

    ASSERT_TRUE(wb.EditFeature(sketch));
    EXPECT_EQ(wb.Contexts().CurrentKind(), ContextKind::Sketch);
    EXPECT_EQ(wb.Contexts().ActiveSketch()->children.size(), 2u);
    EXPECT_EQ(wb.Contexts().ActiveSketch()->featureId, sketch); // same identity
}

TEST(EditMode, ConfirmReplacesTheSlotRatherThanAppending)
{
    Workbench wb;
    FeatureId sketch = AuthorSketch(wb, 2);
    ASSERT_EQ(wb.Doc().Size(), 1u);

    ASSERT_TRUE(wb.EditFeature(sketch));
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 50 });
    wb.ActiveTool().AddPoint({ 10, 50 });
    wb.FinishTool();
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_EQ(wb.Doc().Size(), 1u); // replaced, NOT appended
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);
    EXPECT_EQ(wb.Evaluated().sketches[0].entities.size(), 3u);
    EXPECT_EQ(wb.Evaluated().sketches[0].id, sketch);
}

TEST(EditMode, CancellingAnEditIsFree)
{
    // The edit copy is discarded; the original was never touched, so nothing to undo.
    Workbench wb;
    FeatureId sketch = AuthorSketch(wb, 2);

    ASSERT_TRUE(wb.EditFeature(sketch));
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 50 });
    wb.ActiveTool().AddPoint({ 10, 50 });
    wb.FinishTool();

    ASSERT_TRUE(wb.Escape());

    EXPECT_EQ(wb.Doc().Size(), 1u);
    EXPECT_EQ(wb.Evaluated().sketches[0].entities.size(), 2u); // untouched
}

TEST(EditMode, EditingMarksTheSlotDirty)
{
    Workbench wb;
    FeatureId sketch = AuthorSketch(wb, 1);
    (void)wb.Evaluated(); // clean

    ASSERT_TRUE(wb.EditFeature(sketch));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_TRUE(wb.Doc().IsDirty());
    EXPECT_EQ(wb.Doc().FirstDirty(), 0u);
}

TEST(EditMode, EditingAMissingFeatureFails)
{
    Workbench wb;
    EXPECT_FALSE(wb.EditFeature(4242));
}

TEST(EditMode, DeletingAFeatureRemovesItsSlot)
{
    Workbench wb;
    FeatureId a = AuthorSketch(wb, 1);
    AuthorSketch(wb, 1);
    ASSERT_EQ(wb.Doc().Size(), 2u);

    ASSERT_TRUE(wb.DeleteFeature(a));
    EXPECT_EQ(wb.Doc().Size(), 1u);
    EXPECT_EQ(wb.Evaluated().sketches.size(), 1u);
}

// ── tools ────────────────────────────────────────────────────────────────────

TEST(ToolLayer, IsNotReadyUntilEveryDeclaredInputArrives)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    EXPECT_FALSE(wb.ActiveTool().Ready());
    EXPECT_EQ(wb.ActiveTool().PointsRemaining(), 2u);

    wb.ActiveTool().AddPoint({ 0, 0 });
    EXPECT_FALSE(wb.ActiveTool().Ready());
    EXPECT_EQ(wb.ActiveTool().PointsRemaining(), 1u);

    wb.ActiveTool().AddPoint({ 1, 1 });
    EXPECT_TRUE(wb.ActiveTool().Ready());
    EXPECT_EQ(wb.ActiveTool().PointsRemaining(), 0u);
}

TEST(ToolLayer, FinishingEarlyIsInert)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    EXPECT_FALSE(wb.FinishTool());
    EXPECT_TRUE(wb.Contexts().ActiveSketch()->children.empty());
}

TEST(ToolLayer, ExcessInputIsIgnoredNotAccumulated)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 1, 1 });
    wb.ActiveTool().AddPoint({ 9, 9 }); // one click too many
    EXPECT_EQ(wb.ActiveTool().Points().size(), 2u);
}

TEST(ToolLayer, ZeroLengthLineIsRejected)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 5 });
    wb.ActiveTool().AddPoint({ 5, 5 }); // a double-click, not a line
    wb.FinishTool();

    EXPECT_TRUE(wb.Contexts().ActiveSketch()->children.empty());
}

TEST(ToolLayer, CircleTakesCentreThenRim)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Circle);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 3, 4 }); // radius 5
    ASSERT_TRUE(wb.FinishTool());

    // The gesture is a compound of the centre point + the rim point + the circle. The
    // circle holds no radius — it references the two points, and radius = ‖centre − rim‖.
    const SketchCmd& gesture = wb.Contexts().ActiveSketch()->children[0];
    const CompoundSketchCmd* g = gesture.As<CompoundSketchCmd>();
    ASSERT_NE(g, nullptr);
    const SketchCircle* c = nullptr;
    for (const SketchCmd& child : g->children) {
        if (const SketchCircle* cc = child.As<SketchCircle>()) {
            c = cc;
        }
    }
    ASSERT_NE(c, nullptr);
    Geometry::Point2 centre = PointInit(gesture, c->a);
    Geometry::Point2 rim = PointInit(gesture, c->b);
    EXPECT_NEAR(std::hypot(rim.x - centre.x, rim.y - centre.y), 5.0, kEps);
}

TEST(ToolLayer, ArcTakesCentreStartAndEndOnTheRim)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Arc);
    wb.ActiveTool().AddPoint({ 0, 0 }); // centre
    wb.ActiveTool().AddPoint({ 5, 0 }); // start — fixes radius 5
    wb.ActiveTool().AddPoint({ 0, 9 }); // end direction (+Y); the raw click is off the rim
    ASSERT_TRUE(wb.FinishTool());

    const SketchCmd& gesture = wb.Contexts().ActiveSketch()->children[0];
    const CompoundSketchCmd* g = gesture.As<CompoundSketchCmd>();
    ASSERT_NE(g, nullptr);
    const SketchArc* a = nullptr;
    for (const SketchCmd& child : g->children) {
        if (const SketchArc* aa = child.As<SketchArc>()) {
            a = aa;
        }
    }
    ASSERT_NE(a, nullptr);

    Geometry::Point2 c = PointInit(gesture, a->center);
    Geometry::Point2 s = PointInit(gesture, a->start);
    Geometry::Point2 e = PointInit(gesture, a->end);
    // start fixes the radius; end is snapped ONTO that circle (equal radius), not left at
    // the raw third click — so the arc is a proper equal-radius arc from the first solve.
    EXPECT_NEAR(std::hypot(s.x - c.x, s.y - c.y), 5.0, kEps);
    EXPECT_NEAR(std::hypot(e.x - c.x, e.y - c.y), 5.0, kEps);
    EXPECT_NEAR(e.x, 0.0, kEps); // end direction was +Y -> (0, 5)
    EXPECT_NEAR(e.y, 5.0, kEps);
}

TEST(ToolLayer, EveryCommittedCommandGetsAUniqueFeatureId)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    std::vector<FeatureId> seen;
    for (u32 k = 0; k < 5; ++k) {
        wb.StartTool(ToolId::Line);
        wb.ActiveTool().AddPoint({ 0, static_cast<f64>(k) });
        wb.ActiveTool().AddPoint({ 1, static_cast<f64>(k) });
        wb.FinishTool();
        seen.push_back(LastCurveId(wb));
    }

    for (u32 i = 0; i < seen.size(); ++i) {
        EXPECT_NE(seen[i], kNullFeature);
        for (u32 j = i + 1; j < seen.size(); ++j) {
            EXPECT_NE(seen[i], seen[j]);
        }
    }
}

TEST(ToolLayer, SeedPointChainsFromThePreviousLine)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().SeedPoint({ 3, 3 });
    wb.ActiveTool().AddPoint({ 9, 9 });
    ASSERT_TRUE(wb.ActiveTool().Ready());
    ASSERT_TRUE(wb.FinishTool());

    const SketchCmd& gesture = wb.Contexts().ActiveSketch()->children[0];
    const SketchLine* l = LineIn(gesture);
    ASSERT_NE(l, nullptr);
    // The line's seed points are the seeded start and the clicked end.
    EXPECT_NEAR(PointInit(gesture, l->p0).x, 3.0, kEps);
    EXPECT_NEAR(PointInit(gesture, l->p1).x, 9.0, kEps);
}

// ── Workbench is movable (Scene lives in a vector) ───────────────────────────

TEST(Workbench, SurvivesBeingMoved)
{
    // Scenes live in a std::vector; a reallocation moves the Workbench. If Document's
    // back-pointers weren't rebound, this would read freed memory.
    std::vector<Workbench> scenes;
    scenes.reserve(1);
    scenes.emplace_back();
    AuthorSketch(scenes[0], 2);

    scenes.reserve(64); // forces a move
    scenes.emplace_back();

    ASSERT_EQ(scenes[0].Evaluated().sketches.size(), 1u);
    EXPECT_EQ(scenes[0].Evaluated().sketches[0].entities.size(), 2u);

    // And it still works after the move.
    AuthorSketch(scenes[0], 1);
    EXPECT_EQ(scenes[0].Evaluated().sketches.size(), 2u);
}

// ── constraint tools (tool -> command -> solved geometry) ────────────────────

namespace {

// Author a sketch with two lines, leaving the context open. Returns their ids.
std::pair<FeatureId, FeatureId> TwoLines(Workbench& wb)
{
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 }); // horizontal
    wb.FinishTool();
    FeatureId a = LastCurveId(wb);

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 5 });
    wb.ActiveTool().AddPoint({ 8, 3 }); // sloped
    wb.FinishTool();
    FeatureId b = LastCurveId(wb);
    return { a, b };
}

const SketchConstraintCmd* LastConstraint(Workbench& wb)
{
    return wb.Contexts().ActiveSketch()->children.back().As<SketchConstraintCmd>();
}

} // namespace

TEST(ConstraintTool, ParallelToolCreatesAParallelConstraint)
{
    Workbench wb;
    auto [a, b] = TwoLines(wb);
    ASSERT_TRUE(wb.StartTool(ToolId::Parallel));
    wb.ActiveTool().AddPick(a);
    wb.ActiveTool().AddPick(b);
    ASSERT_TRUE(wb.FinishTool());

    const SketchConstraintCmd* c = LastConstraint(wb);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, ConstraintKind::Parallel);
    EXPECT_EQ(c->a, a);
    EXPECT_EQ(c->b, b);
}

TEST(ConstraintTool, EveryTwoEntityConstraintToolIsAvailableInASketch)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    std::vector<ToolId> tools = wb.AvailableTools();
    for (ToolId t : { ToolId::Coincident, ToolId::Parallel, ToolId::Perpendicular,
             ToolId::Equal, ToolId::Tangent, ToolId::Ground }) {
        bool found = false;
        for (ToolId a : tools) {
            found = found || a == t;
        }
        EXPECT_TRUE(found) << "constraint tool missing from the sketch context";
    }
}

TEST(ConstraintTool, ParallelActuallyAlignsGeometryOnceSolved)
{
    // The full path: tool -> Parallel constraint -> hybrid solver (constraint present) ->
    // the two lines end up parallel in the evaluated geometry.
    Workbench wb;
    auto [a, b] = TwoLines(wb);
    wb.StartTool(ToolId::Parallel);
    wb.ActiveTool().AddPick(a);
    wb.ActiveTool().AddPick(b);
    wb.FinishTool();
    wb.StartTool(ToolId::FinishSketch);

    const SketchDocument& doc = wb.Evaluated().sketches.at(0);
    const SketchEntity* la = doc.Find(a);
    const SketchEntity* lb = doc.Find(b);
    ASSERT_NE(la, nullptr);
    ASSERT_NE(lb, nullptr);
    f64 cross = (la->b.x - la->a.x) * (lb->b.y - lb->a.y) - (la->b.y - la->a.y) * (lb->b.x - lb->a.x);
    EXPECT_NEAR(cross, 0.0, 1e-4);
}

TEST(ConstraintTool, GroundToolNeedsOnlyOnePick)
{
    Workbench wb;
    auto [a, b] = TwoLines(wb);
    (void)b;
    wb.StartTool(ToolId::Ground);
    EXPECT_FALSE(wb.ActiveTool().Ready());
    wb.ActiveTool().AddPick(a);
    EXPECT_TRUE(wb.ActiveTool().Ready());
    ASSERT_TRUE(wb.FinishTool());
    EXPECT_EQ(LastConstraint(wb)->kind, ConstraintKind::Ground);
}

TEST(ConstraintTool, NoConstraintsKeepsGeometryAsDrawn)
{
    // A dimension-only sketch takes the DirectSketchSolver fast path (hybrid), so the
    // dimension-drag behaviour is unchanged from before the constraint solver existed.
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();
    FeatureId line = LastCurveId(wb);

    wb.StartTool(ToolId::Dimension);
    wb.ActiveTool().AddPick(line);
    wb.ActiveTool().SetValue("100mm");
    wb.FinishTool();
    wb.StartTool(ToolId::FinishSketch);

    // DirectSketchSolver extends from the start: a stays at origin, b at (100, 0).
    const SketchEntity* e = wb.Evaluated().sketches.at(0).Find(line);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(e->a.x, 0.0, 1e-9);
    EXPECT_NEAR(e->b.x, 100.0, 1e-9);
}

// ── auto-coincident on chained lines ─────────────────────────────────────────

namespace {

// Replicate the canvas's polyline chaining: draw the first line, then each subsequent
// segment via StartTool(Line)+SetChain(true)+SeedPoint, as UiCanvas does. Returns the
// committed line ids in order.
std::vector<FeatureId> DrawPolyline(Workbench& wb, const std::vector<Geometry::Point2>& pts)
{
    std::vector<FeatureId> lines;
    for (u32 k = 0; k + 1 < pts.size(); ++k) {
        wb.StartTool(ToolId::Line);
        if (k > 0) {
            wb.ActiveTool().SetChain(true); // canvas marks auto-restarted segments
            wb.ActiveTool().SeedPoint(pts[k]);
        } else {
            wb.ActiveTool().AddPoint(pts[k]);
        }
        wb.ActiveTool().AddPoint(pts[k + 1]);
        wb.FinishTool();
        // A chained segment commits the line gesture (a compound) AND the coincident; the
        // most recently authored curve is the line just drawn.
        lines.push_back(LastCurveId(wb));
    }
    return lines;
}

u32 CoincidentCount(const std::vector<SketchCmd>& children)
{
    u32 n = 0;
    for (const SketchCmd& c : children) {
        if (const auto* k = c.As<SketchConstraintCmd>()) {
            if (k->kind == ConstraintKind::Coincident) {
                ++n;
            }
        }
    }
    return n;
}

} // namespace

TEST(ChainedLines, FirstSegmentGetsNoCoincident)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 10, 0 });
    wb.FinishTool();

    EXPECT_EQ(CoincidentCount(wb.Contexts().ActiveSketch()->children), 0u);
}

TEST(ChainedLines, EachChainedSegmentAddsACoincident)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    // A 3-segment polyline -> 3 lines + 2 joins.
    std::vector<FeatureId> lines = DrawPolyline(wb, { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } });
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(CoincidentCount(wb.Contexts().ActiveSketch()->children), 2u);
}

TEST(ChainedLines, TheCoincidentJoinsPrevEndToNewStart)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    std::vector<FeatureId> lines = DrawPolyline(wb, { { 0, 0 }, { 10, 0 }, { 10, 10 } });
    ASSERT_EQ(lines.size(), 2u);

    // Find the coincident and confirm it links line0's END point to line1's START point —
    // two distinct point ids, not a merge.
    const std::vector<SketchCmd>& children = wb.Contexts().ActiveSketch()->children;
    const SketchConstraintCmd* join = nullptr;
    for (const SketchCmd& c : children) {
        if (const auto* k = c.As<SketchConstraintCmd>()) {
            if (k->kind == ConstraintKind::Coincident) {
                join = k;
            }
        }
    }
    ASSERT_NE(join, nullptr);
    const SketchLine* l0 = FindLineCmd(children, lines[0]);
    const SketchLine* l1 = FindLineCmd(children, lines[1]);
    ASSERT_NE(l0, nullptr);
    ASSERT_NE(l1, nullptr);
    EXPECT_EQ(join->a, l0->p1); // previous segment's end point
    EXPECT_EQ(join->b, l1->p0); // this segment's start point
}

TEST(ChainedLines, DraggingOneSegmentCarriesTheJoinedNeighbour)
{
    // The whole point: with the auto-coincident, dimensioning the first line drags the
    // shared corner, so the second line's start follows.
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    std::vector<FeatureId> lines = DrawPolyline(wb, { { 0, 0 }, { 10, 0 }, { 10, 10 } });
    // Ground the first line's start + make it horizontal, then dimension it longer.
    SketchConstraintCmd g {};
    g.kind = ConstraintKind::Ground;
    g.a = lines[0];
    wb.Contexts().Commit(wb.Doc(), SketchCmd { g });
    SketchConstraintCmd h {};
    h.kind = ConstraintKind::Horizontal;
    h.a = lines[0];
    wb.Contexts().Commit(wb.Doc(), SketchCmd { h });

    Param::UPID v = wb.Params().CreateDimension("40mm", wb.Contexts().ActiveSketch()->featureId);
    SketchDimensionCmd d {};
    d.kind = DimensionKind::Length;
    d.targetA = lines[0];
    d.value = v;
    wb.Contexts().Commit(wb.Doc(), SketchCmd { d });

    wb.StartTool(ToolId::FinishSketch);

    const SketchDocument& doc = wb.Evaluated().sketches.at(0);
    const SketchEntity* l0 = doc.Find(lines[0]);
    const SketchEntity* l1 = doc.Find(lines[1]);
    ASSERT_NE(l0, nullptr);
    ASSERT_NE(l1, nullptr);
    // line0 is now 40 long along +X from the origin...
    EXPECT_NEAR(l0->b.x, 40.0, 1e-4);
    // ...and the coincident dragged line1's start to that same corner.
    EXPECT_NEAR(l1->a.x, l0->b.x, 1e-4);
    EXPECT_NEAR(l1->a.y, l0->b.y, 1e-4);
}

TEST(ChainedLines, ANewSketchStartsAFreshChain)
{
    // The chain must not leak across a Finish Sketch: the first line of the next sketch
    // links to nothing.
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    DrawPolyline(wb, { { 0, 0 }, { 10, 0 }, { 10, 10 } });
    wb.StartTool(ToolId::FinishSketch);

    // Second sketch, one line.
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XZ);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 5, 5 });
    wb.FinishTool();

    EXPECT_EQ(CoincidentCount(wb.Contexts().ActiveSketch()->children), 0u);
}

// ── direct manipulation (canvas drag) ────────────────────────────────────────

namespace {

// Author one line in a fresh sketch, leaving the context open. Returns its id.
FeatureId OpenSketchWithLine(Workbench& wb, Geometry::Point2 a, Geometry::Point2 b)
{
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint(a);
    wb.ActiveTool().AddPoint(b);
    wb.FinishTool();
    return LastCurveId(wb);
}

} // namespace

TEST(Drag, MovingAFreeEndpointFollowsTheCursorExactly)
{
    Workbench wb;
    FeatureId line = OpenSketchWithLine(wb, { 0, 0 }, { 10, 0 });

    ASSERT_TRUE(wb.MoveLinePoint(line, PointRef::End, { 20, 7 }));

    SketchDocument doc;
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    const SketchEntity* e = doc.Find(line);
    ASSERT_NE(e, nullptr);
    // Unconstrained -> the point goes exactly where dragged.
    EXPECT_NEAR(e->b.x, 20.0, 1e-6);
    EXPECT_NEAR(e->b.y, 7.0, 1e-6);
    EXPECT_NEAR(e->a.x, 0.0, 1e-6); // the other end stayed put
}

TEST(Drag, TranslatingALineMovesBothEndpoints)
{
    Workbench wb;
    FeatureId line = OpenSketchWithLine(wb, { 0, 0 }, { 10, 0 });

    ASSERT_TRUE(wb.TranslateLine(line, { 3, 4 }));

    SketchDocument doc;
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    const SketchEntity* e = doc.Find(line);
    EXPECT_NEAR(e->a.x, 3.0, 1e-6);
    EXPECT_NEAR(e->a.y, 4.0, 1e-6);
    EXPECT_NEAR(e->b.x, 13.0, 1e-6);
    EXPECT_NEAR(e->b.y, 4.0, 1e-6);
}

TEST(Drag, DraggingAConstrainedEndpointRespectsTheConstraint)
{
    // A length-dimensioned line: dragging its end toward the cursor keeps the length, so
    // the point follows only as far as the constraint allows (min-norm to the drag).
    Workbench wb;
    FeatureId line = OpenSketchWithLine(wb, { 0, 0 }, { 10, 0 });

    // Ground the start and fix length to 10, then drag the end far away.
    SketchConstraintCmd g {};
    g.kind = ConstraintKind::Ground;
    g.a = line;
    wb.Contexts().Commit(wb.Doc(), SketchCmd { g });
    Param::UPID v = wb.Params().CreateDimension("10mm", wb.Contexts().ActiveSketch()->featureId);
    SketchDimensionCmd d {};
    d.kind = DimensionKind::Length;
    d.targetA = line;
    d.value = v;
    wb.Contexts().Commit(wb.Doc(), SketchCmd { d });

    ASSERT_TRUE(wb.MoveLinePoint(line, PointRef::End, { 100, 100 }));

    SketchDocument doc;
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    const SketchEntity* e = doc.Find(line);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(e->a.x, 0.0, 1e-4); // grounded start held
    EXPECT_NEAR(e->a.y, 0.0, 1e-4);
    EXPECT_NEAR(std::hypot(e->b.x - e->a.x, e->b.y - e->a.y), 10.0, 1e-4); // length held
    // ...but it rotated toward the drag target (up-right), not left at (10,0).
    EXPECT_GT(e->b.y, 1.0);
}

TEST(Drag, MovingANonExistentLineFails)
{
    Workbench wb;
    OpenSketchWithLine(wb, { 0, 0 }, { 10, 0 });
    EXPECT_FALSE(wb.MoveLinePoint(4242, PointRef::End, { 1, 1 }));
    EXPECT_FALSE(wb.TranslateLine(4242, { 1, 1 }));
}

TEST(Drag, FailsGracefullyWithNoActiveSketch)
{
    Workbench wb; // at Root, no sketch
    EXPECT_FALSE(wb.MoveLinePoint(1, PointRef::End, { 1, 1 }));
}
