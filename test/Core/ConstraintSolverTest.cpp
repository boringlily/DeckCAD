// The constraint solver: residuals, convergence, and degree-of-freedom analysis.
//
// Built directly on SketchDocument (no Workbench) so each test controls the exact
// entities/dimensions/constraints. Pure math over DTL.h — straight into core_tests.

#include "ConstraintSolver.h"
#include "ParameterEngine.h"
#include "SketchDocument.h"

#include <gtest/gtest.h>
#include <cmath>

namespace {

constexpr f64 kEps = 1e-4; // the solver converges to ~1e-5; assert a hair looser

SketchEntity Line(FeatureId id, f64 ax, f64 ay, f64 bx, f64 by)
{
    SketchEntity e;
    e.id = id;
    e.kind = EntityKind::Line;
    e.a = { ax, ay };
    e.b = { bx, by };
    return e;
}

SketchEntity Circle(FeatureId id, f64 cx, f64 cy, f64 r)
{
    SketchEntity e;
    e.id = id;
    e.kind = EntityKind::Circle;
    e.a = { cx, cy };
    e.radius = r;
    return e;
}

f64 Len(const SketchEntity& e)
{
    return std::hypot(e.b.x - e.a.x, e.b.y - e.a.y);
}

// Solve a document with the real solver and return its result.
SketchSolveResult Solve(SketchDocument& doc)
{
    ConstraintSketchSolver solver;
    solver.Solve(doc);
    return doc.lastSolve;
}

// A document that owns a parameter table, for dimension tests.
struct Doc {
    Param::ParameterEngine params;
    SketchDocument doc;
    Doc() { doc.params = &params; }

    // Add a dimension whose value is a literal expression.
    void Dimension(FeatureId id, DimensionKind kind, FeatureId targetA, std::string_view expr,
        FeatureId targetB = kNullFeature)
    {
        Param::UPID v = params.CreateDimension(expr, doc.id);
        SketchDimensionRecord d;
        d.id = id;
        d.kind = kind;
        d.targetA = targetA;
        d.targetB = targetB;
        d.value = v;
        doc.dimensions.push_back(d);
    }
};

SketchConstraintRecord Constraint(FeatureId id, ConstraintKind kind, FeatureId a,
    FeatureId b = kNullFeature, FeatureId axis = kNullFeature)
{
    SketchConstraintRecord c;
    c.id = id;
    c.kind = kind;
    c.a = a;
    c.b = b;
    c.axis = axis;
    return c;
}

} // namespace

// ── the measured-vs-dictated rule ────────────────────────────────────────────

TEST(ConstraintSolver, UnconstrainedGeometryIsLeftAsDrawn)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 2, 3, 12, 8));
    Solve(doc);

    const SketchEntity* e = doc.Find(1);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(e->a.x, 2, kEps);
    EXPECT_NEAR(e->a.y, 3, kEps);
    EXPECT_NEAR(e->b.x, 12, kEps);
    EXPECT_NEAR(e->b.y, 8, kEps);
}

TEST(ConstraintSolver, AFreeLineIsUnderConstrained)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    SketchSolveResult r = Solve(doc);
    EXPECT_EQ(r.status, SketchSolveStatus::UnderConstrained);
    EXPECT_GT(r.dof, 0u);
}

// ── dimensions ───────────────────────────────────────────────────────────────

TEST(ConstraintSolver, LengthDimensionAchievesTheLength)
{
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    d.Dimension(2, DimensionKind::Length, 1, "100mm");
    Solve(d.doc);
    EXPECT_NEAR(Len(*d.doc.Find(1)), 100.0, kEps);
}

