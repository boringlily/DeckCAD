#pragma once
#include "DTL.h"
#include "Geometry.h"
#include "ParameterEngine.h"
#include <cmath>
#include <string_view>
#include <vector>

// The evaluated contents of one sketch — what SketchCmd::execute() regenerates into,
// and what the canvas draws.
//
// This is a CACHE, never a source of truth: it is thrown away and rebuilt by replaying
// the command history. Nothing outside the replay may mutate it, or the next recompute
// silently discards the edit.
//
// Cross-references are FeatureId handles, never pointers or indices: entities move
// around in the vector between recomputes, but a FeatureId means the same thing
// forever, so a dimension can outlive any number of rebuilds.

// A stable handle to whatever one command produced. Assigned once at commit and
// carried through every recompute — this is what makes a dimension's reference to a
// line survive editing an earlier command.
using FeatureId = u32;
inline constexpr FeatureId kNullFeature = u32_max;

enum class EntityKind : u8 {
    Line,
    Arc,
    Circle,
};

// A parametric point — the ONLY thing in a solved sketch that carries a position. The
// solver's variables live here; curves reference points by id. Every coordinate a sketch
// has lives on a point; curves and their radii/angles are all derived from points.
struct GeomPoint {
    FeatureId id { kNullFeature };
    Geometry::Point2 pos {};
};

// A curve in the SOLVED output. It REFERENCES points (its skeleton, where the DOF live)
// and caches the values convenient for drawing/measuring (centre/radius/angles) that are
// DERIVED from those points after each solve. The cached values are OUTPUT, not an authored
// definition — nothing here is a degree of freedom the solver varies; only the points are.
//
// Point roles by kind:
//   Line   — p0 = start,  p1 = end.
//   Circle — p0 = centre, p1 = rim (radius = ‖p0−p1‖).
//   Arc    — p0 = centre, p1 = start, p2 = end. radius = ‖p0−p1‖; the solver keeps
//            ‖p0−p1‖ = ‖p0−p2‖ so both endpoints sit on the same circle.
struct SketchEntity {
    FeatureId id { kNullFeature };
    EntityKind kind { EntityKind::Line };

    FeatureId p0 { kNullFeature }; // Line start / Circle+Arc centre
    FeatureId p1 { kNullFeature }; // Line end / Circle rim / Arc start
    FeatureId p2 { kNullFeature }; // Arc end

    bool construction { false };

    // Derived draw/measure cache, refreshed from the referenced points post-solve (see
    // DeriveOne). These are output, never authored:
    //   a = line start, or circle/arc centre.   b = line end.
    Geometry::Point2 a {};
    Geometry::Point2 b {};
    f64 radius { 0 }; // Circle/Arc, derived from the rim/start point.
    f64 startAngle { 0 }; // Arc, radians, derived from p1.
    f64 endAngle { 0 }; // Arc, radians, derived from p2.
};

enum class DimensionKind : u8 {
    Length, // of a line
    Radius, // of a circle/arc
    Angle, // of a line vs. the sketch X axis
    Distance, // between two entities
};

// A dimension DICTATES its target's size. Its value is a UPID, not a number, so it can
// be an expression referencing other parameters ($w * 2) and re-solve when they change.
struct SketchDimensionRecord {
    FeatureId id { kNullFeature };
    DimensionKind kind { DimensionKind::Length };
    FeatureId targetA { kNullFeature };
    FeatureId targetB { kNullFeature }; // Distance only
    Param::UPID value { Param::kNullUpid };
};

enum class ConstraintKind : u8 {
    Coincident, // point a[aPoint] == point b[bPoint]
    Symmetry, // line a/b mirrored about `axis`
    Horizontal, // line a is horizontal
    Vertical, // line a is vertical
    Parallel, // lines a, b parallel
    Perpendicular, // lines a, b perpendicular
    Tangent, // line a tangent to circle/arc b
    Equal, // lines a, b equal length
    Ground, // pin point a[aPoint] to where it was drawn (removes translation freedom)
};

// Which point of an entity a constraint refers to. Only Coincident uses these; every
// other constraint is entity-level and leaves them at their defaults.
enum class PointRef : u8 {
    Start, // line: endpoint a.   circle/arc: centre.
    End, // line: endpoint b.
    Center, // circle/arc centre (explicit; same as Start for those).
};

// Coincident/Ground reference POINT ids in a/b; every other constraint references CURVE
// ids (lines, circles). There is no PointRef field: a point is a first-class entity with
// its own id, so "which endpoint" is just a point id.
struct SketchConstraintRecord {
    FeatureId id { kNullFeature };
    ConstraintKind kind { ConstraintKind::Coincident };
    FeatureId a { kNullFeature };
    FeatureId b { kNullFeature };
    FeatureId axis { kNullFeature }; // Symmetry only
};

// ── solve diagnostics ──────────────────────────────────────────────────────────
// How well a sketch's geometry is pinned down by its dimensions and constraints. Filled
// by the solver each recompute and stored on the document so the UI can show it.
enum class SketchSolveStatus : u8 {
    WellConstrained, // exactly determined and satisfied
    UnderConstrained, // has remaining freedom (solved to the nearest-as-drawn pose)
    OverConstrained, // redundant or conflicting constraints
    DidNotConverge, // the iteration failed to reach a solution
};

struct SketchSolveResult {
    SketchSolveStatus status { SketchSolveStatus::WellConstrained };
    u32 dof { 0 }; // remaining degrees of freedom
    f64 residualNorm { 0 }; // ‖hard residual‖ at the solution
    u32 iterations { 0 };
    std::vector<FeatureId> conflicting; // constraints implicated when Over/DidNotConverge

