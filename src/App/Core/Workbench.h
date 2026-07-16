#pragma once
#include "ConstraintSolver.h" // HybridSketchSolver: the default behind the seam.
#include "Context.h"
#include "DTL.h"
#include "Document.h"
#include "ParameterEngine.h"
#include "Tool.h"
#include "ToolId.h"

// One scene's modelling session: the three layers and their wiring, in one owner.
//
//   Document        persistent  — committed history + the geometry recomputed from it
//   ContextStack    session     — where commands land, and which tools are legal
//   Tool            ephemeral   — the current gesture's in-progress state
//   ParameterEngine persistent  — every expression, user- or dimension-authored
//
// The GUI calls into this and reads back from it. It never touches Document directly,
// which is the whole point of the split: an immediate-mode UI rebuilt every frame has no
// business mutating persistent state as a side effect of drawing a button.
//
// Everything here is a value. Workbench is movable (Scene lives in a std::vector, and
// App.dll hot-reloads move state around), which is why ContextStack takes the Document
// per call rather than storing a pointer to it.
class Workbench {
public:
    Workbench()
    {
        document.SetParams(&parameters);
        document.SetSolver(&solver);
    }

    // Rebind the non-owning back-pointers after a move: `other`'s Document now aims at
    // `other`'s dead members. Cheaper and far less error-prone than making Scene
    // non-movable or heap-allocating it.
    Workbench(Workbench&& o) noexcept
        : parameters { std::move(o.parameters) }
        , solver { o.solver }
        , document { std::move(o.document) }
        , contexts { std::move(o.contexts) }
        , tool { std::move(o.tool) }
    {
        document.SetParams(&parameters);
        document.SetSolver(&solver);
    }

    Workbench& operator=(Workbench&& o) noexcept
    {
        if (this != &o) {
            parameters = std::move(o.parameters);
            solver = o.solver;
            document = std::move(o.document);
            contexts = std::move(o.contexts);
            tool = std::move(o.tool);
            document.SetParams(&parameters);
            document.SetSolver(&solver);
        }
        return *this;
    }

    Workbench(const Workbench&) = delete;
    Workbench& operator=(const Workbench&) = delete;

    // ── accessors ────────────────────────────────────────────────────────────
    Document& Doc() { return document; }
    const Document& Doc() const { return document; }
    Param::ParameterEngine& Params() { return parameters; }
    const Param::ParameterEngine& Params() const { return parameters; }
    ContextStack& Contexts() { return contexts; }
    const ContextStack& Contexts() const { return contexts; }
    Tool& ActiveTool() { return tool; }
    const Tool& ActiveTool() const { return tool; }

    const PartDocument& Evaluated() const { return document.Evaluated(); }

    // Evaluate the sketch currently being AUTHORED, without committing it.
    //
    // A provisional context puts nothing in document history until confirm, so the
    // in-progress sketch has no evaluated form of its own — yet the canvas has to show
    // solved geometry, or applying a dimension would appear to do nothing until the
    // sketch was finished. This runs the same replay+solve the real commit will run, on
    // the context's children.
    //
    // Unlike the committed path, a failing child does NOT abort the rest: mid-edit a
    // command is transiently broken all the time (a dimension whose target was just
    // deleted, say), and blanking the whole sketch under the user while they fix it is
    // worse than drawing everything that still works.
    bool BuildSketchPreview(SketchDocument& out) const
    {
        const SketchContext* s = contexts.ActiveSketch();
        if (!s) {
            return false;
        }

        out.Clear();
        out.id = s->featureId;
        out.plane = s->plane;
        out.params = &parameters;

        for (const SketchCmd& c : s->children) {
            (void)c.Get().execute(out); // best-effort: keep going past a broken command
        }
        solver.Solve(out);
        return true;
    }

    // What the toolbar renders, every frame. No switch, no visibility enum: the current
    // context is the authority on which tools exist.
    std::vector<ToolId> AvailableTools() const { return contexts.AvailableTools(); }

