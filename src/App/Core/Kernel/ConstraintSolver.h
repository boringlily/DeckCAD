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
// Flattens every entity's degrees of freedom into one vector x, and remembers where
// each entity's variables live so residuals can read them and the solution can be
// written back.
struct VarLayout {
    struct Ent {
        FeatureId id { kNullFeature };
        EntityKind kind { EntityKind::Line };
        u32 base { 0 }; // index of this entity's first variable in x
    };

    std::vector<f64> x; // live variable values
    std::vector<f64> drawn; // as-drawn snapshot (Ground pins a point to its value here)
    std::vector<Ent> ents;

    static u32 DofOf(EntityKind k)
    {
        switch (k) {
        case EntityKind::Line:
            return 4; // a.x a.y b.x b.y
        case EntityKind::Circle:
            return 3; // c.x c.y r
        case EntityKind::Arc:
            return 5; // c.x c.y r start end
        }
        return 0;
    }

    const Ent* Of(FeatureId id) const
    {
        for (const Ent& e : ents) {
            if (e.id == id) {
                return &e;
            }
        }
        return nullptr;
    }

    void Build(const SketchDocument& doc)
    {
        u32 n = 0;
        for (const SketchEntity& e : doc.entities) {
            ents.push_back({ e.id, e.kind, n });
            n += DofOf(e.kind);
        }
        x.assign(n, 0.0);
        for (u32 k = 0; k < doc.entities.size(); ++k) {
            const SketchEntity& e = doc.entities[k];
            u32 b = ents[k].base;
            switch (e.kind) {
            case EntityKind::Line:
                x[b] = e.a.x;
                x[b + 1] = e.a.y;
                x[b + 2] = e.b.x;
                x[b + 3] = e.b.y;
                break;
            case EntityKind::Circle:
                x[b] = e.a.x;
                x[b + 1] = e.a.y;
                x[b + 2] = e.radius;
                break;
            case EntityKind::Arc:
                x[b] = e.a.x;
                x[b + 1] = e.a.y;
                x[b + 2] = e.radius;
                x[b + 3] = e.startAngle;
                x[b + 4] = e.endAngle;
                break;
            }
        }
        drawn = x;
    }

    void WriteBack(SketchDocument& doc) const
    {
        for (const Ent& en : ents) {
            SketchEntity* e = doc.Find(en.id);
            if (!e) {
                continue;
            }
            u32 b = en.base;
            switch (e->kind) {
            case EntityKind::Line:
                e->a = { x[b], x[b + 1] };
                e->b = { x[b + 2], x[b + 3] };
                break;
            case EntityKind::Circle:
                e->a = { x[b], x[b + 1] };
                e->radius = x[b + 2];
                break;
            case EntityKind::Arc:
                e->a = { x[b], x[b + 1] };
                e->radius = x[b + 2];
                e->startAngle = x[b + 3];
                e->endAngle = x[b + 4];
                break;
            }
        }
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
        const VarLayout::Ent* e = vars.Of(entity);
        if (!e) {
            return 0;
        }
        u32 xi = e->base;
        u32 yi = e->base + 1;
        if (e->kind == EntityKind::Line && which == PointRef::End) {
            xi = e->base + 2;
            yi = e->base + 3;
        }

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
    static f64 PointX(const std::vector<f64>& x, const VarLayout& v, FeatureId id, PointRef which)
    {
        const VarLayout::Ent* e = v.Of(id);
        if (!e) {
            return 0.0;
        }
        if (e->kind == EntityKind::Line) {
            return which == PointRef::End ? x[e->base + 2] : x[e->base];
        }
        return x[e->base]; // circle/arc centre
    }
    static f64 PointY(const std::vector<f64>& x, const VarLayout& v, FeatureId id, PointRef which)
    {
        const VarLayout::Ent* e = v.Of(id);
        if (!e) {
            return 0.0;
        }
        if (e->kind == EntityKind::Line) {
            return which == PointRef::End ? x[e->base + 3] : x[e->base + 1];
        }
        return x[e->base + 1];
    }

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
            f64 target = ev.value.value; // base units (mm / radians)
            AddDimension(rs, vars, d, target);
        }

        // Constraints.
        for (const SketchConstraintRecord& c : doc.constraints) {
            AddConstraint(rs, vars, c);
        }

