#pragma once
#include "Context.h"
#include "DTL.h"
#include "Geometry.h"
#include "ParameterEngine.h"
#include "SketchCmd.h"
#include "ToolId.h"
#include <cmath>
#include <string>
#include <vector>

// The ephemeral interaction layer: one gesture's worth of in-progress state.
//
// NOTHING BECOMES A COMMAND UNTIL A TOOL CONFIRMS. While a tool is active the canvas
// renders straight from its in-progress parameters (Points() below); only on Finish()
// does it become a real command, through ONE commit path — used identically whether the
// value came from a click-drag or a typed number.
//
// The tool collects inputs GENERICALLY against its ToolInfo row (how many points, how
// many picks, a value, a plane). The canvas just feeds points and picks; it never
// switches on which tool is running. That is what removes the per-command interaction
// code the old design hand-wrote for lines and would have had to rewrite for arcs.

enum class ContextAction : u8 {
    None,
    Push, // enter a nested context
    Confirm, // finish the current context (composite hands its result down)
    Cancel, // back out one level
};

struct PushRequest {
    ContextKind kind { ContextKind::Sketch };
    Geometry::SketchPlane plane { Geometry::SketchPlane::XY };
    FeatureId axis { kNullFeature }; // SymmetryGroup
};

// What a finished tool asks the system to do. A tool does not always emit a command —
// "New Sketch" only pushes a context (there is nothing to commit yet), and "Finish
// Sketch" only confirms one.
struct ToolOutcome {
    DTL::Optional<SketchCmd> command {};
    ContextAction action { ContextAction::None };
    PushRequest push {};

    // Set on a line that was chained from the previous one (a polyline segment). The
    // commit path reads it to auto-add a Coincident between the previous line's end and
    // this line's start, so the polyline is a real connected chain, not just visually
    // touching points.
    bool chainLink { false };
};

class Tool {
public:
    // ── lifecycle ────────────────────────────────────────────────────────────
    void Begin(ToolId t)
    {
        Reset();
        id = t;
        active = (t != ToolId::None);
    }

    void Reset()
    {
        id = ToolId::None;
        active = false;
        points.clear();
        picks.clear();
        value.clear();
        plane.reset();
        chain = false;
    }

    bool Active() const { return active; }
    ToolId Id() const { return id; }
    const ToolInfo* Info() const { return FindTool(id); }

    // ── input collection ─────────────────────────────────────────────────────
    void AddPoint(Geometry::Point2 p)
    {
        const ToolInfo* i = Info();
        if (!active || !i || points.size() >= i->points) {
            return;
        }
        points.push_back(p);
    }

    void AddPick(FeatureId f)
    {
        const ToolInfo* i = Info();
        if (!active || !i || picks.size() >= i->picks || f == kNullFeature) {
            return;
        }
        picks.push_back(f);
    }

    void SetValue(std::string_view v) { value.assign(v); }

    // Mark this line as a chain link — the canvas sets it when it auto-restarts the Line
    // tool to continue a polyline, so the commit path knows to add the coincident.
    void SetChain(bool c) { chain = c; }
    bool IsChain() const { return chain; }
    void SetPlane(Geometry::SketchPlane p) { plane = p; }

    // Seed the first point — used to chain a new line from the previous one's end.
    void SeedPoint(Geometry::Point2 p)
    {
        if (points.empty()) {
            points.push_back(p);
        }
    }

    // ── readiness ────────────────────────────────────────────────────────────
    // Every declared input collected? This is the whole of the tool state machine.
    bool Ready() const
    {
        const ToolInfo* i = Info();
        if (!active || !i) {
            return false;
        }
        if (points.size() < i->points || picks.size() < i->picks) {
            return false;
        }
        if (i->value && value.empty()) {
            return false;
        }
        if (i->plane && !plane.has_value()) {
            return false;
        }
        return true;
    }

    // How many more canvas clicks this gesture wants (drives the preview).
    u32 PointsRemaining() const
    {
        const ToolInfo* i = Info();
        if (!i || points.size() >= i->points) {
            return 0;
        }
        return i->points - static_cast<u32>(points.size());
    }

    const std::vector<Geometry::Point2>& Points() const { return points; }
    const std::vector<FeatureId>& Picks() const { return picks; }
    const std::string& Value() const { return value; }
    const DTL::Optional<Geometry::SketchPlane>& Plane() const { return plane; }