    // ── direct manipulation (canvas drag) ─────────────────────────────────────
    // Move a line's endpoint (or a circle/arc centre) in the ACTIVE sketch to `to`. This
    // edits the authoring command's as-drawn coordinate; the next BuildSketchPreview /
    // Evaluated re-solves from there, so a constrained line follows the cursor only as
    // far as its constraints allow. Returns false if `id` isn't an editable line in the
    // sketch being authored.
    bool MoveLinePoint(FeatureId id, PointRef which, Geometry::Point2 to)
    {
        SketchContext* s = contexts.ActiveSketch();
        if (!s) {
            return false;
        }
        SketchLine* l = FindLineMutable(s->children, id);
        if (!l) {
            return false;
        }
        if (which == PointRef::End) {
            l->b = to;
        } else {
            l->a = to;
        }
        return true;
    }

    // Translate a whole line (both endpoints) by `delta` — dragging the line body.
    bool TranslateLine(FeatureId id, Geometry::Point2 delta)
    {
        SketchContext* s = contexts.ActiveSketch();
        if (!s) {
            return false;
        }
        SketchLine* l = FindLineMutable(s->children, id);
        if (!l) {
            return false;
        }
        l->a = { l->a.x + delta.x, l->a.y + delta.y };
        l->b = { l->b.x + delta.x, l->b.y + delta.y };
        return true;
    }

    // ── tool driving ─────────────────────────────────────────────────────────
    // Begin a tool, refusing any the current context doesn't offer. That refusal is why
    // "Finish Sketch" can't fire while a mirror is active: it simply isn't available.
    bool StartTool(ToolId id)
    {
        if (!contexts.IsToolAvailable(id)) {
            return false;
        }
        tool.Begin(id);

        // A tool needing no input at all (Finish Sketch, Stop Mirror) has nothing to
        // wait for; run it immediately rather than leaving it "active" forever.
        const ToolInfo* info = FindTool(id);
        if (info && info->immediate) {
            return FinishTool();
        }
        return true;
    }

    // Apply the active tool's outcome, if it has collected everything it needs.
    // THE single commit path: identical whether the inputs came from clicks or typing.
    bool FinishTool()
    {
        if (!tool.Ready()) {
            return false;
        }

        const SketchContext* sketch = contexts.ActiveSketch();
        FeatureId owner = sketch ? sketch->featureId : kNullFeature;

        ToolOutcome out = tool.Finish(parameters, owner);
        bool ok = Apply(std::move(out));
        tool.Reset();
        return ok;
    }

    void CancelTool() { tool.Reset(); }

    // Return to a clean slate before a load swaps the document: no active gesture, no
    // nested context. The document + parameters are replaced by the caller afterward.
    void PrepareForLoad()
    {
        tool.Reset();
        contexts.Reset();
    }

    // Escape: drop the in-progress gesture first; only once there is no gesture does it
    // pop a context — and then exactly one level, never straight to root.
    bool Escape()
    {
        if (tool.Active()) {
            tool.Reset();
            return true;
        }
        return contexts.Cancel();
    }

    // ── history ──────────────────────────────────────────────────────────────
    bool Undo() { return document.Undo(); }
    bool Redo() { return document.Redo(); }
    bool CanUndo() const { return document.CanUndo(); }
    bool CanRedo() const { return document.CanRedo(); }

    // Re-enter a committed sketch to edit it.
    bool EditFeature(FeatureId id)
    {
        DTL::Optional<u32> slot = document.IndexOf(id);
        if (!slot.has_value()) {
            return false;
        }
        return contexts.PushSketchForEdit(document, *slot);
    }