TEST(ConstraintSolver, LengthPreservesDirection)
{
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 3, 4)); // length 5, slope 4/3
    d.Dimension(2, DimensionKind::Length, 1, "10mm");
    Solve(d.doc);
    const SketchEntity* e = d.doc.Find(1);
    EXPECT_NEAR(Len(*e), 10.0, kEps);
    // Same direction: the unit vector stays (0.6, 0.8).
    f64 ux = (e->b.x - e->a.x) / Len(*e);
    f64 uy = (e->b.y - e->a.y) / Len(*e);
    EXPECT_NEAR(ux, 0.6, kEps);
    EXPECT_NEAR(uy, 0.8, kEps);
}

TEST(ConstraintSolver, RadiusDimensionAchievesTheRadius)
{
    Doc d;
    d.doc.entities.push_back(Circle(1, 0, 0, 5));
    d.Dimension(2, DimensionKind::Radius, 1, "25mm");
    Solve(d.doc);
    EXPECT_NEAR(d.doc.Find(1)->radius, 25.0, kEps);
}

TEST(ConstraintSolver, AngleDimensionRotatesTheLine)
{
    // Angle alone doesn't pin length (that DOF stays free), so a real angle dimension is
    // used alongside a length + a ground — the well-constrained case.
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0)); // along +X
    d.doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1)); // pin the start
    d.Dimension(3, DimensionKind::Length, 1, "10mm");
    d.Dimension(4, DimensionKind::Angle, 1, "90deg");
    Solve(d.doc);
    const SketchEntity* e = d.doc.Find(1);
    f64 ang = std::atan2(e->b.y - e->a.y, e->b.x - e->a.x);
    EXPECT_NEAR(ang, Param::kPi / 2.0, kEps);
    EXPECT_NEAR(Len(*e), 10.0, kEps); // length held by its own dimension
}

TEST(ConstraintSolver, DistanceDimensionSeparatesTwoEntities)
{
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 1, 0));
    d.doc.entities.push_back(Circle(2, 3, 0, 1));
    d.Dimension(3, DimensionKind::Distance, 1, "50mm", 2);
    Solve(d.doc);
    // Distance between line's anchor (a) and circle centre == 50.
    const SketchEntity* l = d.doc.Find(1);
    const SketchEntity* c = d.doc.Find(2);
    EXPECT_NEAR(std::hypot(c->a.x - l->a.x, c->a.y - l->a.y), 50.0, kEps);
}

TEST(ConstraintSolver, ABrokenDimensionLeavesGeometryAsDrawn)
{
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    // A dimension referencing an unknown parameter -> no residual.
    Param::UPID v = d.params.CreateDimension("$ghost", d.doc.id);
    SketchDimensionRecord dim;
    dim.id = 2;
    dim.kind = DimensionKind::Length;
    dim.targetA = 1;
    dim.value = v;
    d.doc.dimensions.push_back(dim);

    Solve(d.doc);
    EXPECT_NEAR(Len(*d.doc.Find(1)), 10.0, kEps); // untouched
}

// ── geometric constraints ────────────────────────────────────────────────────

TEST(ConstraintSolver, HorizontalFlattensALine)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 3));
    doc.constraints.push_back(Constraint(2, ConstraintKind::Horizontal, 1));
    Solve(doc);
    const SketchEntity* e = doc.Find(1);
    EXPECT_NEAR(e->a.y, e->b.y, kEps);
}

TEST(ConstraintSolver, VerticalStandsALineUp)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 3, 10));
    doc.constraints.push_back(Constraint(2, ConstraintKind::Vertical, 1));
    Solve(doc);
    const SketchEntity* e = doc.Find(1);
    EXPECT_NEAR(e->a.x, e->b.x, kEps);
}

TEST(ConstraintSolver, ParallelAlignsTwoLines)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0)); // horizontal
    doc.entities.push_back(Line(2, 0, 5, 8, 3)); // sloped
    doc.constraints.push_back(Constraint(3, ConstraintKind::Parallel, 1, 2));
    Solve(doc);
    const SketchEntity* a = doc.Find(1);
    const SketchEntity* b = doc.Find(2);
    f64 cross = (a->b.x - a->a.x) * (b->b.y - b->a.y) - (a->b.y - a->a.y) * (b->b.x - b->a.x);
    EXPECT_NEAR(cross, 0.0, kEps);
}

