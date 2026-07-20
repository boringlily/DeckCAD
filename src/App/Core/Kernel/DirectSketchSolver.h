#pragma once
#include "DTL.h"
#include "ISketchSolver.h"
#include "ParameterEngine.h"
#include "SketchDocument.h"
#include <cmath>

// The closed-form solver: each dimension drives its own target directly, with no DOF
// analysis and no iteration.
//
//   Length  slides a line's END point along the line's OWN direction, so the line keeps
//           its start and its angle and only changes how long it is.
//   Radius  sets a circle's or arc's radius.
//   Angle   rotates a line's end point about its start, preserving its length.
//
// This is deliberately not a constraint solver. It handles the case where one dimension
// drives one entity, which is the overwhelming majority of real sketching, and it is
// exact and instant. What it cannot do is resolve dimensions that interact (two lines
// sharing a constrained endpoint), detect over/under-constrained sketches, or honour
// geometric constraints — those need a residual system and Newton-Raphson, and land
// behind this same ISketchSolver interface without any caller noticing.
//
// Entities with no dimension are left EXACTLY as drawn. That is the requirement, not an
// omission: an unconstrained line can be measured, but nothing dictates its length.
//
// A dimension whose expression is broken (unknown parameter, cycle, typo) is SKIPPED
// rather than defaulted to zero — the geometry stays as last drawn while the user fixes
// the expression, instead of collapsing under them mid-edit.
struct DirectSketchSolver : ISketchSolver {
    void Solve(SketchDocument& doc) const override
    {
        if (!doc.params) {
            return; // no parameter table: nothing can resolve, leave geometry as drawn
        }

        for (const SketchDimensionRecord& d : doc.dimensions) {
            Param::EvalResult r = doc.params->Value(d.value);
            if (!r.Ok()) {
                continue;
            }

            SketchEntity* e = doc.Find(d.targetA);
            if (!e) {
                continue;
            }

            switch (d.kind) {
            case DimensionKind::Length:
                ApplyLength(doc, *e, r.value);
                break;
            case DimensionKind::Radius:
                ApplyRadius(doc, *e, r.value);
                break;
            case DimensionKind::Angle:
                ApplyAngle(doc, *e, r.value);
                break;
            case DimensionKind::Distance:
                // Two-entity dimensions have no single target to move: which one gives?
                // That is a genuine DOF question, so it waits for the real solver rather
                // than picking one arbitrarily.
                break;
            }
        }

        // Length/Angle moved the endpoint POINTS; sync each curve's a/b draw cache.
        doc.RefreshDerived();
    }

private:
    // A Length dimension accepts a Length (already in mm) or a bare Number, which is
    // taken to be in the base unit. An Angle would be nonsense here and is ignored.
    static DTL::Optional<f64> AsLength(Param::Quantity q)
    {
        if (q.kind == Param::QuantityKind::Length || q.kind == Param::QuantityKind::Number) {
            return q.value;
        }
        return std::nullopt;
    }

    static DTL::Optional<f64> AsAngle(Param::Quantity q)
    {
        if (q.kind == Param::QuantityKind::Angle || q.kind == Param::QuantityKind::Number) {
            return q.value; // radians
        }
        return std::nullopt;
    }

    static void ApplyLength(SketchDocument& doc, SketchEntity& e, Param::Quantity q)
    {
        if (e.kind != EntityKind::Line) {
            return;
        }
        DTL::Optional<f64> target = AsLength(q);
        if (!target.has_value() || *target < 0.0) {
            return;
        }
        GeomPoint* pb = doc.FindPoint(e.p1);
        if (!pb) {
            return;
        }
        Geometry::Point2 a = doc.PointPos(e.p0);
        Geometry::Point2 b = pb->pos;

        f64 dx = b.x - a.x;
        f64 dy = b.y - a.y;
        f64 len = std::sqrt(dx * dx + dy * dy);
        if (len == 0.0) {
            // No direction to grow along. Fall back to +X so the dimension still has a
            // visible effect rather than silently doing nothing.
            pb->pos = { a.x + *target, a.y };
            return;
        }

        f64 s = *target / len;
        pb->pos = { a.x + dx * s, a.y + dy * s };
    }

    static void ApplyRadius(SketchDocument& doc, SketchEntity& e, Param::Quantity q)
    {
        if (e.kind != EntityKind::Circle && e.kind != EntityKind::Arc) {
            return;
        }
        DTL::Optional<f64> target = AsLength(q);
        if (!target.has_value() || *target <= 0.0) {
            return;
        }
        // Radius is derived from points now, so drive it by MOVING the rim/endpoint points
        // to `target` along their current direction from the centre. For an arc both
        // endpoints move, so it stays a proper (equal-radius) arc. RefreshDerived (called at
        // the end of Solve) recomputes the cached radius from the moved points.
        Geometry::Point2 c = doc.PointPos(e.p0);
        auto moveToRadius = [&](FeatureId pid) {
            GeomPoint* p = doc.FindPoint(pid);
            if (!p) {
                return;
            }
            f64 dx = p->pos.x - c.x;
            f64 dy = p->pos.y - c.y;
            f64 len = std::sqrt(dx * dx + dy * dy);
            if (len == 0.0) {
                p->pos = { c.x + *target, c.y }; // no direction — fall back to +X
                return;
            }
            f64 s = *target / len;
            p->pos = { c.x + dx * s, c.y + dy * s };
        };
        moveToRadius(e.p1); // circle rim / arc start
        if (e.kind == EntityKind::Arc) {
            moveToRadius(e.p2); // arc end — keeps ‖centre−start‖ = ‖centre−end‖
        }
    }

    static void ApplyAngle(SketchDocument& doc, SketchEntity& e, Param::Quantity q)
    {
        if (e.kind != EntityKind::Line) {
            return;
        }
        DTL::Optional<f64> target = AsAngle(q);
        if (!target.has_value()) {
            return;
        }
        GeomPoint* pb = doc.FindPoint(e.p1);
        if (!pb) {
            return;
        }
        Geometry::Point2 a = doc.PointPos(e.p0);
        f64 dx = pb->pos.x - a.x;
        f64 dy = pb->pos.y - a.y;
        f64 len = std::sqrt(dx * dx + dy * dy);
        if (len == 0.0) {
            return;
        }
        // Rotate about the start, preserving length.
        pb->pos = { a.x + len * std::cos(*target), a.y + len * std::sin(*target) };
    }
};