    bool Solved() const
    {
        return status == SketchSolveStatus::WellConstrained
            || status == SketchSolveStatus::UnderConstrained;
    }
};

// The outcome of executing one command. -fno-exceptions: failures come back, never up.
enum class ExecStatus : u8 {
    Ok,
    InvalidCommand, // the command's own parameters are incomplete
    MissingReference, // it referenced a FeatureId that isn't in the document
    BadParameter, // its expression didn't evaluate
};

struct ExecResult {
    ExecStatus status { ExecStatus::Ok };
    std::string_view msg {};
    bool Ok() const { return status == ExecStatus::Ok; }
};

inline ExecResult ExecOk() { return {}; }
inline ExecResult ExecFail(ExecStatus s, std::string_view m) { return { s, m }; }

class SketchDocument {
public:
    // The FeatureId of the SketchFeatureCommand that produced this. Dimension values
    // in the parameter table are owned by it, so deleting the sketch can take its
    // dimensions with it.
    FeatureId id { kNullFeature };
    Geometry::SketchPlane plane { Geometry::SketchPlane::XY };

    std::vector<GeomPoint> points; // the DOF: every position lives here
    std::vector<SketchEntity> entities; // curves referencing points (+ derived cache)
    std::vector<SketchDimensionRecord> dimensions;
    std::vector<SketchConstraintRecord> constraints;

    // Non-owning view of the scene's parameter table, so a dimension can resolve its
    // UPID during execute(). Null in tests that don't exercise dimensions.
    const Param::ParameterEngine* params { nullptr };

    // The last solve's diagnostics — set by ISketchSolver::Solve, read by the UI.
    SketchSolveResult lastSolve {};

    void Clear()
    {
        points.clear();
        entities.clear();
        dimensions.clear();
        constraints.clear();
    }

    // Add a point with an explicit id — used by SketchPointCmd::execute during replay, so
    // the point carries its command's stable FeatureId (referenceable by curves and
    // constraints).
    void AddPointWithId(FeatureId pid, Geometry::Point2 pos)
    {
        points.push_back({ pid, pos });
    }

    GeomPoint* FindPoint(FeatureId pid)
    {
        for (GeomPoint& p : points) {
            if (p.id == pid) {
                return &p;
            }
        }
        return nullptr;
    }

    const GeomPoint* FindPoint(FeatureId pid) const
    {
        for (const GeomPoint& p : points) {
            if (p.id == pid) {
                return &p;
            }
        }
        return nullptr;
    }

    Geometry::Point2 PointPos(FeatureId pid) const
    {
        const GeomPoint* p = FindPoint(pid);
        return p ? p->pos : Geometry::Point2 {};
    }

    // Recompute one curve's derived draw/measure cache (a/b/centre/radius/angles) from its
    // referenced points. This is where "a circle's radius" and "an arc's angles" actually
    // come from now — they are outputs of the points, never stored DOF.
    void DeriveOne(SketchEntity& e) const
    {
        Geometry::Point2 p0 = PointPos(e.p0);
        e.a = p0; // line start / circle+arc centre
        switch (e.kind) {
        case EntityKind::Line:
            e.b = PointPos(e.p1);
            break;
        case EntityKind::Circle: {
            Geometry::Point2 rim = PointPos(e.p1);
            e.radius = std::hypot(rim.x - p0.x, rim.y - p0.y);
            break;
        }
        case EntityKind::Arc: {
            Geometry::Point2 s = PointPos(e.p1);
            Geometry::Point2 en = PointPos(e.p2);
            e.radius = std::hypot(s.x - p0.x, s.y - p0.y);
            e.startAngle = std::atan2(s.y - p0.y, s.x - p0.x);
            e.endAngle = std::atan2(en.y - p0.y, en.x - p0.x);
            break;
        }
        }
    }

    // Recompute every curve's derived cache from its points. Called by the solver after it
    // writes solved point positions back, so consumers can read a/b/radius/angles.
    void RefreshDerived()
    {
        for (SketchEntity& e : entities) {
            DeriveOne(e);
        }
    }

    SketchEntity* Find(FeatureId id)
    {
        for (SketchEntity& e : entities) {
            if (e.id == id) {
                return &e;
            }
        }
        return nullptr;
    }

    const SketchEntity* Find(FeatureId id) const
    {
        for (const SketchEntity& e : entities) {
            if (e.id == id) {
                return &e;
            }
        }
        return nullptr;
    }

    // Is this entity's size dictated by a dimension? An entity with no dimension can
    // still be MEASURED (see MeasureLength) — it just has nothing driving it.
    bool IsDriven(FeatureId id) const
    {
        for (const SketchDimensionRecord& d : dimensions) {
            if (d.targetA == id) {
                return true;
            }
        }
        return false;
    }

    // The length a line currently happens to have. Always answerable, dimensioned or
    // not — that distinction is exactly what IsDriven reports.
    DTL::Optional<f64> MeasureLength(FeatureId id) const
    {
        const SketchEntity* e = Find(id);
        if (!e || e->kind != EntityKind::Line) {
            return std::nullopt;
        }
        f64 dx = e->b.x - e->a.x;
        f64 dy = e->b.y - e->a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    DTL::Optional<f64> MeasureRadius(FeatureId id) const
    {
        const SketchEntity* e = Find(id);
        if (!e || (e->kind != EntityKind::Circle && e->kind != EntityKind::Arc)) {
            return std::nullopt;
        }
        return e->radius;
    }
};