        // No anchor residuals. The iteration takes MINIMUM-NORM steps (Δx = Jᵀy), which
        // live in the row space of the constraint Jacobian and so never move the null
        // space — free degrees of freedom stay exactly as drawn, and a lone length
        // dimension stretches a line along its own direction rather than rotating it,
        // all without an anchor term biasing the constraints. See RunLM.
        return rs;
    }

    static void AddDimension(std::vector<Residual>& rs, const VarLayout& vars,
        const SketchDimensionRecord& d, f64 target)
    {
        const VarLayout::Ent* ea = vars.Of(d.targetA);
        if (!ea) {
            return;
        }
        FeatureId owner = d.id;

        switch (d.kind) {
        case DimensionKind::Length: {
            if (ea->kind != EntityKind::Line) {
                return;
            }
            u32 b = ea->base;
            rs.push_back({ owner, [b, target](const std::vector<f64>& x) {
                              f64 dx = x[b + 2] - x[b];
                              f64 dy = x[b + 3] - x[b + 1];
                              return std::sqrt(dx * dx + dy * dy) - target;
                          } });
            break;
        }
        case DimensionKind::Radius: {
            if (ea->kind == EntityKind::Line) {
                return;
            }
            u32 b = ea->base;
            rs.push_back({ owner, [b, target](const std::vector<f64>& x) { return x[b + 2] - target; } });
            break;
        }
        case DimensionKind::Angle: {
            if (ea->kind != EntityKind::Line) {
                return;
            }
            u32 b = ea->base;
            rs.push_back({ owner, [b, target](const std::vector<f64>& x) {
                              f64 ang = std::atan2(x[b + 3] - x[b + 1], x[b + 2] - x[b]);
                              f64 diff = ang - target;
                              // wrap to (-pi, pi] so the residual stays continuous.
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
            const VarLayout::Ent* eb = vars.Of(d.targetB);
            if (!eb) {
                return;
            }
            // Between the entities' anchor points (line start / circle centre).
            u32 ba = ea->base;
            u32 bb = eb->base;
            rs.push_back({ owner, [ba, bb, target](const std::vector<f64>& x) {
                              f64 dx = x[bb] - x[ba];
                              f64 dy = x[bb + 1] - x[ba + 1];
                              return std::sqrt(dx * dx + dy * dy) - target;
                          } });
            break;
        }
        }
    }

    static void AddConstraint(std::vector<Residual>& rs, const VarLayout& vars,
        const SketchConstraintRecord& c)
    {
        FeatureId owner = c.id;
        const VarLayout::Ent* ea = vars.Of(c.a);

        switch (c.kind) {
        case ConstraintKind::Coincident: {
            const VarLayout::Ent* eb = vars.Of(c.b);
            if (!ea || !eb) {
                return;
            }
            FeatureId a = c.a;
            FeatureId b = c.b;
            PointRef pa = c.aPoint;
            PointRef pb = c.bPoint;
            rs.push_back({ owner, [&vars, a, b, pa, pb](const std::vector<f64>& x) {
                              return PointX(x, vars, a, pa) - PointX(x, vars, b, pb);
                          } });
            rs.push_back({ owner, [&vars, a, b, pa, pb](const std::vector<f64>& x) {
                              return PointY(x, vars, a, pa) - PointY(x, vars, b, pb);
                          } });
            break;
        }
        case ConstraintKind::Horizontal: {
            if (!ea || ea->kind != EntityKind::Line) {
                return;
            }
            u32 b = ea->base;
            rs.push_back({ owner, [b](const std::vector<f64>& x) { return x[b + 3] - x[b + 1]; } });
            break;
        }
        case ConstraintKind::Vertical: {
            if (!ea || ea->kind != EntityKind::Line) {
                return;
            }
            u32 b = ea->base;
            rs.push_back({ owner, [b](const std::vector<f64>& x) { return x[b + 2] - x[b]; } });
            break;
        }
        case ConstraintKind::Parallel:
        case ConstraintKind::Perpendicular: {
            const VarLayout::Ent* eb = vars.Of(c.b);
            if (!ea || !eb || ea->kind != EntityKind::Line || eb->kind != EntityKind::Line) {
                return;
            }
            u32 ba = ea->base;
            u32 bb = eb->base;
            bool perp = c.kind == ConstraintKind::Perpendicular;
            rs.push_back({ owner, [ba, bb, perp](const std::vector<f64>& x) {
                              f64 d1x = x[ba + 2] - x[ba];
                              f64 d1y = x[ba + 3] - x[ba + 1];
                              f64 d2x = x[bb + 2] - x[bb];
                              f64 d2y = x[bb + 3] - x[bb + 1];
                              // parallel: cross == 0.   perpendicular: dot == 0.
                              return perp ? (d1x * d2x + d1y * d2y) : (d1x * d2y - d1y * d2x);
                          } });
            break;
        }
        case ConstraintKind::Equal: {
            const VarLayout::Ent* eb = vars.Of(c.b);
            if (!ea || !eb || ea->kind != EntityKind::Line || eb->kind != EntityKind::Line) {
                return;
            }
            u32 ba = ea->base;
            u32 bb = eb->base;
            rs.push_back({ owner, [ba, bb](const std::vector<f64>& x) {
                              f64 l1 = std::hypot(x[ba + 2] - x[ba], x[ba + 3] - x[ba + 1]);
                              f64 l2 = std::hypot(x[bb + 2] - x[bb], x[bb + 3] - x[bb + 1]);
                              return l1 - l2;
                          } });
            break;
        }
        case ConstraintKind::Tangent: {
            const VarLayout::Ent* eb = vars.Of(c.b); // the circle/arc
            if (!ea || !eb || ea->kind != EntityKind::Line || eb->kind == EntityKind::Line) {
                return;
            }
            u32 bl = ea->base;
            u32 bc = eb->base;
            rs.push_back({ owner, [bl, bc](const std::vector<f64>& x) {
                              // distance(centre, infinite line) - radius, in squared form
                              // to avoid the |.| kink at the solution.
                              f64 dx = x[bl + 2] - x[bl];
                              f64 dy = x[bl + 3] - x[bl + 1];
                              f64 lenSq = dx * dx + dy * dy;
                              if (lenSq < 1e-12) {
                                  return 0.0;
                              }
                              f64 cross = dx * (x[bc + 1] - x[bl + 1]) - dy * (x[bc] - x[bl]);
                              f64 distSq = (cross * cross) / lenSq;
                              f64 r = x[bc + 2];
                              return distSq - r * r;
                          } });
            break;
        }
        case ConstraintKind::Ground: {
            if (!ea) {
                return;
            }
            // Pin the point to its as-drawn location (captured now, from vars.drawn).
            f64 gx = PointX(vars.drawn, vars, c.a, c.aPoint);
            f64 gy = PointY(vars.drawn, vars, c.a, c.aPoint);
            FeatureId a = c.a;
            PointRef pa = c.aPoint;
            rs.push_back({ owner, [&vars, a, pa, gx](const std::vector<f64>& x) {
                              return PointX(x, vars, a, pa) - gx;
                          } });
            rs.push_back({ owner, [&vars, a, pa, gy](const std::vector<f64>& x) {
                              return PointY(x, vars, a, pa) - gy;
                          } });
            break;
        }
        case ConstraintKind::Symmetry: {
            const VarLayout::Ent* eb = vars.Of(c.b);
            const VarLayout::Ent* eax = vars.Of(c.axis);
            if (!ea || !eb || !eax || ea->kind != EntityKind::Line
                || eb->kind != EntityKind::Line || eax->kind != EntityKind::Line) {
                return;
            }
            u32 ba = ea->base;
            u32 bb = eb->base;
            u32 bx = eax->base;
            // b must be the reflection of a across the axis line: two points, two comps.
            for (u32 pt = 0; pt < 2; ++pt) {
                for (u32 comp = 0; comp < 2; ++comp) {
                    rs.push_back({ owner, [ba, bb, bx, pt, comp](const std::vector<f64>& x) {
                                      f64 px = x[ba + pt * 2];
                                      f64 py = x[ba + pt * 2 + 1];
                                      f64 rx = 0, ry = 0;
                                      ReflectAcrossAxis(x, bx, px, py, rx, ry);
                                      f64 bpx = x[bb + pt * 2];
                                      f64 bpy = x[bb + pt * 2 + 1];
                                      return comp == 0 ? (rx - bpx) : (ry - bpy);
                                  } });
                }
            }
            break;
        }
        }
    }

    // Reflect (px,py) across the infinite line through the axis entity at base `bx`.
    static void ReflectAcrossAxis(const std::vector<f64>& x, u32 bx, f64 px, f64 py, f64& rx, f64& ry)
    {
        f64 ax = x[bx], ay = x[bx + 1];
        f64 dx = x[bx + 2] - ax, dy = x[bx + 3] - ay;
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
