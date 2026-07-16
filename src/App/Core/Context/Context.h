#pragma once
#include "DTL.h"
#include "Document.h"
#include "Geometry.h"
#include "Mirror.h"
#include "ParameterEngine.h"
#include "PolymorphicVariant.h"
#include "SketchCmd.h"
#include "ToolId.h"
#include <vector>

// The context stack — what sits between a Tool and the Document.
//
//   RootContext → SketchContext → SymmetryGroupContext → ...
//
// stack.back() is always "current". The toolbar reads exactly current().AvailableTools()
// every frame, so "only sketch tools are legal while sketching" needs no special-casing
// anywhere in the UI: it is a property of which context is on top.
//
// ── composite vs. decorator ──────────────────────────────────────────────────
// The question to ask of ANY new nested mode, before modelling it: does it MATERIALIZE
// something that doesn't exist until you finish, or does it just CHANGE HOW already-real
// commands get synthesized while it's active?
//
//   * SketchContext is COMPOSITE / PROVISIONAL. It builds a sub-document with its own
//     coordinate frame. Nothing is real until Confirm() hands one composite command to
//     the layer below. Cancelling discards local state, because none of it ever landed.
//
//   * SymmetryGroupContext is DECORATOR / PASS-THROUGH. It "runs in place" and owns no
//     sub-document at all. On every commit it synthesizes a mirrored twin plus a
//     symmetry constraint and forwards all three straight down. Confirm() returns
//     nothing — the effect was already committed as it happened. Cancelling doesn't
//     roll anything back; it just stops the auto-behaviour, which is how Dynamic Mirror
//     behaves in the tools this imitates.
//
// A pass-through context forwards to "one slot down the same stack" BY INDEX, never via
// a stored parent pointer — the stack is a vector and reallocates.

enum class ContextKind : u8 {
    Root,
    Sketch,
    SymmetryGroup,
};

struct ContextBase {
    virtual ~ContextBase() = default;
    virtual ContextKind Kind() const = 0;
    virtual void AvailableTools(std::vector<ToolId>& out) const = 0;
};

struct RootContext : ContextBase {
    ContextKind Kind() const override { return ContextKind::Root; }
    void AvailableTools(std::vector<ToolId>& out) const override
    {
        out.push_back(ToolId::CreateSketch);
    }
};

struct SketchContext : ContextBase {
    Geometry::SketchPlane plane { Geometry::SketchPlane::XY };
    std::vector<SketchCmd> children;

    // The FeatureId this sketch will commit under. Allocated on push and PRESERVED when
    // re-editing, so dimensions and any future downstream feature keep referring to the
    // same sketch across an edit.
    FeatureId featureId { kNullFeature };

    // Set when re-editing an existing sketch: which history slot to replace on confirm.
    // Absent means this is a brand-new sketch and confirm appends.
    DTL::Optional<u32> editingSlot {};

    ContextKind Kind() const override { return ContextKind::Sketch; }

    void AvailableTools(std::vector<ToolId>& out) const override
    {
        out.push_back(ToolId::Line);
        out.push_back(ToolId::Circle);
        out.push_back(ToolId::Arc);
        out.push_back(ToolId::Dimension);
        out.push_back(ToolId::Coincident);
        out.push_back(ToolId::Parallel);
        out.push_back(ToolId::Perpendicular);
        out.push_back(ToolId::Equal);
        out.push_back(ToolId::Tangent);
        out.push_back(ToolId::Ground);
        out.push_back(ToolId::SymmetryGroup);
        out.push_back(ToolId::FinishSketch);
    }
};

struct SymmetryGroupContext : ContextBase {
    FeatureId axis { kNullFeature };
    Geometry::Point2 axisA {};
    Geometry::Point2 axisB {};

    ContextKind Kind() const override { return ContextKind::SymmetryGroup; }

    void AvailableTools(std::vector<ToolId>& out) const override
    {
        out.push_back(ToolId::Line);
        out.push_back(ToolId::Circle);
        out.push_back(ToolId::Arc);
        out.push_back(ToolId::Dimension);
        out.push_back(ToolId::Coincident);
        out.push_back(ToolId::Parallel);
        out.push_back(ToolId::Perpendicular);
        out.push_back(ToolId::Equal);
        out.push_back(ToolId::Tangent);
        out.push_back(ToolId::Ground);
        out.push_back(ToolId::StopSymmetry);

        // DELIBERATELY NOT FinishSketch. If both "Finish Sketch" and "Stop Mirror" were
        // offered at once, a click on Finish would be ambiguous: does it end the mirror
        // or the whole sketch? Requiring the explicit exit first removes the ambiguity
        // rather than resolving it by guesswork.
    }
};

