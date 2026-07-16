// Dimensions: the line between geometry that is MEASURED and geometry that is DICTATED.
//
// The requirement these pin down: an entity has no size descriptor until a dimension is
// applied. An unconstrained line can always be asked for its length, but nothing drives
// it. Applying a dimension is what makes the length dictate the geometry instead of the
// other way round.

#include "DirectSketchSolver.h"
#include "Document.h"
#include "SketchDocument.h"
#include "Tool.h"
#include "Workbench.h"

#include <gtest/gtest.h>
#include <cmath>

namespace {

constexpr f64 kEps = 1e-6;

// Author a sketch containing a single horizontal line of length `len` from the origin,
// leaving the sketch context OPEN. Returns the line's FeatureId.
FeatureId BeginSketchWithLine(Workbench& wb, f64 len = 10.0)
{
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ len, 0 });
    wb.FinishTool();

    return wb.Contexts().ActiveSketch()->children.back().Get().id;
}

bool DimensionLine(Workbench& wb, FeatureId line, std::string_view expr)
{
    if (!wb.StartTool(ToolId::Dimension)) {
        return false;
    }
    wb.ActiveTool().AddPick(line);
    wb.ActiveTool().SetValue(expr);
    return wb.FinishTool();
}

const SketchDocument& OnlySketch(Workbench& wb)
{
    return wb.Evaluated().sketches.at(0);
}

f64 LengthOf(const SketchDocument& doc, FeatureId id)
{
    const SketchEntity* e = doc.Find(id);
    if (!e) {
        return -1.0;
    }
    f64 dx = e->b.x - e->a.x;
    f64 dy = e->b.y - e->a.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

// ── measured vs. dictated ────────────────────────────────────────────────────

TEST(Dimension, UndimensionedLineCanBeMeasuredButIsNotDriven)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    wb.StartTool(ToolId::FinishSketch);

    const SketchDocument& doc = OnlySketch(wb);

    // Measurable...
    DTL::Optional<f64> len = doc.MeasureLength(line);
    ASSERT_TRUE(len.has_value());
    EXPECT_NEAR(*len, 10.0, kEps);

    // ...but nothing dictates it.
    EXPECT_FALSE(doc.IsDriven(line));
    EXPECT_TRUE(doc.dimensions.empty());
}

TEST(Dimension, ApplyingOneMakesTheEntityDriven)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));
    wb.StartTool(ToolId::FinishSketch);

    const SketchDocument& doc = OnlySketch(wb);
    EXPECT_TRUE(doc.IsDriven(line));
    EXPECT_EQ(doc.dimensions.size(), 1u);
}

TEST(Dimension, MeasuringANonLineOrMissingEntityReturnsNothing)
{
    Workbench wb;
    BeginSketchWithLine(wb, 10.0);
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_FALSE(OnlySketch(wb).MeasureLength(9999).has_value());
    EXPECT_FALSE(OnlySketch(wb).MeasureRadius(9999).has_value());
}

// ── driving ──────────────────────────────────────────────────────────────────

TEST(Dimension, LengthMovesTheEndpointAlongTheLinesOwnDirection)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0); // drawn 10 long
    ASSERT_TRUE(DimensionLine(wb, line, "100mm")); // dictated 100
    wb.StartTool(ToolId::FinishSketch);

    const SketchDocument& doc = OnlySketch(wb);
    EXPECT_NEAR(LengthOf(doc, line), 100.0, kEps);

    // The start is untouched and the direction is preserved — only the length changed.
    const SketchEntity* e = doc.Find(line);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(e->a.x, 0.0, kEps);
    EXPECT_NEAR(e->a.y, 0.0, kEps);
    EXPECT_NEAR(e->b.x, 100.0, kEps);
    EXPECT_NEAR(e->b.y, 0.0, kEps);
}

TEST(Dimension, LengthPreservesADiagonalsAngle)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 3, 4 }); // length 5, angle atan2(4,3)
    wb.FinishTool();
    FeatureId line = wb.Contexts().ActiveSketch()->children.back().Get().id;

    ASSERT_TRUE(DimensionLine(wb, line, "10"));
    wb.StartTool(ToolId::FinishSketch);

    const SketchEntity* e = OnlySketch(wb).Find(line);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 10.0, kEps);
    // Same direction, doubled: (3,4) -> (6,8).
    EXPECT_NEAR(e->b.x, 6.0, kEps);
    EXPECT_NEAR(e->b.y, 8.0, kEps);
}

TEST(Dimension, UnitConversionAppliesToDrivenGeometry)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "2in")); // 50.8mm
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 50.8, kEps);
}

