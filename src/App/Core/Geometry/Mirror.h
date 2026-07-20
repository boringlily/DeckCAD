#pragma once
#include "DTL.h"
#include "Geometry.h"
#include "SketchCmd.h"
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

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

// The curve a gesture is "about" — the first line/circle/arc in a compound, or the command
// itself if it is a bare curve. Used to name the two subjects of the symmetry constraint
// (which links the ORIGINAL curve to its TWIN, by id). kNullFeature if there is no curve.
inline FeatureId PrimaryCurveId(const SketchCmd& cmd)
{
    if (cmd.As<SketchLine>() || cmd.As<SketchCircle>() || cmd.As<SketchArc>()) {
        return cmd.Get().id;
    }
    if (const CompoundSketchCmd* g = cmd.As<CompoundSketchCmd>()) {
        for (const SketchCmd& c : g->children) {
            if (c.As<SketchLine>() || c.As<SketchCircle>() || c.As<SketchArc>()) {
                return c.Get().id;
            }
        }
    }
    return kNullFeature;
}

// Build the mirrored twin of `src`, a whole curve GESTURE (a compound of points + a curve).
//
// A curve holds no coordinates now — only point references — so mirroring one means
// reflecting its POINTS (each a distinct new point at the reflected position) and rebuilding
// the curve over the twinned point ids. `nextId` supplies fresh FeatureIds for every twin.
//
// Returns nullopt for anything that has no spatial twin — a bare dimension/constraint, or a
// bare curve with no points to reflect — which the caller forwards unmirrored.
inline DTL::Optional<SketchCmd> MirrorCmd(
    const SketchCmd& src, Point2 axisA, Point2 axisB, const std::function<FeatureId()>& nextId)
{
    const CompoundSketchCmd* g = src.As<CompoundSketchCmd>();
    if (!g) {
        return std::nullopt; // curve gestures are compounds; nothing else has a spatial twin
    }

    CompoundSketchCmd m {};
    m.id = nextId();

    // First pass: reflect each point child, recording old-id → new-id so the curve children
    // can be re-pointed at the twins.
    std::vector<std::pair<FeatureId, FeatureId>> remap;
    for (const SketchCmd& child : g->children) {
        if (const SketchPointCmd* p = child.As<SketchPointCmd>()) {
            SketchPointCmd np = *p;
            np.id = nextId();
            np.initial = MirrorPoint(p->initial, axisA, axisB);
            remap.push_back({ p->id, np.id });
            m.children.push_back(SketchCmd { np });
        }
    }
    auto mapPoint = [&](FeatureId old) -> FeatureId {
        for (const auto& r : remap) {
            if (r.first == old) {
                return r.second;
            }
        }
        return old; // a point defined outside this gesture — leave the reference as-is
    };

    // Second pass: reflect the curve children, re-pointing to the twinned points.
    for (const SketchCmd& child : g->children) {
        if (child.Is<SketchPointCmd>()) {
            continue;
        }
        if (const SketchLine* l = child.As<SketchLine>()) {
            SketchLine ml = *l;
            ml.id = nextId();
            ml.p0 = mapPoint(l->p0);
            ml.p1 = mapPoint(l->p1);
            m.children.push_back(SketchCmd { ml });
        } else if (const SketchCircle* c = child.As<SketchCircle>()) {
            SketchCircle mc = *c;
            mc.id = nextId();
            mc.a = mapPoint(c->a);
            mc.b = mapPoint(c->b);
            m.children.push_back(SketchCmd { mc });
        } else if (const SketchArc* arc = child.As<SketchArc>()) {
            SketchArc ma = *arc;
            ma.id = nextId();
            ma.center = mapPoint(arc->center);
            // Reflection reverses orientation, so the twin's start/end SWAP: the mirrored arc
            // sweeps from the reflected end to the reflected start, covering the mirror-image
            // side. (The angles are derived from the points, so there is nothing else to flip.)
            ma.start = mapPoint(arc->end);
            ma.end = mapPoint(arc->start);
            m.children.push_back(SketchCmd { ma });
        }
    }

    if (PrimaryCurveId(SketchCmd { m }) == kNullFeature) {
        return std::nullopt; // no curve to mirror (e.g. a compound of only points)
    }
    return SketchCmd { std::move(m) };
}

} // namespace Geometry
