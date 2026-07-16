#pragma once
#include "SketchDocument.h"

// The seam between "a sketch's commands have been replayed" and "the sketch's geometry
// satisfies its dimensions and constraints".
//
// Replay (SketchCmd::execute) lays down entities at the coordinates the user clicked.
// Solve() then makes the DIMENSIONED ones obey their dimensions. An entity with no
// dimension is left exactly where it was drawn — it can be measured, but nothing
// dictates it. That is the whole distinction this interface exists to enforce.
//
// Two implementations are planned and interchangeable:
//   * DirectSketchSolver  — closed-form, non-iterative: a length dimension slides the
//                           line's endpoint along its own direction, a radius sets the
//                           radius. Deterministic and cheap; handles the common cases.
//   * (later) a real constraint solver — DOF analysis + Newton-Raphson over a residual
//                           system, with over/under-constrained detection and geometric
//                           constraints (coincident, parallel, tangent, symmetry).
//
// Everything upstream (commands, contexts, tools, UI, parameters) talks to dimensions
// only through this interface, so swapping the implementation touches nothing else.
struct ISketchSolver {
    virtual ~ISketchSolver() = default;

    // Adjust `doc` in place so its dimensioned entities match their dimensions.
    // Called once per sketch per recompute, after every command has executed.
    virtual void Solve(SketchDocument& doc) const = 0;
};

// The no-op solver: replay only, dimensions recorded but inert. What the system uses
// before a real solver is wired in, and useful in tests that want to assert on raw
// as-drawn geometry.
struct NullSketchSolver : ISketchSolver {
    void Solve(SketchDocument&) const override { }
};
