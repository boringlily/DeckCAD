#pragma once
#include "DTL.h"
#include "Geometry.h"
#include "ISketchSolver.h"
#include "ParameterEngine.h"
#include "PolymorphicVariant.h"
#include "SketchCmd.h"
#include "SketchDocument.h"
#include <string_view>
#include <vector>

// The persistent layer: the committed command history and the geometry recomputed from
// it. This is the only thing that survives a gesture, and the only thing worth
// serializing.
//
// UNDO IS RECOMPUTE-BASED. `cursor` is how many commands are currently applied; undo
// moves it back and replays, redo moves it forward. There are no inverse operations to
// implement or get wrong, and because commands regenerate rather than patch, replaying
// a prefix is exactly equivalent to never having run the rest.
//
// One history slot is one undo step, uniformly — whether the slot holds a single line
// or an entire sketch. A multi-entity gesture collapses to one step because the tool
// wraps it in a CompoundSketchCmd BEFORE committing, not because undo does any grouping.

// The evaluated model. Like SketchDocument this is a cache, rebuilt by replay.
struct PartDocument {
    std::vector<SketchDocument> sketches;

    // Non-owning services replay needs. Set by Document before each recompute.
    const Param::ParameterEngine* params { nullptr };
    const ISketchSolver* solver { nullptr };

    void Clear() { sketches.clear(); }

    const SketchDocument* FindSketch(FeatureId id) const
    {
        for (const SketchDocument& s : sketches) {
            if (s.id == id) {
                return &s;
            }
        }
        return nullptr;
    }
};

struct PartCmdBase {
    FeatureId id { kNullFeature };

    virtual ~PartCmdBase() = default;
    virtual ExecResult execute(PartDocument& part) const = 0;
    virtual std::string_view TypeName() const = 0;
    virtual bool Valid() const { return true; }
};

// One finished sketch: the composite a SketchContext produced on confirm. Its children
// never touched document history individually — they arrive here as one unit.
struct SketchFeatureCommand : PartCmdBase {
    Geometry::SketchPlane plane { Geometry::SketchPlane::XY };
    std::vector<SketchCmd> children;

    ExecResult execute(PartDocument& part) const override
    {
        SketchDocument doc;
        doc.id = id;
        doc.plane = plane;
        doc.params = part.params;

        for (const SketchCmd& c : children) {
            ExecResult r = c.Get().execute(doc);
            if (!r.Ok()) {
                return r;
            }
        }

        // Entities are laid down as drawn; the solver is what makes the dimensioned
        // ones obey. Without one they simply stay where they were clicked.
        if (part.solver) {
            part.solver->Solve(doc);
        }

        part.sketches.push_back(std::move(doc));
        return ExecOk();
    }

    std::string_view TypeName() const override { return "Sketch"; }
    bool Valid() const override { return true; } // an empty sketch is legal
};

// Non-recursive: a part command never contains another, so no forward-declaration
// trick is needed here (unlike SketchCmd).
struct Command : PolymorphicVariant<PartCmdBase, SketchFeatureCommand> {
    using PolymorphicVariant::PolymorphicVariant;
};

class Document {
public:
    explicit Document(const Param::ParameterEngine* params = nullptr,
        const ISketchSolver* solver = nullptr)
        : params { params }
        , solver { solver }
    {
    }

    // ── ids ──────────────────────────────────────────────────────────────────
    // Monotonic and never reused, so a FeatureId reference stays meaningful even after
    // the command that produced it is undone and something else is committed.
    FeatureId NextId() { return next_id++; }

    // The value of the id counter, for serialization: it must round-trip so a command
    // committed after a load can never collide with a loaded FeatureId.
    FeatureId PeekNextId() const { return next_id; }

    // Monotonic revision, bumped by every history mutation (via MarkDirtyFrom). Auto-save
    // compares it to detect "the history changed" without diffing the whole document.
    u32 Revision() const { return revision; }

    // ── load ───────────────────────────────────────────────────────────────────
    // Replace the entire document with deserialized state. Sets history, the undo
    // cursor, and the id counter directly — no per-command re-evaluation here; the next
    // Evaluated() rebuilds geometry by replaying, exactly as a fresh edit would.
    void Restore(std::vector<Command> loadedHistory, u32 loadedCursor, FeatureId loadedNextId)
    {
        history = std::move(loadedHistory);
        cursor = loadedCursor > history.size() ? static_cast<u32>(history.size()) : loadedCursor;
        next_id = loadedNextId;
        MarkDirtyFrom(0);
    }

