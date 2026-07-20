#pragma once
#include "DTL.h"
#include "DirectSketchSolver.h"
#include "ISketchSolver.h"
#include "ParameterEngine.h"
#include "SketchDocument.h"

#include <cmath>
#include <functional>
#include <vector>

// A real 2D sketch solver: turns the sketch's dimensions and constraints into a
// nonlinear least-squares system r(x) = 0 and drives it to zero with Levenberg-Marquardt,
// then reports how well-constrained the result is.
//
// This is the payoff of the ISketchSolver seam. It swaps in for DirectSketchSolver with
// no change anywhere above it: same Solve(SketchDocument&), same in-place mutation of
// entity coordinates.
//
// EXCEPTION-FREE and DETERMINISTIC. The linear algebra is hand-rolled dense (no
// dependency, no throw); a singular system is a returned failure that bumps the damping,
// never an exception. No randomness, no clock — the same sketch always solves the same
// way.
//
// Everything an entity WASN'T told about is left as drawn: the iteration takes
// minimum-norm steps (Δx in the row space of the constraint Jacobian), so any degree of
// freedom the constraints don't touch — the null space — never moves. That is the
// measured-vs-dictated rule, enforced numerically, with no anchor term to bias the
// constraints or to tune.
namespace Solver {

// ── dense linear algebra (small systems only) ────────────────────────────────
namespace la {

    // Solve A x = b for a square n×n A (row-major) by Gaussian elimination with partial
    // pivoting. Returns false if A is singular to tolerance (caller raises damping and
    // retries). A and b are consumed.
    inline bool SolveInPlace(std::vector<f64>& A, std::vector<f64>& b, u32 n)
    {
        for (u32 col = 0; col < n; ++col) {
            // Partial pivot: largest magnitude in this column at or below the diagonal.
            u32 pivot = col;
            f64 best = std::fabs(A[col * n + col]);
            for (u32 r = col + 1; r < n; ++r) {
                f64 v = std::fabs(A[r * n + col]);
                if (v > best) {
                    best = v;
                    pivot = r;
                }
            }
            if (best < 1e-14) {
                return false; // singular
            }
            if (pivot != col) {
                for (u32 k = 0; k < n; ++k) {
                    std::swap(A[col * n + k], A[pivot * n + k]);
                }
                std::swap(b[col], b[pivot]);
            }
            // Eliminate below.
            f64 diag = A[col * n + col];
            for (u32 r = col + 1; r < n; ++r) {
                f64 factor = A[r * n + col] / diag;
                if (factor == 0.0) {
                    continue;
                }
                for (u32 k = col; k < n; ++k) {
                    A[r * n + k] -= factor * A[col * n + k];
                }
                b[r] -= factor * b[col];
            }
        }
        // Back-substitute.
        for (u32 i = n; i-- > 0;) {
            f64 sum = b[i];
            for (u32 k = i + 1; k < n; ++k) {
                sum -= A[i * n + k] * b[k];
            }
            b[i] = sum / A[i * n + i];
        }
        return true;
    }

    // Numerical rank of an m×n matrix (row-major) by row-echelon reduction with partial
    // pivoting. Used for degree-of-freedom analysis.
    inline u32 Rank(std::vector<f64> M, u32 rows, u32 cols)
    {
        constexpr f64 kTol = 1e-7;
        u32 rank = 0;
        for (u32 col = 0; col < cols && rank < rows; ++col) {
            u32 pivot = rank;
            f64 best = std::fabs(M[rank * cols + col]);
            for (u32 r = rank + 1; r < rows; ++r) {
                f64 v = std::fabs(M[r * cols + col]);
                if (v > best) {
                    best = v;
                    pivot = r;
                }
            }
            if (best < kTol) {
                continue; // no pivot in this column
            }
            for (u32 k = 0; k < cols; ++k) {
                std::swap(M[rank * cols + k], M[pivot * cols + k]);
            }
            f64 diag = M[rank * cols + col];
            for (u32 r = 0; r < rows; ++r) {
                if (r == rank) {
                    continue;
                }
                f64 factor = M[r * cols + col] / diag;
                if (factor == 0.0) {
                    continue;
                }
                for (u32 k = col; k < cols; ++k) {
                    M[r * cols + k] -= factor * M[rank * cols + k];
                }
            }
            ++rank;
        }
        return rank;
    }

} // namespace la

// ── the variable layout ──────────────────────────────────────────────────────
// The unknowns are POINTS, 2 vars each, and nothing else: a circle's radius and an arc's
// radius/angles are DERIVED from points, not solved scalars. This remembers where each
// point's variables live so residuals can read them by point id and the solution writes
// back to the points.
struct VarLayout {
    static constexpr u32 kNoIdx = u32_max;