TEST(ConstraintSolver, PerpendicularSquaresTwoLines)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    doc.entities.push_back(Line(2, 0, 0, 8, 1)); // nearly along X -> should swing to +Y
    doc.constraints.push_back(Constraint(3, ConstraintKind::Perpendicular, 1, 2));
    Solve(doc);
    const SketchEntity* a = doc.Find(1);
    const SketchEntity* b = doc.Find(2);
    f64 dot = (a->b.x - a->a.x) * (b->b.x - b->a.x) + (a->b.y - a->a.y) * (b->b.y - b->a.y);
    EXPECT_NEAR(dot, 0.0, kEps);
}

TEST(ConstraintSolver, EqualMatchesTwoLengths)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0)); // length 10
    doc.entities.push_back(Line(2, 0, 5, 4, 5)); // length 4
    doc.constraints.push_back(Constraint(3, ConstraintKind::Equal, 1, 2));
    Solve(doc);
    EXPECT_NEAR(Len(*doc.Find(1)), Len(*doc.Find(2)), kEps);
}

TEST(ConstraintSolver, TangentTouchesALineToACircle)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, -10, 3, 10, 3)); // horizontal line at y=3
    doc.entities.push_back(Circle(2, 0, 0, 5)); // radius 5 at origin -> needs the line at |y|=5
    doc.constraints.push_back(Constraint(3, ConstraintKind::Tangent, 1, 2));
    Solve(doc);

    const SketchEntity* l = doc.Find(1);
    const SketchEntity* c = doc.Find(2);
    // Distance from centre to the (now) line equals the radius.
    f64 dx = l->b.x - l->a.x, dy = l->b.y - l->a.y;
    f64 lenD = std::hypot(dx, dy);
    f64 dist = std::fabs(dx * (c->a.y - l->a.y) - dy * (c->a.x - l->a.x)) / lenD;
    EXPECT_NEAR(dist, c->radius, kEps);
}

TEST(ConstraintSolver, CoincidentJoinsTwoEndpoints)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    doc.entities.push_back(Line(2, 12, 1, 20, 5)); // starts near, but not at, line 1's end
    // line1.end coincides with line2.start
    SketchConstraintRecord c = Constraint(3, ConstraintKind::Coincident, 1, 2);
    c.aPoint = PointRef::End;
    c.bPoint = PointRef::Start;
    doc.constraints.push_back(c);
    Solve(doc);

    const SketchEntity* a = doc.Find(1);
    const SketchEntity* b = doc.Find(2);
    EXPECT_NEAR(a->b.x, b->a.x, kEps);
    EXPECT_NEAR(a->b.y, b->a.y, kEps);
}

TEST(ConstraintSolver, SymmetryMirrorsALineAcrossAnAxis)
{
    // The axis and original are grounded so the symmetry constraint determines the twin
    // alone; otherwise the solver could satisfy it by moving the axis or original too
    // (all three are free), and "twin == -original" wouldn't be the unique answer.
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 0, 10)); // axis: the Y axis (x=0)
    doc.entities.push_back(Line(2, 5, 1, 8, 2)); // original, on the +X side
    doc.entities.push_back(Line(3, -4, 1, -9, 2)); // twin, roughly mirrored but off

    // Pin the axis (both ends) and the original (both ends).
    auto ground = [&](FeatureId id, FeatureId ent, PointRef pt) {
        SketchConstraintRecord g = Constraint(id, ConstraintKind::Ground, ent);
        g.aPoint = pt;
        doc.constraints.push_back(g);
    };
    ground(10, 1, PointRef::Start);
    ground(11, 1, PointRef::End);
    ground(12, 2, PointRef::Start);
    ground(13, 2, PointRef::End);

    doc.constraints.push_back(Constraint(4, ConstraintKind::Symmetry, 2, 3, 1));
    Solve(doc);

    const SketchEntity* orig = doc.Find(2);
    const SketchEntity* twin = doc.Find(3);
    // With the (grounded) axis at x=0, the mirror negates x and keeps y.
    EXPECT_NEAR(twin->a.x, -orig->a.x, kEps);
    EXPECT_NEAR(twin->a.y, orig->a.y, kEps);
    EXPECT_NEAR(twin->b.x, -orig->b.x, kEps);
    EXPECT_NEAR(twin->b.y, orig->b.y, kEps);
}