TEST(Dimension, MixedUnitExpressionDrivesGeometry)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "10mm + 2in"));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 60.8, kEps);
}

TEST(Dimension, ImperialSymbolsDriveGeometry)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "5\""));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 127.0, kEps);
}

TEST(Dimension, FeetAndInchesDriveGeometry)
{
    // The workflow the notation exists for: a deck board dimensioned five foot six.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "5' 6\""));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 1676.4, kEps);
}

TEST(Dimension, ImperialExpressionCanReferenceAParameter)
{
    Workbench wb;
    Param::UPID span = wb.Params().Create("span", "8'");
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$span - 3\""));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 2438.4 - 76.2, kEps);

    ASSERT_TRUE(wb.Params().SetExpression(span, "10'"));
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 3048.0 - 76.2, kEps);
}

TEST(Dimension, ABareNumberIsTakenInTheBaseUnit)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "42"));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 42.0, kEps);
}

TEST(Dimension, AnUndimensionedLineIsLeftExactlyAsDrawn)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 7, 0 });
    wb.FinishTool();
    FeatureId free = wb.Contexts().ActiveSketch()->children.back().Get().id;

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 5 });
    wb.ActiveTool().AddPoint({ 9, 5 });
    wb.FinishTool();
    FeatureId driven = wb.Contexts().ActiveSketch()->children.back().Get().id;

    ASSERT_TRUE(DimensionLine(wb, driven, "100"));
    wb.StartTool(ToolId::FinishSketch);

    // The dimensioned one moved; its neighbour did not.
    EXPECT_NEAR(LengthOf(OnlySketch(wb), driven), 100.0, kEps);
    EXPECT_NEAR(LengthOf(OnlySketch(wb), free), 7.0, kEps);
}

// ── parametric dimensions ────────────────────────────────────────────────────

TEST(Dimension, ExpressionCanReferenceAUserParameter)
{
    Workbench wb;
    wb.Params().Create("w", "100mm");

    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$w * 2"));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 200.0, kEps);
}

TEST(Dimension, EditingAParameterReDrivesTheGeometry)
{
    // The headline parametric workflow.
    Workbench wb;
    Param::UPID w = wb.Params().Create("w", "100mm");

    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$w * 2"));
    wb.StartTool(ToolId::FinishSketch);
    ASSERT_NEAR(LengthOf(OnlySketch(wb), line), 200.0, kEps);

    ASSERT_TRUE(wb.Params().SetExpression(w, "150mm"));

    // No explicit invalidation: the document notices the parameter generation moved.
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 300.0, kEps);
}

TEST(Dimension, TransitiveParameterEditReDrivesGeometry)
{
    Workbench wb;
    Param::UPID base = wb.Params().Create("base", "10mm");
    wb.Params().Create("derived", "$base * 3");

    FeatureId line = BeginSketchWithLine(wb, 1.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$derived"));
    wb.StartTool(ToolId::FinishSketch);
    ASSERT_NEAR(LengthOf(OnlySketch(wb), line), 30.0, kEps);

    ASSERT_TRUE(wb.Params().SetExpression(base, "20mm"));
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 60.0, kEps);
}

TEST(Dimension, DimensionValueIsOwnedByItsSketch)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    FeatureId sketch = wb.Contexts().ActiveSketch()->featureId;
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));
    wb.StartTool(ToolId::FinishSketch);

    const SketchDocument& doc = OnlySketch(wb);
    ASSERT_EQ(doc.dimensions.size(), 1u);

    const Param::ParametricExpression* p = wb.Params().Get(doc.dimensions[0].value);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->IsDimension());
    EXPECT_EQ(p->Owner(), sketch);
}

TEST(Dimension, DeletingASketchTakesItsDimensionValuesWithIt)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    FeatureId sketch = wb.Contexts().ActiveSketch()->featureId;
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));
    wb.StartTool(ToolId::FinishSketch);
    ASSERT_EQ(wb.Params().Parameters().size(), 1u);

    ASSERT_TRUE(wb.DeleteFeature(sketch));
    EXPECT_EQ(wb.Params().Parameters().size(), 0u);
}

TEST(Dimension, DeletingASketchLeavesUserParametersAlone)
{
    Workbench wb;
    wb.Params().Create("w", "100mm");

    FeatureId line = BeginSketchWithLine(wb, 10.0);
    FeatureId sketch = wb.Contexts().ActiveSketch()->featureId;
    ASSERT_TRUE(DimensionLine(wb, line, "$w"));
    wb.StartTool(ToolId::FinishSketch);

    ASSERT_TRUE(wb.DeleteFeature(sketch));
    EXPECT_NE(wb.Params().FindByName("w"), Param::kNullUpid);
}

