#pragma once
#include "DTL.h"
#include "Geometry.h"
#include "SketchCmd.h"
#include <cmath>

// Reflection helpers for SymmetryGroupContext.
//
// The mirror is a genuine synthesis: a twin command is produced with real coordinates
// and committed for real, exactly as if the user had drawn it. It is NOT a rendering
// trick, which is why cancelling a mirror context leaves the twins behind (they were
// always real) and why the twin is an ordinary command the user can select and delete.
namespace Geometry {

// Reflect `p` across the infinite line through a→b. Degenerate axes (a == b) have no
// direction to reflect across, so the point is returned untouched rather than turned
// into a NaN.
inline Point2 MirrorPoint(Point2 p, Point2 a, Point2 b)
{
    f64 dx = b.x - a.x;
    f64 dy = b.y - a.y;
    f64 lenSq = dx * dx + dy * dy;
    if (lenSq == 0.0) {
        return p;
    }

    f64 vx = p.x - a.x;
    f64 vy = p.y - a.y;
    f64 t = (vx * dx + vy * dy) / lenSq; // projection of v onto the axis

    f64 projX = a.x + t * dx;
    f64 projY = a.y + t * dy;

    // The reflection is the point stepped twice from itself to its projection.
    return Point2 { 2.0 * projX - p.x, 2.0 * projY - p.y };
}

inline f64 AxisAngle(Point2 a, Point2 b) { return std::atan2(b.y - a.y, b.x - a.x); }

// Build the mirrored twin of `src`. Returns nullopt for commands that have no spatial
// meaning to reflect (dimensions, constraints, nested groups) — those are forwarded
// unmirrored by the caller rather than duplicated.
inline DTL::Optional<SketchCmd> MirrorCmd(const SketchCmd& src, Point2 axisA, Point2 axisB, FeatureId newId)
{
    if (const SketchLine* l = src.As<SketchLine>()) {
        SketchLine m = *l;
        m.id = newId;
        m.a = MirrorPoint(l->a, axisA, axisB);
        m.b = MirrorPoint(l->b, axisA, axisB);
        return SketchCmd { m };
    }

    if (const SketchCircle* c = src.As<SketchCircle>()) {
        SketchCircle m = *c;
        m.id = newId;
        m.center = MirrorPoint(c->center, axisA, axisB);
        return SketchCmd { m };
    }

    if (const SketchArc* arc = src.As<SketchArc>()) {
        SketchArc m = *arc;
        m.id = newId;
        m.center = MirrorPoint(arc->center, axisA, axisB);
        // Reflecting reverses orientation: an angle a about an axis at angle t maps to
        // 2t - a, and start/end swap so the arc still sweeps the correct side.
        f64 t = AxisAngle(axisA, axisB);
        m.startAngle = 2.0 * t - arc->endAngle;
        m.endAngle = 2.0 * t - arc->startAngle;
        return SketchCmd { m };
    }

    return std::nullopt;
}

} // namespace Geometry