// ── grounding + DOF classification ───────────────────────────────────────────

TEST(ConstraintSolver, GroundPinsAPoint)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 2, 3, 12, 3));
    doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1)); // pin start
    Solve(doc);
    const SketchEntity* e = doc.Find(1);
    EXPECT_NEAR(e->a.x, 2, kEps); // start held where it was drawn
    EXPECT_NEAR(e->a.y, 3, kEps);
}

TEST(ConstraintSolver, AFullyPinnedLineIsWellConstrained)
{
    // Ground the start, dimension the length, and fix the angle: no freedom left.
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    d.doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1)); // -2 DOF
    d.doc.constraints.push_back(Constraint(3, ConstraintKind::Horizontal, 1)); // -1 DOF
    d.Dimension(4, DimensionKind::Length, 1, "50mm"); // -1 DOF
    SketchSolveResult r = Solve(d.doc);

    EXPECT_EQ(r.status, SketchSolveStatus::WellConstrained);
    EXPECT_EQ(r.dof, 0u);
    EXPECT_NEAR(Len(*d.doc.Find(1)), 50.0, kEps);
}

TEST(ConstraintSolver, RedundantConstraintsAreOverConstrainedButConsistent)
{
    // Two Horizontal constraints on the same line: the second is redundant but agrees.
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1));
    doc.constraints.push_back(Constraint(3, ConstraintKind::Horizontal, 1));
    doc.constraints.push_back(Constraint(4, ConstraintKind::Horizontal, 1)); // redundant
    SketchSolveResult r = Solve(doc);
    EXPECT_EQ(r.status, SketchSolveStatus::OverConstrained);
}

TEST(ConstraintSolver, ConflictingDimensionsAreDetectedNotCrashed)
{
    // Two different lengths on one line can't both hold.
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    d.doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1));
    d.doc.constraints.push_back(Constraint(3, ConstraintKind::Horizontal, 1));
    d.Dimension(4, DimensionKind::Length, 1, "50mm");
    d.Dimension(5, DimensionKind::Length, 1, "80mm"); // conflicts with 50
    SketchSolveResult r = Solve(d.doc);

    EXPECT_EQ(r.status, SketchSolveStatus::OverConstrained);
    EXPECT_FALSE(r.conflicting.empty()); // the offending dimensions are named
    EXPECT_GT(r.residualNorm, kEps); // couldn't satisfy everything
}

// ── real shapes converge ─────────────────────────────────────────────────────

