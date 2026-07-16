// What a dimension actually renders: its dimension line, and its label.
//
// The requirement is that a dimension shows "the expression and the result". These pin
// the label composition and the dimension-line geometry, both of which are pure logic
// over the document + parameter table. Whether the label FITS on the line is decided in
// the canvas (it needs real font metrics) and is not covered here.

#include "DimensionVisual.h"
#include "Tool.h"
#include "Workbench.h"

#include <gtest/gtest.h>
#include <cmath>
#include <string>

using namespace AppUi;

namespace {

constexpr f64 kEps = 1e-6;

// A sketch with one horizontal line from the origin, left open.
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

std::vector<DimensionVisual> VisualsFor(Workbench& wb, Param::Unit display = Param::Unit::Millimeter)
{
    SketchDocument doc;
    if (!wb.BuildSketchPreview(doc)) {
        return {};
    }
    return BuildDimensionVisuals(doc, wb.Params(), display);
}

bool Contains(const std::string& s, std::string_view needle)
{
    return s.find(needle) != std::string::npos;
}

} // namespace

// ── the label ────────────────────────────────────────────────────────────────

TEST(DimensionVisual, NoneWithoutDimensions)
{
    Workbench wb;
    BeginSketchWithLine(wb);
    EXPECT_TRUE(VisualsFor(wb).empty());
}

TEST(DimensionVisual, ALiteralAlreadyInTheDisplayUnitShowsJustItsValue)
{
    // "100mm = 100mm" would be tautological — the expression IS the result.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));

    std::vector<DimensionVisual> v = VisualsFor(wb, Param::Unit::Millimeter);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(v[0].ok);
    EXPECT_EQ(v[0].label, "100mm");
    EXPECT_FALSE(Contains(v[0].label, "="));
}

TEST(DimensionVisual, AConvertedLiteralShowsBothHalvesEvenWithNoOperator)
{
    // `2in` has no operator in it, but `50.8mm` is not what was typed — the conversion
    // is exactly what the label is for.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "2in"));

    std::vector<DimensionVisual> v = VisualsFor(wb, Param::Unit::Millimeter);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].label, "2in = 50.8mm");
}

TEST(DimensionVisual, FeetAndInchesShowsWhatWasTypedAndWhatItMeans)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "5' 6\""));

    std::vector<DimensionVisual> mm = VisualsFor(wb, Param::Unit::Millimeter);
    ASSERT_EQ(mm.size(), 1u);
    EXPECT_EQ(mm[0].label, "5' 6\" = 1676.4mm");

    // Displayed in inches it is 66in — still not what was typed, so still both halves.
    std::vector<DimensionVisual> inch = VisualsFor(wb, Param::Unit::Inch);
    ASSERT_EQ(inch.size(), 1u);
    EXPECT_EQ(inch[0].label, "5' 6\" = 66in");
}

TEST(DimensionVisual, ALiteralMatchingItsDisplayUnitCollapsesInAnyUnit)
{
    // The collapse rule is about the expression matching the RENDERED value, so it
    // follows the display unit rather than being hardcoded to mm.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "4in"));

    // Displayed as inches: "4in" == "4in" -> collapses.
    std::vector<DimensionVisual> inch = VisualsFor(wb, Param::Unit::Inch);
    ASSERT_EQ(inch.size(), 1u);
    EXPECT_EQ(inch[0].label, "4in");

    // Displayed as mm: "4in" != "101.6mm" -> both halves.
    std::vector<DimensionVisual> mm = VisualsFor(wb, Param::Unit::Millimeter);
    ASSERT_EQ(mm.size(), 1u);
    EXPECT_EQ(mm[0].label, "4in = 101.6mm");
}

TEST(DimensionVisual, AComputedExpressionShowsBothExpressionAndResult)
{
    Workbench wb;
    wb.Params().Create("w", "100mm");
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "$w * 2"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(v[0].ok);
    EXPECT_EQ(v[0].label, "$w * 2 = 200mm");
}

TEST(DimensionVisual, APlainReferenceShowsBothHalves)
{
    Workbench wb;
    wb.Params().Create("w", "42mm");
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "$w"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].label, "$w = 42mm");
}

TEST(DimensionVisual, ACallShowsBothHalves)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "@Min(30mm, 80mm)"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].label, "@Min(30mm, 80mm) = 30mm");
}

TEST(DimensionVisual, ArithmeticOnLiteralsCounts)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "10mm + 2in"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].label, "10mm + 2in = 60.8mm");
}

TEST(DimensionVisual, LabelFollowsAParameterEdit)
{
    Workbench wb;
    Param::UPID w = wb.Params().Create("w", "10mm");
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "$w * 2"));
    ASSERT_EQ(VisualsFor(wb)[0].label, "$w * 2 = 20mm");

    ASSERT_TRUE(wb.Params().SetExpression(w, "50mm"));
    EXPECT_EQ(VisualsFor(wb)[0].label, "$w * 2 = 100mm");
}

TEST(DimensionVisual, LabelRendersInTheScenesDisplayUnit)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "2 * 50.8mm")); // 101.6mm == 4in

    std::vector<DimensionVisual> mm = VisualsFor(wb, Param::Unit::Millimeter);
    ASSERT_EQ(mm.size(), 1u);
    EXPECT_TRUE(Contains(mm[0].label, "101.6mm")) << mm[0].label;

    // Same dimension, same expression — only the presentation changes.
    std::vector<DimensionVisual> inch = VisualsFor(wb, Param::Unit::Inch);
    ASSERT_EQ(inch.size(), 1u);
    EXPECT_TRUE(Contains(inch[0].label, "4in")) << inch[0].label;
    EXPECT_TRUE(Contains(inch[0].label, "2 * 50.8mm")) << inch[0].label;
}