struct ContextVariant : PolymorphicVariant<ContextBase, RootContext, SketchContext, SymmetryGroupContext> {
    using PolymorphicVariant::PolymorphicVariant;
};

class ContextStack {
public:
    // Deliberately holds NO Document pointer. Scenes live in a std::vector, so a
    // reallocation moves the Document a stored pointer would still be aiming at — and
    // App.dll hot-reloads make a stale pointer even easier to acquire. The document is
    // passed per call instead: it is the caller's, never the stack's.
    ContextStack()
    {
        stack.push_back(ContextVariant { RootContext {} });
    }

    // Drop every nested context back to Root — used before a load replaces the document
    // out from under an in-progress sketch.
    void Reset()
    {
        stack.clear();
        stack.push_back(ContextVariant { RootContext {} });
    }

    // ── queries ──────────────────────────────────────────────────────────────
    ContextKind CurrentKind() const { return stack.back().Get().Kind(); }
    u32 Depth() const { return static_cast<u32>(stack.size()); }

    std::vector<ToolId> AvailableTools() const
    {
        std::vector<ToolId> out;
        stack.back().Get().AvailableTools(out);
        return out;
    }

    bool IsToolAvailable(ToolId id) const
    {
        for (ToolId t : AvailableTools()) {
            if (t == id) {
                return true;
            }
        }
        return false;
    }

    // The sketch being authored, if any — the nearest SketchContext, which is one slot
    // down when a mirror is active. This is what the canvas previews from.
    const SketchContext* ActiveSketch() const
    {
        for (u32 k = static_cast<u32>(stack.size()); k-- > 0;) {
            if (const SketchContext* s = stack[k].As<SketchContext>()) {
                return s;
            }
        }
        return nullptr;
    }

    SketchContext* ActiveSketch()
    {
        for (u32 k = static_cast<u32>(stack.size()); k-- > 0;) {
            if (SketchContext* s = stack[k].As<SketchContext>()) {
                return s;
            }
        }
        return nullptr;
    }

    const SymmetryGroupContext* ActiveSymmetry() const { return stack.back().As<SymmetryGroupContext>(); }

    // ── push ─────────────────────────────────────────────────────────────────
    // "New Sketch" pushes a context and commits nothing: there is no sketch yet to
    // commit, only a place for one to be built.
    bool PushSketch(Document& doc, Geometry::SketchPlane plane)
    {
        if (CurrentKind() != ContextKind::Root) {
            return false;
        }
        SketchContext s {};
        s.plane = plane;
        s.featureId = doc.NextId();
        stack.push_back(ContextVariant { std::move(s) });
        return true;
    }

    // Re-enter an existing sketch. Its data is DEEP-COPIED into the fresh context, which
    // is what makes cancel free: backing out means dropping the copy, with nothing to
    // undo. vector<SketchCmd> is a value type, so the copy is just an assignment.
    bool PushSketchForEdit(Document& doc, u32 historyIndex)
    {
        if (CurrentKind() != ContextKind::Root || historyIndex >= doc.History().size()) {
            return false;
        }
        const SketchFeatureCommand* f = doc.History()[historyIndex].As<SketchFeatureCommand>();
        if (!f) {
            return false;
        }

        SketchContext s {};
        s.plane = f->plane;
        s.children = f->children; // deep copy, no clone() needed
        s.featureId = f->id; // same feature, same identity
        s.editingSlot = historyIndex;
        stack.push_back(ContextVariant { std::move(s) });
        return true;
    }

    // Enter dynamic-mirror mode about an existing line in the sketch being authored.
    //
    // The axis endpoints are supplied by the caller rather than read out of the axis
    // command, because a dimensioned axis's SOLVED position is not its as-drawn one —
    // mirroring about the raw coordinates would reflect across a line that isn't where
    // the user can see it. Workbench passes the solved endpoints.
    bool PushSymmetry(FeatureId axis, Geometry::Point2 axisA, Geometry::Point2 axisB)
    {
        const SketchContext* s = ActiveSketch();
        if (!s || CurrentKind() == ContextKind::Root) {
            return false;
        }
        // The axis must still be a line in this sketch.
        if (!FindLine(s->children, axis)) {
            return false;
        }

        SymmetryGroupContext g {};
        g.axis = axis;
        g.axisA = axisA;
        g.axisB = axisB;
        stack.push_back(ContextVariant { std::move(g) });
        return true;
    }