TEST(ConstraintSolver, RectangleFromConstraintsAndDimensions)
{
    // Four lines, corners coincident, two horizontal + two vertical, one width + one
    // height dimension, one corner grounded -> a fully determined rectangle.
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 1)); // bottom (roughly)
    d.doc.entities.push_back(Line(2, 10, 1, 9, 6)); // right
    d.doc.entities.push_back(Line(3, 9, 6, 1, 5)); // top
    d.doc.entities.push_back(Line(4, 1, 5, 0, 0)); // left

    // Close the loop: each line's end coincides with the next line's start.
    auto corner = [&](FeatureId id, FeatureId a, FeatureId b) {
        SketchConstraintRecord c = Constraint(id, ConstraintKind::Coincident, a, b);
        c.aPoint = PointRef::End;
        c.bPoint = PointRef::Start;
        d.doc.constraints.push_back(c);
    };
    corner(10, 1, 2);
    corner(11, 2, 3);
    corner(12, 3, 4);
    corner(13, 4, 1);

    d.doc.constraints.push_back(Constraint(20, ConstraintKind::Horizontal, 1));
    d.doc.constraints.push_back(Constraint(21, ConstraintKind::Horizontal, 3));
    d.doc.constraints.push_back(Constraint(22, ConstraintKind::Vertical, 2));
    d.doc.constraints.push_back(Constraint(23, ConstraintKind::Vertical, 4));
    d.doc.constraints.push_back(Constraint(24, ConstraintKind::Ground, 1)); // pin a corner

    d.Dimension(30, DimensionKind::Length, 1, "20mm"); // width
    d.Dimension(31, DimensionKind::Length, 2, "12mm"); // height

    SketchSolveResult r = Solve(d.doc);
    EXPECT_TRUE(r.Solved()) << "status " << static_cast<int>(r.status);

    // It really is a 20x12 axis-aligned rectangle.
    EXPECT_NEAR(Len(*d.doc.Find(1)), 20.0, kEps); // bottom width
    EXPECT_NEAR(Len(*d.doc.Find(3)), 20.0, kEps); // top width equals bottom (closed + parallel)
    EXPECT_NEAR(Len(*d.doc.Find(2)), 12.0, kEps); // right height
    EXPECT_NEAR(Len(*d.doc.Find(4)), 12.0, kEps);

    const SketchEntity* bottom = d.doc.Find(1);
    EXPECT_NEAR(bottom->a.y, bottom->b.y, kEps); // horizontal
}

TEST(ConstraintSolver, TriangleFromThreeLengths)
{
    // A 3-4-5 triangle from three coincident-cornered lines with length dimensions.
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 3, 1)); // -> length 3
    d.doc.entities.push_back(Line(2, 3, 1, 3, 4)); // -> length 4
    d.doc.entities.push_back(Line(3, 3, 4, 0, 0)); // -> length 5 (closes)

    auto corner = [&](FeatureId id, FeatureId a, FeatureId b) {
        SketchConstraintRecord c = Constraint(id, ConstraintKind::Coincident, a, b);
        c.aPoint = PointRef::End;
        c.bPoint = PointRef::Start;
        d.doc.constraints.push_back(c);
    };
    corner(10, 1, 2);
    corner(11, 2, 3);
    corner(12, 3, 1);

    d.doc.constraints.push_back(Constraint(20, ConstraintKind::Ground, 1));
    d.doc.constraints.push_back(Constraint(21, ConstraintKind::Horizontal, 1));

    d.Dimension(30, DimensionKind::Length, 1, "3mm");
    d.Dimension(31, DimensionKind::Length, 2, "4mm");
    d.Dimension(32, DimensionKind::Length, 3, "5mm");

    SketchSolveResult r = Solve(d.doc);
    EXPECT_TRUE(r.Solved()) << "status " << static_cast<int>(r.status);
    EXPECT_NEAR(Len(*d.doc.Find(1)), 3.0, kEps);
    EXPECT_NEAR(Len(*d.doc.Find(2)), 4.0, kEps);
    EXPECT_NEAR(Len(*d.doc.Find(3)), 5.0, kEps);
}

// ── parametric, determinism, robustness ──────────────────────────────────────

TEST(ConstraintSolver, ReSolvesWhenAParameterChanges)
{
    Doc d;
    Param::UPID w = d.params.Create("w", "40mm");
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    d.doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1));
    d.doc.constraints.push_back(Constraint(3, ConstraintKind::Horizontal, 1));
    // dimension = $w
    Param::UPID v = d.params.CreateDimension("$w", d.doc.id);
    SketchDimensionRecord dim;
    dim.id = 4;
    dim.kind = DimensionKind::Length;
    dim.targetA = 1;
    dim.value = v;
    d.doc.dimensions.push_back(dim);

    Solve(d.doc);
    EXPECT_NEAR(Len(*d.doc.Find(1)), 40.0, kEps);

    ASSERT_TRUE(d.params.SetExpression(w, "90mm"));
    Solve(d.doc);
    EXPECT_NEAR(Len(*d.doc.Find(1)), 90.0, kEps);
}