    // ── history ──────────────────────────────────────────────────────────────
    const std::vector<Command>& History() const { return history; }
    u32 Cursor() const { return cursor; }
    u32 Size() const { return static_cast<u32>(history.size()); }

    // Commit one command = one undo step. Committing after an undo discards the
    // redo branch, which is the conventional (and only sane) behaviour: the redo tail
    // describes a future that no longer follows from the present.
    void PushCommand(Command cmd)
    {
        if (cursor < history.size()) {
            history.erase(history.begin() + cursor, history.end());
        }
        history.push_back(std::move(cmd));
        cursor = static_cast<u32>(history.size());
        MarkDirtyFrom(cursor - 1);
    }

    bool CanUndo() const { return cursor > 0; }
    bool CanRedo() const { return cursor < history.size(); }

    bool Undo()
    {
        if (!CanUndo()) {
            return false;
        }
        --cursor;
        MarkDirtyFrom(cursor);
        return true;
    }

    bool Redo()
    {
        if (!CanRedo()) {
            return false;
        }
        ++cursor;
        MarkDirtyFrom(cursor - 1);
        return true;
    }

    // Re-editing an existing feature REPLACES its slot rather than appending: the
    // edited sketch is the same feature, at the same point in history, so everything
    // downstream of it is invalidated.
    bool ReplaceAt(u32 index, Command cmd)
    {
        if (index >= history.size()) {
            return false;
        }
        history[index] = std::move(cmd);
        MarkDirtyFrom(index);
        return true;
    }

    bool RemoveAt(u32 index)
    {
        if (index >= history.size()) {
            return false;
        }
        history.erase(history.begin() + index);
        if (cursor > index) {
            --cursor;
        }
        MarkDirtyFrom(index);
        return true;
    }

    DTL::Optional<u32> IndexOf(FeatureId id) const
    {
        for (u32 k = 0; k < history.size(); ++k) {
            if (history[k].Get().id == id) {
                return k;
            }
        }
        return std::nullopt;
    }

    // ── recompute ────────────────────────────────────────────────────────────
    // Replays history[0, cursor) into a fresh PartDocument.
    //
    // Today this always rebuilds from zero. That is deliberate: the only part-level
    // command is a sketch, and nothing yet depends on anything else, so `firstDirty`
    // has no work to save. It is tracked (and asserted on in tests) so that when a
    // command with real upstream dependencies arrives — Extrude on a sketch's face —
    // caching the intermediate PartDocument per step turns this into the forward walk
    // from the first dirty step without changing any caller.
    const PartDocument& Evaluated() const
    {
        u32 gen = params ? params->Generation() : 0;
        if (!dirty && gen == cachedParamGen) {
            return cache;
        }

        cache.Clear();
        cache.params = params;
        cache.solver = solver;
        lastError = ExecOk();

        for (u32 k = 0; k < cursor; ++k) {
            ExecResult r = history[k].Get().execute(cache);
            if (!r.Ok() && lastError.Ok()) {
                lastError = r; // keep the first failure; later features still replay
            }
        }

        dirty = false;
        firstDirty = u32_max;
        cachedParamGen = gen;
        return cache;
    }

    // The first failure from the last recompute, if any.
    const ExecResult& LastError() const { return lastError; }

    void MarkDirtyFrom(u32 index)
    {
        dirty = true;
        ++revision; // every history mutation routes through here; auto-save watches this
        if (index < firstDirty) {
            firstDirty = index;
        }
    }

    u32 FirstDirty() const { return firstDirty; }
    bool IsDirty() const { return dirty; }

    void SetSolver(const ISketchSolver* s)
    {
        solver = s;
        MarkDirtyFrom(0);
    }
    void SetParams(const Param::ParameterEngine* p)
    {
        params = p;
        MarkDirtyFrom(0);
    }

private:
    std::vector<Command> history;
    u32 cursor { 0 }; // how many commands are currently applied
    FeatureId next_id { 1 }; // 0 stays free; kNullFeature is u32_max
    u32 revision { 0 }; // bumps on every mutation; drives auto-save change detection

    const Param::ParameterEngine* params { nullptr };
    const ISketchSolver* solver { nullptr };

    mutable PartDocument cache;
    mutable bool dirty { true };
    mutable u32 firstDirty { 0 };
    mutable u32 cachedParamGen { u32_max };
    mutable ExecResult lastError {};
};