    struct PVar {
        FeatureId id { kNullFeature }; // GeomPoint id
        u32 base { 0 }; // its x is at base, y at base+1
    };

    std::vector<f64> x; // live variable values
    std::vector<f64> drawn; // as-drawn snapshot (Ground pins a point to its value here)
    std::vector<PVar> pvars;

    const PVar* PointVarOf(FeatureId pid) const
    {
        for (const PVar& p : pvars) {
            if (p.id == pid) {
                return &p;
            }
        }
        return nullptr;
    }

    // The variable base of a point referenced directly by id (Coincident/Ground).
    u32 PointBaseById(FeatureId pid) const
    {
        const PVar* pv = PointVarOf(pid);
        return pv ? pv->base : kNoIdx;
    }

    // The variable base for a constraint's (curve, PointRef) reference: a line's start/end
    // point, or a circle/arc centre. kNoIdx if unresolvable. Used by the curve-referencing
    // constraints (Horizontal, Parallel, …); point-referencing ones use PointBaseById.
    u32 PointBase(const SketchDocument& doc, FeatureId curveId, PointRef which) const
    {
        const SketchEntity* e = doc.Find(curveId);
        if (!e) {
            return kNoIdx;
        }
        FeatureId pid = (e->kind == EntityKind::Line && which == PointRef::End) ? e->p1 : e->p0;
        return PointBaseById(pid);
    }

    void Build(const SketchDocument& doc)
    {
        u32 n = 0;
        for (const GeomPoint& p : doc.points) {
            pvars.push_back({ p.id, n });
            n += 2;
        }

        x.assign(n, 0.0);
        for (const PVar& pv : pvars) {
            const GeomPoint* p = doc.FindPoint(pv.id);
            if (p) {
                x[pv.base] = p->pos.x;
                x[pv.base + 1] = p->pos.y;
            }
        }
        drawn = x;
    }

    void WriteBack(SketchDocument& doc) const
    {
        for (const PVar& pv : pvars) {
            GeomPoint* p = doc.FindPoint(pv.id);
            if (p) {
                p->pos = { x[pv.base], x[pv.base + 1] };
            }
        }
        doc.RefreshDerived(); // recompute every curve's centre/radius/angles from the points
    }
};

// Options — all deterministic, all with sane defaults.
struct SolveOptions {
    u32 maxIterations { 100 };
    f64 tolerance { 1e-9 }; // convergence on ‖r‖ and on the step norm
};

// One residual equation r_i(x) = 0. `owner` is the constraint or dimension it came from,
// so a conflict can be named back to the feature the user made.
struct Residual {
    FeatureId owner { kNullFeature };
    std::function<f64(const std::vector<f64>&)> f;
};

class ConstraintSolver {
public:
    explicit ConstraintSolver(SolveOptions opts = {})
        : opts_ { opts }
    {
    }

    // Solve `doc` in place and return the diagnostics (also stored on doc.lastSolve by
    // ConstraintSketchSolver).
    SketchSolveResult Solve(SketchDocument& doc) const
    {
        VarLayout vars;
        vars.Build(doc);

        SketchSolveResult result;
        if (vars.x.empty()) {
            result.status = SketchSolveStatus::WellConstrained; // nothing to solve
            return result;
        }

        std::vector<Residual> residuals = BuildResiduals(doc, vars);

        // Run Levenberg-Marquardt.
        result.iterations = RunLM(vars, residuals);

        vars.WriteBack(doc);

        Diagnose(vars, residuals, result);
        return result;
    }