TEST(ConstraintSolver, IsDeterministic)
{
    auto build = []() {
        Doc* d = new Doc();
        d->doc.entities.push_back(Line(1, 0, 0, 10, 2));
        d->doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1));
        d->Dimension(3, DimensionKind::Length, 1, "37mm");
        return d;
    };
    Doc* a = build();
    Doc* b = build();
    Solve(a->doc);
    Solve(b->doc);
    EXPECT_NEAR(a->doc.Find(1)->b.x, b->doc.Find(1)->b.x, 1e-12);
    EXPECT_NEAR(a->doc.Find(1)->b.y, b->doc.Find(1)->b.y, 1e-12);
    delete a;
    delete b;
}

TEST(ConstraintSolver, IsIdempotent)
{
    // Solving an already-solved sketch must not move it further.
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    d.doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1));
    d.doc.constraints.push_back(Constraint(3, ConstraintKind::Horizontal, 1));
    d.Dimension(4, DimensionKind::Length, 1, "55mm");

    Solve(d.doc);
    f64 bx = d.doc.Find(1)->b.x;
    f64 by = d.doc.Find(1)->b.y;
    Solve(d.doc);
    EXPECT_NEAR(d.doc.Find(1)->b.x, bx, 1e-9);
    EXPECT_NEAR(d.doc.Find(1)->b.y, by, 1e-9);
}

TEST(ConstraintSolver, EmptySketchIsWellConstrained)
{
    SketchDocument doc;
    SketchSolveResult r = Solve(doc);
    EXPECT_EQ(r.status, SketchSolveStatus::WellConstrained);
    EXPECT_EQ(r.dof, 0u);
}

TEST(ConstraintSolver, DanglingConstraintReferenceIsIgnored)
{
    // A constraint naming a missing entity contributes nothing and must not crash.
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    doc.constraints.push_back(Constraint(2, ConstraintKind::Parallel, 1, 999));
    SketchSolveResult r = Solve(doc);
    EXPECT_TRUE(r.Solved()); // no residual added; the line stays as drawn
    EXPECT_NEAR(Len(*doc.Find(1)), 10.0, kEps);
}

// ── point freedom (drag DOF gating) ──────────────────────────────────────────

TEST(PointFreedom, AFreeLineHasFullyFreeEndpoints)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    EXPECT_EQ(SketchPointFreedom(doc, 1, PointRef::Start), 2u);
    EXPECT_EQ(SketchPointFreedom(doc, 1, PointRef::End), 2u);
}

TEST(PointFreedom, AGroundedPointIsLockedButTheOtherEndIsFree)
{
    SketchDocument doc;
    doc.entities.push_back(Line(1, 0, 0, 10, 0));
    doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1)); // pins start
    EXPECT_EQ(SketchPointFreedom(doc, 1, PointRef::Start), 0u); // locked
    EXPECT_EQ(SketchPointFreedom(doc, 1, PointRef::End), 2u); // still free
}

TEST(PointFreedom, AFullyPinnedLineHasNoFreedom)
{
    Doc d;
    d.doc.entities.push_back(Line(1, 0, 0, 10, 0));
    d.doc.constraints.push_back(Constraint(2, ConstraintKind::Ground, 1));
    d.doc.constraints.push_back(Constraint(3, ConstraintKind::Horizontal, 1));
    d.Dimension(4, DimensionKind::Length, 1, "50mm");
    EXPECT_EQ(SketchPointFreedom(d.doc, 1, PointRef::Start), 0u);
    EXPECT_EQ(SketchPointFreedom(d.doc, 1, PointRef::End), 0u);
}

TEST(PointFreedom, MissingEntityIsLocked)
{
    SketchDocument doc;
    EXPECT_EQ(SketchPointFreedom(doc, 999, PointRef::Start), 0u);
}