// ── robustness ───────────────────────────────────────────────────────────────

TEST(Dimension, ABrokenExpressionLeavesGeometryAsDrawnRatherThanCollapsingIt)
{
    // Mid-edit an expression is transiently invalid. Defaulting to zero would collapse
    // the model under the user; skipping keeps it where it was.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$doesNotExist"));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 10.0, kEps);
}

TEST(Dimension, ACyclicExpressionLeavesGeometryAsDrawn)
{
    Workbench wb;
    wb.Params().Create("a", "$b");
    wb.Params().Create("b", "$a");

    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$a"));
    wb.StartTool(ToolId::FinishSketch);

    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 10.0, kEps);
}

TEST(Dimension, DrivenGeometrySurvivesRecomputeIdentically)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));
    wb.StartTool(ToolId::FinishSketch);
    ASSERT_NEAR(LengthOf(OnlySketch(wb), line), 100.0, kEps);

    // Solving must be idempotent: a second pass must not compound the first.
    wb.Doc().MarkDirtyFrom(0);
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 100.0, kEps);
    wb.Doc().MarkDirtyFrom(0);
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 100.0, kEps);
}

TEST(Dimension, UndoRemovesTheWholeDimensionedSketchInOneStep)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));
    wb.StartTool(ToolId::FinishSketch);
    ASSERT_EQ(wb.Evaluated().sketches.size(), 1u);

    ASSERT_TRUE(wb.Undo());
    EXPECT_TRUE(wb.Evaluated().sketches.empty());
}

TEST(Dimension, ToolRefusesWithoutAValue)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);

    wb.StartTool(ToolId::Dimension);
    wb.ActiveTool().AddPick(line);
    EXPECT_FALSE(wb.ActiveTool().Ready()); // no value typed yet
    EXPECT_FALSE(wb.FinishTool());

    wb.ActiveTool().SetValue("50mm");
    EXPECT_TRUE(wb.ActiveTool().Ready());
    EXPECT_TRUE(wb.FinishTool());
}

TEST(Dimension, ADimensionOnAMissingTargetFailsExecutionNotTheProcess)
{
    SketchDocument doc;
    SketchDimensionCmd d {};
    d.id = 1;
    d.targetA = 4242; // never drawn
    d.value = 0;

    ExecResult r = d.execute(doc);
    EXPECT_FALSE(r.Ok());
    EXPECT_EQ(r.status, ExecStatus::MissingReference);
}

// ── live preview: the sketch updates without exiting it ──────────────────────

TEST(SketchPreview, ReflectsAnAppliedDimensionWithoutFinishingTheSketch)
{
    // The canvas draws BuildSketchPreview, so this is what "the sketch updates when the
    // dimension is applied" actually means.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);

    SketchDocument before;
    ASSERT_TRUE(wb.BuildSketchPreview(before));
    EXPECT_NEAR(*before.MeasureLength(line), 10.0, kEps);

    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));

    SketchDocument after;
    ASSERT_TRUE(wb.BuildSketchPreview(after));
    EXPECT_NEAR(*after.MeasureLength(line), 100.0, kEps);

    // Still authoring: nothing was committed to reach that state.
    EXPECT_NE(wb.Contexts().ActiveSketch(), nullptr);
    EXPECT_EQ(wb.Doc().Size(), 0u);
}

TEST(SketchPreview, TracksAParameterEditWhileStillInTheSketch)
{
    Workbench wb;
    Param::UPID w = wb.Params().Create("w", "20mm");
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "$w"));

    SketchDocument doc;
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    EXPECT_NEAR(*doc.MeasureLength(line), 20.0, kEps);

    ASSERT_TRUE(wb.Params().SetExpression(w, "70mm"));
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    EXPECT_NEAR(*doc.MeasureLength(line), 70.0, kEps);
}

TEST(SketchPreview, IsEmptyWhenNoSketchIsBeingAuthored)
{
    Workbench wb;
    SketchDocument doc;
    EXPECT_FALSE(wb.BuildSketchPreview(doc));
}

TEST(SketchPreview, DoesNotAccumulateAcrossRebuilds)
{
    Workbench wb;
    BeginSketchWithLine(wb, 10.0);

    SketchDocument doc;
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    u32 n = static_cast<u32>(doc.entities.size());
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    EXPECT_EQ(doc.entities.size(), n);
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    EXPECT_EQ(doc.entities.size(), n);
}