    // ── finish ───────────────────────────────────────────────────────────────
    // Turn the collected inputs into an outcome. `params` is needed because a dimension's
    // value is an expression that must be registered to get a UPID; `sketchOwner` is the
    // sketch that expression belongs to.
    //
    // Returns an empty outcome (action None, no command) when the tool isn't ready, so a
    // premature call is inert rather than committing something half-specified.
    ToolOutcome Finish(Param::ParameterEngine& params, FeatureId sketchOwner)
    {
        ToolOutcome out {};
        if (!Ready()) {
            return out;
        }

        switch (id) {
        case ToolId::CreateSketch: {
            out.action = ContextAction::Push;
            out.push.kind = ContextKind::Sketch;
            out.push.plane = *plane;
            break;
        }

        case ToolId::Line: {
            SketchLine l {};
            l.a = points[0];
            l.b = points[1];
            if (l.Valid()) {
                out.command = SketchCmd { l };
                out.chainLink = chain; // link this segment to the previous line
            }
            break;
        }

        case ToolId::Circle: {
            SketchCircle c {};
            c.center = points[0];
            c.radius = Distance(points[0], points[1]);
            if (c.Valid()) {
                out.command = SketchCmd { c };
            }
            break;
        }

        case ToolId::Arc: {
            // centre, then a point fixing the radius + start angle, then the end angle.
            SketchArc a {};
            a.center = points[0];
            a.radius = Distance(points[0], points[1]);
            a.startAngle = Angle(points[0], points[1]);
            a.endAngle = Angle(points[0], points[2]);
            if (a.Valid()) {
                out.command = SketchCmd { a };
            }
            break;
        }

        case ToolId::Dimension: {
            // The typed expression becomes a real parameter so it can reference others
            // ($w * 2) and re-solve when they change. Same path whether it was typed or
            // will later be dragged.
            Param::UPID upid = params.CreateDimension(value, sketchOwner);
            if (upid == Param::kNullUpid) {
                break;
            }
            SketchDimensionCmd d {};
            d.kind = DimensionKind::Length;
            d.targetA = picks[0];
            d.value = upid;
            if (d.Valid()) {
                out.command = SketchCmd { d };
            }
            break;
        }

        case ToolId::Coincident:
        case ToolId::Parallel:
        case ToolId::Perpendicular:
        case ToolId::Equal:
        case ToolId::Tangent: {
            // The two-entity geometric constraints all share one shape: pick a, pick b,
            // emit a constraint of the matching kind. Coincident defaults to the two
            // entities' start points (precise endpoint selection is a later UI step).
            SketchConstraintCmd c {};
            switch (id) {
            case ToolId::Coincident:
                c.kind = ConstraintKind::Coincident;
                break;
            case ToolId::Parallel:
                c.kind = ConstraintKind::Parallel;
                break;
            case ToolId::Perpendicular:
                c.kind = ConstraintKind::Perpendicular;
                break;
            case ToolId::Equal:
                c.kind = ConstraintKind::Equal;
                break;
            default:
                c.kind = ConstraintKind::Tangent;
                break;
            }
            c.a = picks[0];
            c.b = picks[1];
            if (c.Valid()) {
                out.command = SketchCmd { c };
            }
            break;
        }

        case ToolId::Ground: {
            // Pin one entity's point where it is — the sketch's anchor to the world.
            SketchConstraintCmd c {};
            c.kind = ConstraintKind::Ground;
            c.a = picks[0];
            if (c.Valid()) {
                out.command = SketchCmd { c };
            }
            break;
        }

        case ToolId::SymmetryGroup: {
            out.action = ContextAction::Push;
            out.push.kind = ContextKind::SymmetryGroup;
            out.push.axis = picks[0];
            break;
        }

        case ToolId::FinishSketch:
        case ToolId::StopSymmetry: {
            out.action = ContextAction::Confirm;
            break;
        }

        case ToolId::None:
            break;
        }

        return out;
    }

private:
    static f64 Distance(Geometry::Point2 a, Geometry::Point2 b)
    {
        f64 dx = b.x - a.x;
        f64 dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static f64 Angle(Geometry::Point2 center, Geometry::Point2 p)
    {
        return std::atan2(p.y - center.y, p.x - center.x);
    }

    ToolId id { ToolId::None };
    bool active { false };
    bool chain { false }; // this line continues a polyline from the previous one
    std::vector<Geometry::Point2> points;
    std::vector<FeatureId> picks;
    std::string value;
    DTL::Optional<Geometry::SketchPlane> plane {};
};