TEST(DimensionVisual, ABrokenExpressionIsMarkedNotOkAndStillLabelled)
{
    // The dimension must remain visible while its expression is being fixed, or the
    // user loses the thing they are trying to correct.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "$ghost"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_FALSE(v[0].ok);
    EXPECT_EQ(v[0].label, "$ghost = ?");
}

TEST(DimensionVisual, ACyclicExpressionIsMarkedNotOk)
{
    Workbench wb;
    wb.Params().Create("a", "$b");
    wb.Params().Create("b", "$a");
    FeatureId line = BeginSketchWithLine(wb);
    ASSERT_TRUE(DimensionLine(wb, line, "$a"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_FALSE(v[0].ok);
}

// ── the dimension line ───────────────────────────────────────────────────────

TEST(DimensionVisual, DimensionLineIsOffsetOffTheEntityItMeasures)
{
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0); // along +X at y = 0
    ASSERT_TRUE(DimensionLine(wb, line, "10mm"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);

    // Extension lines anchor on the entity's own endpoints...
    EXPECT_NEAR(v[0].extA.x, 0.0, kEps);
    EXPECT_NEAR(v[0].extA.y, 0.0, kEps);
    EXPECT_NEAR(v[0].extB.x, 10.0, kEps);

    // ...and the dimension line sits off it along the normal, not on top of it.
    EXPECT_NEAR(v[0].a.y, DIM_OFFSET, kEps);
    EXPECT_NEAR(v[0].b.y, DIM_OFFSET, kEps);
    EXPECT_NEAR(v[0].a.x, 0.0, kEps);
    EXPECT_NEAR(v[0].b.x, 10.0, kEps);
}

TEST(DimensionVisual, DimensionLineTracksTheSolvedGeometryNotTheDrawnGeometry)
{
    // The line was drawn 10 long and dimensioned to 100. The dimension line must span
    // the SOLVED length — otherwise it annotates a line that is not there.
    Workbench wb;
    FeatureId line = BeginSketchWithLine(wb, 10.0);
    ASSERT_TRUE(DimensionLine(wb, line, "100mm"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);
    f64 span = std::sqrt((v[0].b.x - v[0].a.x) * (v[0].b.x - v[0].a.x)
        + (v[0].b.y - v[0].a.y) * (v[0].b.y - v[0].a.y));
    EXPECT_NEAR(span, 100.0, kEps);
    EXPECT_NEAR(v[0].extB.x, 100.0, kEps);
}

TEST(DimensionVisual, DimensionLineIsParallelToADiagonal)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 3, 4 }); // length 5
    wb.FinishTool();
    FeatureId line = wb.Contexts().ActiveSketch()->children.back().Get().id;
    ASSERT_TRUE(DimensionLine(wb, line, "5mm"));

    std::vector<DimensionVisual> v = VisualsFor(wb);
    ASSERT_EQ(v.size(), 1u);

    // The dimension line runs parallel to the entity (cross product ~ 0)...
    f64 ex = 3.0;
    f64 ey = 4.0;
    f64 dx = v[0].b.x - v[0].a.x;
    f64 dy = v[0].b.y - v[0].a.y;
    EXPECT_NEAR(ex * dy - ey * dx, 0.0, 1e-6);

    // ...and is offset perpendicular by exactly DIM_OFFSET.
    f64 offx = v[0].a.x - v[0].extA.x;
    f64 offy = v[0].a.y - v[0].extA.y;
    EXPECT_NEAR(std::sqrt(offx * offx + offy * offy), DIM_OFFSET, kEps);
    EXPECT_NEAR(ex * offx + ey * offy, 0.0, 1e-6); // perpendicular to the entity
}

TEST(DimensionVisual, SkipsADimensionWhoseTargetIsGone)
{
    // A dangling dimension must not produce a visual anchored at the origin.
    SketchDocument doc;
    Param::ParameterEngine params;
    Param::UPID v = params.Create("x", "10mm");

    SketchDimensionRecord d {};
    d.id = 1;
    d.kind = DimensionKind::Length;
    d.targetA = 4242; // never drawn
    d.value = v;
    doc.dimensions.push_back(d);
    doc.params = &params;

    EXPECT_TRUE(BuildDimensionVisuals(doc, params, Param::Unit::Millimeter).empty());
}

TEST(DimensionVisual, OneVisualPerDimension)
{
    Workbench wb;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();

    for (u32 k = 0; k < 3; ++k) {
        wb.StartTool(ToolId::Line);
        wb.ActiveTool().AddPoint({ 0, static_cast<f64>(k) * 5.0 });
        wb.ActiveTool().AddPoint({ 10, static_cast<f64>(k) * 5.0 });
        wb.FinishTool();
        FeatureId id = wb.Contexts().ActiveSketch()->children.back().Get().id;
        ASSERT_TRUE(DimensionLine(wb, id, std::to_string((k + 1) * 10) + "mm"));
    }

    EXPECT_EQ(VisualsFor(wb).size(), 3u);
}