TEST(SketchPreview, KeepsDrawingWhatWorksWhenOneCommandIsBroken)
{
    // Mid-edit a command is transiently broken; blanking the sketch under the user is
    // worse than drawing everything that still resolves.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);

    // A dimension pointing at an entity that does not exist.
    Param::UPID v = wb.Params().CreateDimension("5mm", wb.Contexts().ActiveSketch()->featureId);
    SketchDimensionCmd bad {};
    bad.kind = DimensionKind::Length;
    bad.targetA = 4242;
    bad.value = v;
    ASSERT_TRUE(wb.Contexts().Commit(wb.Doc(), SketchCmd { bad }));

    SketchDocument doc;
    ASSERT_TRUE(wb.BuildSketchPreview(doc));
    // The good line still renders despite the broken dimension ahead of it.
    ASSERT_NE(doc.Find(line), nullptr);
    EXPECT_NEAR(*doc.MeasureLength(line), 10.0, kEps);
}

TEST(SketchPreview, MatchesTheCommittedResult)
{
    // The preview must not be a different evaluation from the real one, or geometry
    // would jump the moment the sketch is finished.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "42mm"));

    SketchDocument preview;
    ASSERT_TRUE(wb.BuildSketchPreview(preview));
    f64 previewLen = *preview.MeasureLength(line);

    wb.StartTool(ToolId::FinishSketch);
    f64 committedLen = *OnlySketch(wb).MeasureLength(line);

    EXPECT_NEAR(previewLen, committedLen, kEps);
    EXPECT_NEAR(committedLen, 42.0, kEps);
}

// ── the solver seam ──────────────────────────────────────────────────────────

TEST(SolverSeam, NullSolverLeavesDimensionsRecordedButInert)
{
    // Proves the seam: same commands, different solver, different geometry — and
    // nothing else in the system had to change.
    Param::ParameterEngine params;
    NullSketchSolver null;
    Document doc { &params, &null };

    Param::UPID v = params.Create("len", "100mm");

    SketchLine l {};
    l.id = 10;
    l.a = { 0, 0 };
    l.b = { 10, 0 };

    SketchDimensionCmd d {};
    d.id = 11;
    d.kind = DimensionKind::Length;
    d.targetA = 10;
    d.value = v;

    SketchFeatureCommand f {};
    f.id = 1;
    f.plane = Geometry::SketchPlane::XY;
    f.children.push_back(SketchCmd { l });
    f.children.push_back(SketchCmd { d });
    doc.PushCommand(Command { std::move(f) });

    ASSERT_EQ(doc.Evaluated().sketches.size(), 1u);
    // Recorded...
    EXPECT_EQ(doc.Evaluated().sketches[0].dimensions.size(), 1u);
    // ...but not applied: still as drawn.
    EXPECT_NEAR(LengthOf(doc.Evaluated().sketches[0], 10), 10.0, kEps);

    // Swap in the real solver and the same history now obeys the dimension.
    DirectSketchSolver direct;
    doc.SetSolver(&direct);
    EXPECT_NEAR(LengthOf(doc.Evaluated().sketches[0], 10), 100.0, kEps);
}

TEST(SolverSeam, RadiusDrivesACircle)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    wb.StartTool(ToolId::Circle);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 5, 0 }); // drawn radius 5
    wb.FinishTool();
    FeatureId circle = wb.Contexts().ActiveSketch()->children.back().Get().id;

    // The Dimension tool defaults to Length; drive the radius through the command
    // directly, which is the same commit path the tool uses.
    Param::UPID v = wb.Params().CreateDimension("25mm", wb.Contexts().ActiveSketch()->featureId);
    SketchDimensionCmd d {};
    d.kind = DimensionKind::Radius;
    d.targetA = circle;
    d.value = v;
    ASSERT_TRUE(wb.Contexts().Commit(wb.Doc(), SketchCmd { d }));
    wb.StartTool(ToolId::FinishSketch);

    const SketchEntity* e = OnlySketch(wb).Find(circle);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(e->radius, 25.0, kEps);
}

TEST(SolverSeam, AngleRotatesALinePreservingItsLength)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0); // along +X

    Param::UPID v = wb.Params().CreateDimension("90deg", wb.Contexts().ActiveSketch()->featureId);
    SketchDimensionCmd d {};
    d.kind = DimensionKind::Angle;
    d.targetA = line;
    d.value = v;
    ASSERT_TRUE(wb.Contexts().Commit(wb.Doc(), SketchCmd { d }));
    wb.StartTool(ToolId::FinishSketch);

    const SketchEntity* e = OnlySketch(wb).Find(line);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(LengthOf(OnlySketch(wb), line), 10.0, kEps); // length preserved
    EXPECT_NEAR(e->b.x, 0.0, kEps); // rotated to +Y
    EXPECT_NEAR(e->b.y, 10.0, kEps);
}