    // How many independent directions a point can still move in (0, 1, or 2), given the
    // sketch's constraints. This is exactly the freedom a drag has: 0 = pinned (a handle
    // shouldn't offer to move it), 1 = can slide along a line, 2 = fully free in the plane.
    //
    // Computed by how much pinning the point would RAISE the constraint Jacobian's rank:
    // if pinning it adds an independent equation, that direction was free; if pinning is
    // redundant, the constraints already determined it. Evaluated at `doc`'s current
    // (already-solved) geometry.
    u32 PointFreedom(const SketchDocument& doc, FeatureId entity, PointRef which) const
    {
        VarLayout vars;
        vars.Build(doc);
        u32 base = vars.PointBase(doc, entity, which);
        if (base == VarLayout::kNoIdx) {
            return 0;
        }
        u32 xi = base;
        u32 yi = base + 1;

        std::vector<Residual> rs = BuildResiduals(doc, vars);
        if (rs.empty()) {
            return 2; // no constraints at all — the point is free in the plane
        }

        u32 n = static_cast<u32>(vars.x.size());
        u32 m = static_cast<u32>(rs.size());
        std::vector<f64> J;
        std::vector<f64> xCopy = vars.x;
        Jacobian(rs, xCopy, J); // m×n
        u32 r0 = la::Rank(J, m, n);

        // Append two rows pinning the point's x and y, and see how far the rank rose.
        std::vector<f64> J2 = J;
        J2.resize(static_cast<size_t>(m + 2) * n, 0.0);
        J2[static_cast<size_t>(m) * n + xi] = 1.0;
        J2[static_cast<size_t>(m + 1) * n + yi] = 1.0;
        u32 r1 = la::Rank(J2, m + 2, n);

        return r1 - r0;
    }

private:
    SolveOptions opts_;

    // ── residual construction ─────────────────────────────────────────────────
    // All references resolve to POINT variable bases at build time (via
    // vars.PointBase(doc, curve, PointRef)); the lambdas capture those indices, so
    // evaluation never re-walks the document. A base of kNoIdx means the reference didn't
    // resolve — that residual is skipped, leaving the geometry as drawn.
    static constexpr u32 kNoIdx = VarLayout::kNoIdx;

    std::vector<Residual> BuildResiduals(const SketchDocument& doc, const VarLayout& vars) const
    {
        std::vector<Residual> rs;

        // Dimensions.
        for (const SketchDimensionRecord& d : doc.dimensions) {
            if (!doc.params) {
                continue;
            }
            Param::EvalResult ev = doc.params->Value(d.value);
            if (!ev.Ok()) {
                continue; // broken expression -> no residual, geometry stays as drawn
            }
            AddDimension(rs, doc, vars, d, ev.value.value); // base units (mm / radians)
        }

        // Constraints.
        for (const SketchConstraintRecord& c : doc.constraints) {
            AddConstraint(rs, doc, vars, c);
        }

        // Arc equal-radius invariant, always on: an arc's start and end must stay the same
        // distance from its centre, so its radius is one quantity however the points move
        // and its endpoints are ordinary points that can coincide with neighbours. Owner is
        // the arc, so if it ever conflicts the arc is the feature named.
        for (const SketchEntity& e : doc.entities) {
            if (e.kind != EntityKind::Arc) {
                continue;
            }
            u32 ctr = vars.PointBaseById(e.p0);
            u32 s = vars.PointBaseById(e.p1);
            u32 en = vars.PointBaseById(e.p2);
            if (ctr == kNoIdx || s == kNoIdx || en == kNoIdx) {
                continue;
            }
            FeatureId owner = e.id;
            rs.push_back({ owner, [ctr, s, en](const std::vector<f64>& x) {
                              f64 rs = std::hypot(x[s] - x[ctr], x[s + 1] - x[ctr + 1]);
                              f64 re = std::hypot(x[en] - x[ctr], x[en + 1] - x[ctr + 1]);
                              return rs - re;
                          } });
        }

        // No anchor residuals. The iteration takes MINIMUM-NORM steps (Δx = Jᵀy), which
        // live in the row space of the constraint Jacobian and so never move the null
        // space — free degrees of freedom stay exactly as drawn. See RunLM.
        return rs;
    }