    // Delete a committed feature, taking its dimension expressions with it so they don't
    // linger in the ParameterTable pointing at geometry that no longer exists.
    bool DeleteFeature(FeatureId id)
    {
        DTL::Optional<u32> slot = document.IndexOf(id);
        if (!slot.has_value()) {
            return false;
        }
        parameters.RemoveOwnedBy(id);
        return document.RemoveAt(*slot);
    }

private:
    // Find a mutable SketchLine by id in a command tree (recursing into groups).
    static SketchLine* FindLineMutable(std::vector<SketchCmd>& cmds, FeatureId id)
    {
        for (SketchCmd& c : cmds) {
            if (SketchLine* l = c.As<SketchLine>()) {
                if (l->id == id) {
                    return l;
                }
            }
            if (CompoundSketchCmd* g = c.As<CompoundSketchCmd>()) {
                if (SketchLine* nested = FindLineMutable(g->children, id)) {
                    return nested;
                }
            }
        }
        return nullptr;
    }

    bool Apply(ToolOutcome out)
    {
        bool ok = true;

        // Note whether we're committing a line BEFORE the move consumes the command.
        bool committedLine = out.command.has_value() && out.command->Is<SketchLine>();
        bool chainLink = out.chainLink;

        if (out.command.has_value()) {
            ok = contexts.Commit(document, std::move(*out.command));
        }

        // Auto-coincident on chained lines: the polyline the canvas draws by restarting
        // the Line tool becomes a truly connected chain, so dragging or dimensioning one
        // segment carries its neighbours. Skipped inside a mirror (the pass-through
        // context has its own synthesis) and whenever a non-line command breaks the run.
        if (committedLine && ok && contexts.ActiveSymmetry() == nullptr) {
            const SketchContext* s = contexts.ActiveSketch();
            FeatureId newLine = (s && !s->children.empty() && s->children.back().Is<SketchLine>())
                ? s->children.back().Get().id
                : kNullFeature;
            if (newLine != kNullFeature) {
                if (chainLink && lastLine != kNullFeature) {
                    SketchConstraintCmd c {};
                    c.kind = ConstraintKind::Coincident;
                    c.a = lastLine;
                    c.aPoint = PointRef::End;
                    c.b = newLine;
                    c.bPoint = PointRef::Start;
                    contexts.Commit(document, SketchCmd { c });
                }
                lastLine = newLine;
            }
        } else if (out.command.has_value()) {
            lastLine = kNullFeature; // a non-line command ends the chain
        }

        switch (out.action) {
        case ContextAction::Push:
            if (out.push.kind == ContextKind::Sketch) {
                ok = contexts.PushSketch(document, out.push.plane) && ok;
            } else if (out.push.kind == ContextKind::SymmetryGroup) {
                // Mirror about where the axis actually IS (post-solve), not where it was
                // drawn — those differ the moment the axis carries a dimension.
                SketchDocument preview;
                Geometry::Point2 a {};
                Geometry::Point2 b {};
                if (BuildSketchPreview(preview)) {
                    if (const SketchEntity* e = preview.Find(out.push.axis)) {
                        a = e->a;
                        b = e->b;
                    }
                }
                ok = contexts.PushSymmetry(out.push.axis, a, b) && ok;
            }
            break;
        case ContextAction::Confirm:
            ok = contexts.Confirm(document) && ok;
            break;
        case ContextAction::Cancel:
            ok = contexts.Cancel() && ok;
            break;
        case ContextAction::None:
            break;
        }

        // Entering/leaving a context (finish sketch, start/stop mirror, ...) ends any
        // polyline run: the next Line starts a fresh chain.
        if (out.action != ContextAction::None) {
            lastLine = kNullFeature;
        }

        return ok;
    }

    // Declaration order matters: `document` holds non-owning pointers to `parameters`
    // and `solver`, so both must be constructed first.
    Param::ParameterEngine parameters;
    HybridSketchSolver solver;
    Document document;
    ContextStack contexts;
    Tool tool;

    // The previous line committed in the current polyline run, for auto-coincident
    // chaining. kNullFeature when no chain is in progress. A transient UI hint, so it is
    // intentionally not preserved across a Workbench move.
    FeatureId lastLine { kNullFeature };
};