    // ── commit ───────────────────────────────────────────────────────────────
    // Route a finished sketch command into the current context. Takes the variant by
    // value because committing STORES it — a SketchCmdBase& could not be stored without
    // reintroducing exactly the clone()/type-erasure this design removed.
    bool Commit(Document& doc, SketchCmd cmd)
    {
        if (stack.size() < 2) {
            return false; // nothing but Root: no sketch is being authored
        }
        return CommitAt(doc, static_cast<u32>(stack.size()) - 1, std::move(cmd));
    }

    // ── confirm / cancel ─────────────────────────────────────────────────────
    // Confirm the current context. A composite context hands its result to the new top
    // of stack; a pass-through one returns nothing, because it already committed.
    bool Confirm(Document& doc)
    {
        if (stack.size() < 2) {
            return false;
        }

        if (stack.back().Is<SymmetryGroupContext>()) {
            stack.pop_back(); // on_confirm(): nothing to hand down
            return true;
        }

        SketchContext* s = stack.back().As<SketchContext>();
        if (!s) {
            return false;
        }

        SketchFeatureCommand f {};
        f.id = s->featureId;
        f.plane = s->plane;
        f.children = std::move(s->children);
        DTL::Optional<u32> slot = s->editingSlot;

        stack.pop_back();

        // An edited sketch REPLACES its original slot — it is the same feature at the
        // same point in history. Appending would duplicate it and leave the original
        // driving whatever depends on it.
        if (slot.has_value()) {
            return doc.ReplaceAt(*slot, Command { std::move(f) });
        }
        doc.PushCommand(Command { std::move(f) });
        return true;
    }

    // Escape pops EXACTLY ONE level — never straight to root. Nested modes are entered
    // deliberately and are exited the same way.
    bool Cancel()
    {
        if (stack.size() < 2) {
            return false; // Root can't be popped
        }
        // Provisional contexts drop their local state; pass-through ones have nothing to
        // drop, since everything they produced was committed for real as it happened.
        stack.pop_back();
        return true;
    }

private:
    static const SketchLine* FindLine(const std::vector<SketchCmd>& cmds, FeatureId id)
    {
        for (const SketchCmd& c : cmds) {
            if (const SketchLine* l = c.As<SketchLine>()) {
                if (l->id == id) {
                    return l;
                }
            }
            if (const CompoundSketchCmd* g = c.As<CompoundSketchCmd>()) {
                if (const SketchLine* nested = FindLine(g->children, id)) {
                    return nested;
                }
            }
        }
        return nullptr;
    }

    // Commit into stack[index], synthesizing as each pass-through layer is crossed.
    bool CommitAt(Document& doc, u32 index, SketchCmd cmd)
    {
        if (SketchContext* s = stack[index].As<SketchContext>()) {
            if (cmd.Get().id == kNullFeature) {
                cmd.Get().id = doc.NextId();
            }
            s->children.push_back(std::move(cmd));
            return true;
        }

        if (SymmetryGroupContext* g = stack[index].As<SymmetryGroupContext>()) {
            if (index == 0) {
                return false; // a pass-through context can't be the bottom of the stack
            }

            if (cmd.Get().id == kNullFeature) {
                cmd.Get().id = doc.NextId();
            }

            DTL::Optional<SketchCmd> twin = Geometry::MirrorCmd(cmd, g->axisA, g->axisB, doc.NextId());
            if (!twin.has_value()) {
                // Dimensions/constraints/groups have no spatial twin — forward as-is
                // rather than duplicating something meaningless.
                return CommitAt(doc, index - 1, std::move(cmd));
            }

            SketchConstraintCmd link {};
            link.id = doc.NextId();
            link.kind = ConstraintKind::Symmetry;
            link.a = cmd.Get().id;
            link.b = twin->Get().id;
            link.axis = g->axis;

            // Wrap the whole gesture in ONE composite before forwarding. This is what
            // makes line+twin+constraint a single undo step — no gesture tagging, no
            // undo-grouping mechanism, just the composite that already exists.
            CompoundSketchCmd group {};
            group.id = doc.NextId();
            group.children.push_back(std::move(cmd));
            group.children.push_back(std::move(*twin));
            group.children.push_back(SketchCmd { link });

            return CommitAt(doc, index - 1, SketchCmd { std::move(group) });
        }

        return false; // RootContext takes no sketch commands
    }

    std::vector<ContextVariant> stack;
};