    static void AddDimension(std::vector<Residual>& rs, const SketchDocument& doc,
        const VarLayout& vars, const SketchDimensionRecord& d, f64 target)
    {
        const SketchEntity* ea = doc.Find(d.targetA);
        if (!ea) {
            return;
        }
        FeatureId owner = d.id;

        switch (d.kind) {
        case DimensionKind::Length: {
            u32 s = vars.PointBase(doc, d.targetA, PointRef::Start);
            u32 e = vars.PointBase(doc, d.targetA, PointRef::End);
            if (ea->kind != EntityKind::Line || s == kNoIdx || e == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [s, e, target](const std::vector<f64>& x) {
                              f64 dx = x[e] - x[s];
                              f64 dy = x[e + 1] - x[s + 1];
                              return std::sqrt(dx * dx + dy * dy) - target;
                          } });
            break;
        }
        case DimensionKind::Radius: {
            // Radius is now the distance from the centre point (p0) to the rim/start point
            // (p1) — a circle's rim or an arc's start. The arc invariant carries the end.
            if (ea->kind != EntityKind::Circle && ea->kind != EntityKind::Arc) {
                return;
            }
            u32 ctr = vars.PointBaseById(ea->p0);
            u32 rim = vars.PointBaseById(ea->p1);
            if (ctr == kNoIdx || rim == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [ctr, rim, target](const std::vector<f64>& x) {
                              f64 dx = x[rim] - x[ctr];
                              f64 dy = x[rim + 1] - x[ctr + 1];
                              return std::sqrt(dx * dx + dy * dy) - target;
                          } });
            break;
        }
        case DimensionKind::Angle: {
            u32 s = vars.PointBase(doc, d.targetA, PointRef::Start);
            u32 e = vars.PointBase(doc, d.targetA, PointRef::End);
            if (ea->kind != EntityKind::Line || s == kNoIdx || e == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [s, e, target](const std::vector<f64>& x) {
                              f64 ang = std::atan2(x[e + 1] - x[s + 1], x[e] - x[s]);
                              f64 diff = ang - target;
                              while (diff > 3.14159265358979323846) {
                                  diff -= 2.0 * 3.14159265358979323846;
                              }
                              while (diff <= -3.14159265358979323846) {
                                  diff += 2.0 * 3.14159265358979323846;
                              }
                              return diff;
                          } });
            break;
        }
        case DimensionKind::Distance: {
            // Between the two entities' anchor points (line start / circle centre).
            u32 a = vars.PointBase(doc, d.targetA, PointRef::Start);
            u32 b = vars.PointBase(doc, d.targetB, PointRef::Start);
            if (a == kNoIdx || b == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [a, b, target](const std::vector<f64>& x) {
                              f64 dx = x[b] - x[a];
                              f64 dy = x[b + 1] - x[a + 1];
                              return std::sqrt(dx * dx + dy * dy) - target;
                          } });
            break;
        }
        }
    }

    // Resolve an id that names "a point" for a Coincident/Ground constraint. In the
    // point-based model the id IS a point (the precise auto-coincident and drag-produced
    // case). For back-compat with a manual tool that picked a whole curve, an id that is a
    // curve instead resolves to that curve's start point. kNoIdx if neither.
    static u32 ResolveEndpoint(const SketchDocument& doc, const VarLayout& vars, FeatureId id)
    {
        u32 base = vars.PointBaseById(id);
        if (base != kNoIdx) {
            return base; // it's a point
        }
        return vars.PointBase(doc, id, PointRef::Start); // it's a curve → its start point
    }

    static void AddConstraint(std::vector<Residual>& rs, const SketchDocument& doc,
        const VarLayout& vars, const SketchConstraintRecord& c)
    {
        FeatureId owner = c.id;
        const SketchEntity* ea = doc.Find(c.a);

        switch (c.kind) {
        case ConstraintKind::Coincident: {
            // a/b resolve as a point id if they are one (the precise auto-coincident case),
            // otherwise as the referenced curve's start point (a manual tool that picked a
            // line and means its start).
            u32 a = ResolveEndpoint(doc, vars, c.a);
            u32 b = ResolveEndpoint(doc, vars, c.b);
            if (a == kNoIdx || b == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [a, b](const std::vector<f64>& x) { return x[a] - x[b]; } });
            rs.push_back({ owner, [a, b](const std::vector<f64>& x) { return x[a + 1] - x[b + 1]; } });
            break;
        }
        case ConstraintKind::Horizontal:
        case ConstraintKind::Vertical: {
            u32 s = vars.PointBase(doc, c.a, PointRef::Start);
            u32 e = vars.PointBase(doc, c.a, PointRef::End);
            if (!ea || ea->kind != EntityKind::Line || s == kNoIdx || e == kNoIdx) {
                return;
            }
            bool vertical = c.kind == ConstraintKind::Vertical;
            rs.push_back({ owner, [s, e, vertical](const std::vector<f64>& x) {
                              return vertical ? (x[e] - x[s]) : (x[e + 1] - x[s + 1]);
                          } });
            break;
        }
        case ConstraintKind::Parallel:
        case ConstraintKind::Perpendicular: {
            const SketchEntity* eb = doc.Find(c.b);
            u32 s1 = vars.PointBase(doc, c.a, PointRef::Start), e1 = vars.PointBase(doc, c.a, PointRef::End);
            u32 s2 = vars.PointBase(doc, c.b, PointRef::Start), e2 = vars.PointBase(doc, c.b, PointRef::End);
            if (!ea || !eb || ea->kind != EntityKind::Line || eb->kind != EntityKind::Line
                || s1 == kNoIdx || e1 == kNoIdx || s2 == kNoIdx || e2 == kNoIdx) {
                return;
            }
            bool perp = c.kind == ConstraintKind::Perpendicular;
            rs.push_back({ owner, [s1, e1, s2, e2, perp](const std::vector<f64>& x) {
                              f64 d1x = x[e1] - x[s1], d1y = x[e1 + 1] - x[s1 + 1];
                              f64 d2x = x[e2] - x[s2], d2y = x[e2 + 1] - x[s2 + 1];
                              return perp ? (d1x * d2x + d1y * d2y) : (d1x * d2y - d1y * d2x);
                          } });
            break;
        }
        case ConstraintKind::Equal: {
            const SketchEntity* eb = doc.Find(c.b);
            u32 s1 = vars.PointBase(doc, c.a, PointRef::Start), e1 = vars.PointBase(doc, c.a, PointRef::End);
            u32 s2 = vars.PointBase(doc, c.b, PointRef::Start), e2 = vars.PointBase(doc, c.b, PointRef::End);
            if (!ea || !eb || ea->kind != EntityKind::Line || eb->kind != EntityKind::Line
                || s1 == kNoIdx || e1 == kNoIdx || s2 == kNoIdx || e2 == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [s1, e1, s2, e2](const std::vector<f64>& x) {
                              f64 l1 = std::hypot(x[e1] - x[s1], x[e1 + 1] - x[s1 + 1]);
                              f64 l2 = std::hypot(x[e2] - x[s2], x[e2 + 1] - x[s2 + 1]);
                              return l1 - l2;
                          } });
            break;
        }
        case ConstraintKind::Tangent: {
            const SketchEntity* eb = doc.Find(c.b); // the circle/arc
            u32 s = vars.PointBase(doc, c.a, PointRef::Start), e = vars.PointBase(doc, c.a, PointRef::End);
            // Radius is now derived from the circle/arc's own points: centre = p0, rim = p1.
            u32 ctr = eb ? vars.PointBaseById(eb->p0) : kNoIdx;
            u32 rim = eb ? vars.PointBaseById(eb->p1) : kNoIdx;
            if (!ea || !eb || ea->kind != EntityKind::Line || eb->kind == EntityKind::Line
                || s == kNoIdx || e == kNoIdx || ctr == kNoIdx || rim == kNoIdx) {
                return;
            }
            rs.push_back({ owner, [s, e, ctr, rim](const std::vector<f64>& x) {
                              // distance(centre, infinite line)² − radius², squared to avoid
                              // the |.| kink at the solution. radius² = ‖centre − rim‖².
                              f64 dx = x[e] - x[s];
                              f64 dy = x[e + 1] - x[s + 1];
                              f64 lenSq = dx * dx + dy * dy;
                              if (lenSq < 1e-12) {
                                  return 0.0;
                              }
                              f64 cross = dx * (x[ctr + 1] - x[s + 1]) - dy * (x[ctr] - x[s]);
                              f64 rx = x[rim] - x[ctr];
                              f64 ry = x[rim + 1] - x[ctr + 1];
                              f64 radiusSq = rx * rx + ry * ry;
                              return (cross * cross) / lenSq - radiusSq;
                          } });
            break;
        }
        case ConstraintKind::Ground: {
            // a resolves as a point id if it is one, else as the referenced curve's start
            // point (a manual Ground tool that picked a whole line).
            u32 base = ResolveEndpoint(doc, vars, c.a);
            if (base == kNoIdx) {
                return;
            }
            // Pin the point to its as-drawn location (captured now, from vars.drawn).
            f64 gx = vars.drawn[base];
            f64 gy = vars.drawn[base + 1];
            rs.push_back({ owner, [base, gx](const std::vector<f64>& x) { return x[base] - gx; } });
            rs.push_back({ owner, [base, gy](const std::vector<f64>& x) { return x[base + 1] - gy; } });
            break;
        }
        case ConstraintKind::Symmetry: {
            const SketchEntity* eb = doc.Find(c.b);
            const SketchEntity* eax = doc.Find(c.axis);
            u32 aS = vars.PointBase(doc, c.a, PointRef::Start), aE = vars.PointBase(doc, c.a, PointRef::End);
            u32 bS = vars.PointBase(doc, c.b, PointRef::Start), bE = vars.PointBase(doc, c.b, PointRef::End);
            u32 xS = vars.PointBase(doc, c.axis, PointRef::Start), xE = vars.PointBase(doc, c.axis, PointRef::End);
            if (!ea || !eb || !eax || ea->kind != EntityKind::Line || eb->kind != EntityKind::Line
                || eax->kind != EntityKind::Line || aS == kNoIdx || aE == kNoIdx || bS == kNoIdx
                || bE == kNoIdx || xS == kNoIdx || xE == kNoIdx) {
                return;
            }
            // b's start/end must be the reflection of a's start/end across the axis.
            u32 aPts[2] = { aS, aE };
            u32 bPts[2] = { bS, bE };
            for (u32 pt = 0; pt < 2; ++pt) {
                u32 ap = aPts[pt];
                u32 bp = bPts[pt];
                for (u32 comp = 0; comp < 2; ++comp) {
                    rs.push_back({ owner, [ap, bp, xS, xE, comp](const std::vector<f64>& x) {
                                      f64 rx = 0, ry = 0;
                                      ReflectAcrossAxis(x, xS, xE, x[ap], x[ap + 1], rx, ry);
                                      return comp == 0 ? (rx - x[bp]) : (ry - x[bp + 1]);
                                  } });
                }
            }
            break;
        }
        }
    }

    // Reflect (px,py) across the infinite line through the axis points at bases axA, axB.
    static void ReflectAcrossAxis(const std::vector<f64>& x, u32 axA, u32 axB, f64 px, f64 py, f64& rx, f64& ry)
    {
        f64 ax = x[axA], ay = x[axA + 1];
        f64 dx = x[axB] - ax, dy = x[axB + 1] - ay;
        f64 lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-12) {
            rx = px;
            ry = py;
            return;
        }
        f64 t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
        f64 projx = ax + t * dx;
        f64 projy = ay + t * dy;
        rx = 2.0 * projx - px;
        ry = 2.0 * projy - py;
    }

    // ── the iteration ─────────────────────────────────────────────────────────
    static void Eval(const std::vector<Residual>& rs, const std::vector<f64>& x, std::vector<f64>& out)
    {
        out.resize(rs.size());
        for (u32 i = 0; i < rs.size(); ++i) {
            out[i] = rs[i].f(x);
        }
    }

    static f64 SumSq(const std::vector<f64>& v)
    {
        f64 s = 0;
        for (f64 e : v) {
            s += e * e;
        }
        return s;
    }

    static bool Finite(const std::vector<f64>& v)
    {
        for (f64 e : v) {
            if (!std::isfinite(e)) {
                return false;
            }
        }
        return true;
    }

    // Central-difference Jacobian: J[i*n + j] = d r_i / d x_j.
    static void Jacobian(const std::vector<Residual>& rs, std::vector<f64>& x, std::vector<f64>& J)
    {
        u32 m = static_cast<u32>(rs.size());
        u32 n = static_cast<u32>(x.size());
        J.assign(static_cast<size_t>(m) * n, 0.0);
        std::vector<f64> rp(m), rm(m);
        for (u32 j = 0; j < n; ++j) {
            f64 h = 1e-6 * (1.0 + std::fabs(x[j]));
            f64 saved = x[j];
            x[j] = saved + h;
            Eval(rs, x, rp);
            x[j] = saved - h;
            Eval(rs, x, rm);
            x[j] = saved;
            f64 inv = 1.0 / (2.0 * h);
            for (u32 i = 0; i < m; ++i) {
                J[static_cast<size_t>(i) * n + j] = (rp[i] - rm[i]) * inv;
            }
        }
    }

    // Damped minimum-norm Gauss-Newton (Levenberg-Marquardt in the DUAL). Each step
    // solves (J Jᵀ + λI) y = -r for y (an m×m system), then takes Δx = Jᵀy. That step is
    // the SMALLEST change to x that reduces the constraint residual, which is what makes
    // free DOFs stay put and a dimension drag geometry the least it can. λ damps the step
    // and regularizes a rank-deficient J Jᵀ (redundant or conflicting constraints), so a
    // singular system never throws — it just damps harder.
    u32 RunLM(VarLayout& vars, const std::vector<Residual>& rs) const
    {
        std::vector<f64>& x = vars.x;
        u32 n = static_cast<u32>(x.size());
        u32 m = static_cast<u32>(rs.size());
        if (m == 0) {
            return 0; // no constraints: geometry stays exactly as drawn
        }

        std::vector<f64> r(m), J, JJt(static_cast<size_t>(m) * m), xNew(n), rNew(m);
        Eval(rs, x, r);
        if (!Finite(r)) {
            return 0;
        }
        f64 cost = SumSq(r);
        f64 lambda = 1e-6;

        u32 iter = 0;
        for (; iter < opts_.maxIterations; ++iter) {
            if (std::sqrt(cost) < opts_.tolerance) {
                break;
            }
            Jacobian(rs, x, J); // m×n

            // J Jᵀ  (m×m, symmetric).
            for (u32 a = 0; a < m; ++a) {
                for (u32 b = 0; b < m; ++b) {
                    f64 s = 0;
                    for (u32 k = 0; k < n; ++k) {
                        s += J[static_cast<size_t>(a) * n + k] * J[static_cast<size_t>(b) * n + k];
                    }
                    JJt[static_cast<size_t>(a) * m + b] = s;
                }
            }

            bool stepped = false;
            f64 stepNorm = 0;
            for (u32 tries = 0; tries < 16; ++tries) {
                std::vector<f64> M = JJt;
                for (u32 d = 0; d < m; ++d) {
                    M[static_cast<size_t>(d) * m + d] += lambda;
                }
                std::vector<f64> y(m);
                for (u32 i = 0; i < m; ++i) {
                    y[i] = -r[i];
                }
                if (!la::SolveInPlace(M, y, m)) {
                    lambda *= 4.0;
                    continue;
                }
                // Δx = Jᵀ y.
                stepNorm = 0;
                for (u32 k = 0; k < n; ++k) {
                    f64 dxk = 0;
                    for (u32 i = 0; i < m; ++i) {
                        dxk += J[static_cast<size_t>(i) * n + k] * y[i];
                    }
                    xNew[k] = x[k] + dxk;
                    stepNorm += dxk * dxk;
                }
                Eval(rs, xNew, rNew);
                f64 costNew = SumSq(rNew);
                if (Finite(rNew) && costNew < cost) {
                    x = xNew;
                    r = rNew;
                    cost = costNew;
                    lambda = std::fmax(lambda * 0.5, 1e-12);
                    stepped = true;
                    break;
                }
                lambda *= 4.0;
                if (lambda > 1e14) {
                    break;
                }
            }

            if (!stepped) {
                break; // can't reduce further (converged, or genuinely conflicting)
            }
            if (std::sqrt(stepNorm) < opts_.tolerance) {
                break;
            }
        }
        return iter;
    }

    // ── diagnostics ───────────────────────────────────────────────────────────
    void Diagnose(const VarLayout& vars, const std::vector<Residual>& rs, SketchSolveResult& out) const
    {
        u32 n = static_cast<u32>(vars.x.size());
        u32 mHard = static_cast<u32>(rs.size());

        // Residual values at the solution.
        std::vector<f64> hardVals(mHard);
        std::vector<u32> hardOwners(mHard);
        f64 hardNorm = 0;
        for (u32 i = 0; i < mHard; ++i) {
            hardVals[i] = rs[i].f(vars.x);
            hardOwners[i] = rs[i].owner;
            hardNorm += hardVals[i] * hardVals[i];
        }
        hardNorm = std::sqrt(hardNorm);
        out.residualNorm = hardNorm;

        // rank(J) tells how many DOF the constraints pin down.
        u32 rank = 0;
        if (mHard > 0) {
            std::vector<f64> Jh;
            std::vector<f64> xCopy = vars.x;
            Jacobian(rs, xCopy, Jh);
            rank = la::Rank(Jh, mHard, n);
        }

        out.dof = n > rank ? n - rank : 0;
        u32 redundant = mHard > rank ? mHard - rank : 0;

        const f64 kSatisfied = 1e-5;
        bool converged = hardNorm < kSatisfied;

        auto collectConflicts = [&]() {
            for (u32 i = 0; i < mHard; ++i) {
                if (std::fabs(hardVals[i]) > kSatisfied && hardOwners[i] != kNullFeature) {
                    bool seen = false;
                    for (FeatureId f : out.conflicting) {
                        if (f == hardOwners[i]) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen) {
                        out.conflicting.push_back(hardOwners[i]);
                    }
                }
            }
        };

        if (!converged) {
            out.status = redundant > 0 ? SketchSolveStatus::OverConstrained
                                       : SketchSolveStatus::DidNotConverge;
            collectConflicts();
        } else if (redundant > 0) {
            // A redundant (dependent) constraint is flagged even when other DOFs remain:
            // it is an authoring error the user should remove, and takes precedence over
            // "under-constrained" in the report.
            out.status = SketchSolveStatus::OverConstrained; // redundant but consistent
        } else if (out.dof > 0) {
            out.status = SketchSolveStatus::UnderConstrained;
        } else {
            out.status = SketchSolveStatus::WellConstrained;
        }
    }
};

} // namespace Solver

