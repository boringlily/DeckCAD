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

// One drawn entity. A flat struct rather than a variant: the fields that don't apply
// to a kind simply go unread, and keeping it POD makes the document trivially copyable.
struct SketchEntity {
    FeatureId id { kNullFeature };
    EntityKind kind { EntityKind::Line };

    Geometry::Point2 a {}; // Line: start.   Arc/Circle: centre.
    Geometry::Point2 b {}; // Line: end.
    f64 radius { 0 }; // Arc/Circle.
    f64 startAngle { 0 }; // Arc, radians.
    f64 endAngle { 0 }; // Arc, radians.
    bool construction { false };
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

struct SketchConstraintRecord {
    FeatureId id { kNullFeature };
    ConstraintKind kind { ConstraintKind::Coincident };
    FeatureId a { kNullFeature };
    FeatureId b { kNullFeature };
    FeatureId axis { kNullFeature }; // Symmetry only
    PointRef aPoint { PointRef::Start }; // Coincident only
    PointRef bPoint { PointRef::Start }; // Coincident only
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

    std::vector<SketchEntity> entities;
    std::vector<SketchDimensionRecord> dimensions;
    std::vector<SketchConstraintRecord> constraints;

    // Non-owning view of the scene's parameter table, so a dimension can resolve its
    // UPID during execute(). Null in tests that don't exercise dimensions.
    const Param::ParameterEngine* params { nullptr };

    // The last solve's diagnostics — set by ISketchSolver::Solve, read by the UI.
    SketchSolveResult lastSolve {};

    void Clear()
    {
        entities.clear();
        dimensions.clear();
        constraints.clear();
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