// The ISketchSolver implementation. Owns options, delegates to the engine, and records
// the diagnostics on the document so the UI can read them.
struct ConstraintSketchSolver : ISketchSolver {
    Solver::SolveOptions options {};

    void Solve(SketchDocument& doc) const override
    {
        Solver::ConstraintSolver engine { options };
        doc.lastSolve = engine.Solve(doc);
    }
};

// How free a point is (0/1/2 DOF) at the sketch's current geometry — for the canvas drag
// handles. A stateless one-off query; cheap for sketch-scale systems.
inline u32 SketchPointFreedom(const SketchDocument& doc, FeatureId entity, PointRef which)
{
    Solver::ConstraintSolver engine;
    return engine.PointFreedom(doc, entity, which);
}

// The default solver behind the seam. Dispatches on whether the sketch has any geometric
// constraints:
//
//   * No constraints (dimensions only, or nothing) → DirectSketchSolver: closed-form,
//     predictable, and it extends a dimensioned line from its start rather than
//     re-centering it — the drag behaviour the app has always had, and the reason the
//     dimension tests still hold.
//   * Any constraint present → the full ConstraintSolver, which handles the dimensions
//     too, so a mixed sketch is solved as one coupled system.
//
// The plan sanctioned keeping DirectSketchSolver as the fast path for the trivial case;
// this is that dispatch. Nothing above the seam knows which ran.
struct HybridSketchSolver : ISketchSolver {
    DirectSketchSolver direct {};
    ConstraintSketchSolver constraint {};

    void Solve(SketchDocument& doc) const override
    {
        if (doc.constraints.empty()) {
            direct.Solve(doc);
            doc.lastSolve = {}; // no constraint system to analyse
        } else {
            constraint.Solve(doc);
        }
    }
};
