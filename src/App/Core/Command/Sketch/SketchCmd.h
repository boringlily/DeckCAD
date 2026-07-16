#pragma once
#include "DTL.h"
#include "Geometry.h"
#include "ParameterEngine.h"
#include "PolymorphicVariant.h"
#include "SketchDocument.h"
#include <string_view>
#include <vector>

// The sketch command tree.
//
// Commands are passive value records. They never mutate kernel state in place: execute()
// REGENERATES its contribution into a fresh SketchDocument. That is what makes undo and
// re-editing a past step tractable at all — there is no inverse operation to get wrong,
// only a replay to redo.
//
// CompoundSketchCmd is what makes nesting work, and it is deliberately a TREE rather
// than a flat list with scope markers. A tree can't be mismatched, so there is no
// open-scope stack to maintain and no scope-end/scope-start assert to violate; and one
// composite already IS one undo step, so no separate gesture-tagging is needed to group
// a multi-entity action.
//
// ── the recursion, with zero pointers ────────────────────────────────────────
// CompoundSketchCmd holds std::vector<SketchCmd>, but SketchCmd is the variant that
// contains CompoundSketchCmd. The cycle is broken by declaring SketchCmd first and
// leaving it incomplete: std::vector accepts an incomplete element type provided it is
// complete before any member is instantiated. sizeof(vector<T>) doesn't depend on T, so
// the variant can still lay CompoundSketchCmd out.
//
// THE CATCH, and it is not optional: a VIRTUAL destructor is odr-used by the vtable at
// class completion. If CompoundSketchCmd's destructor were implicit, completing the
// class would instantiate ~vector<SketchCmd>() right there — while SketchCmd is still
// incomplete — and the whole trick collapses (MSVC's STL reports it as "incomplete type
// used in type trait expression" from deep inside <xmemory>).
//
// So EVERY special member is declared here and defined at the bottom of the file, once
// SketchCmd is complete. Verified: vector<Incomplete> alone is fine; vector<Incomplete>
// + a virtual dtor + implicit members does NOT compile; out-of-line members fix it.
// Do not "tidy" these into `= default` in the class body — that silently reintroduces
// the early instantiation.

struct SketchCmdBase {
    // Assigned once at commit; survives every recompute. Entities this command produces
    // are keyed by it, so references to them never dangle across an edit.
    FeatureId id { kNullFeature };

    virtual ~SketchCmdBase() = default;

    virtual ExecResult execute(SketchDocument& doc) const = 0;

    // Shown in the Explorer tree.
    virtual std::string_view TypeName() const = 0;

    // Whether the command's own parameters are complete. Tools gate on this before
    // committing, so an invalid command should never reach history.
    virtual bool Valid() const { return true; }
};

struct SketchLine : SketchCmdBase {
    Geometry::Point2 a {};
    Geometry::Point2 b {};
    bool construction { false };

    ExecResult execute(SketchDocument& doc) const override
    {
        SketchEntity e {};
        e.id = id;
        e.kind = EntityKind::Line;
        e.a = a;
        e.b = b;
        e.construction = construction;
        doc.entities.push_back(e);
        return ExecOk();
    }

    std::string_view TypeName() const override { return "Line"; }

    bool Valid() const override
    {
        // A zero-length line is a mis-click, not a feature.
        return !(a.x == b.x && a.y == b.y);
    }
};

struct SketchArc : SketchCmdBase {
    Geometry::Point2 center {};
    f64 radius { 0 };
    f64 startAngle { 0 };
    f64 endAngle { 0 };
    bool construction { false };

    ExecResult execute(SketchDocument& doc) const override
    {
        SketchEntity e {};
        e.id = id;
        e.kind = EntityKind::Arc;
        e.a = center;
        e.radius = radius;
        e.startAngle = startAngle;
        e.endAngle = endAngle;
        e.construction = construction;
        doc.entities.push_back(e);
        return ExecOk();
    }

    std::string_view TypeName() const override { return "Arc"; }
    bool Valid() const override { return radius > 0.0; }
};

struct SketchCircle : SketchCmdBase {
    Geometry::Point2 center {};
    f64 radius { 0 };
    bool construction { false };

    ExecResult execute(SketchDocument& doc) const override
    {
        SketchEntity e {};
        e.id = id;
        e.kind = EntityKind::Circle;
        e.a = center;
        e.radius = radius;
        e.construction = construction;
        doc.entities.push_back(e);
        return ExecOk();
    }

    std::string_view TypeName() const override { return "Circle"; }
    bool Valid() const override { return radius > 0.0; }
};

// Applying one of these is what gives an entity a size that is DICTATED rather than
// merely measured. The value is a UPID, so it can be an expression over parameters.
struct SketchDimensionCmd : SketchCmdBase {
    DimensionKind kind { DimensionKind::Length };
    FeatureId targetA { kNullFeature };
    FeatureId targetB { kNullFeature };
    Param::UPID value { Param::kNullUpid };

    ExecResult execute(SketchDocument& doc) const override
    {
        if (targetA == kNullFeature) {
            return ExecFail(ExecStatus::InvalidCommand, "dimension has no target");
        }
        if (!doc.Find(targetA)) {
            return ExecFail(ExecStatus::MissingReference, "dimension target no longer exists");
        }
        if (kind == DimensionKind::Distance && !doc.Find(targetB)) {
            return ExecFail(ExecStatus::MissingReference, "distance dimension needs two targets");
        }
        if (doc.params && doc.params->Get(value) == nullptr) {
            return ExecFail(ExecStatus::BadParameter, "dimension value is not in the parameter table");
        }

        SketchDimensionRecord d {};
        d.id = id;
        d.kind = kind;
        d.targetA = targetA;
        d.targetB = targetB;
        d.value = value;
        doc.dimensions.push_back(d);
        return ExecOk();
    }

    std::string_view TypeName() const override { return "Dimension"; }
    bool Valid() const override { return targetA != kNullFeature && value != Param::kNullUpid; }
};

struct SketchConstraintCmd : SketchCmdBase {
    ConstraintKind kind { ConstraintKind::Coincident };
    FeatureId a { kNullFeature };
    FeatureId b { kNullFeature };
    FeatureId axis { kNullFeature };
    PointRef aPoint { PointRef::Start }; // Coincident only
    PointRef bPoint { PointRef::Start };

    ExecResult execute(SketchDocument& doc) const override
    {
        if (a == kNullFeature) {
            return ExecFail(ExecStatus::InvalidCommand, "constraint has no subject");
        }
        SketchConstraintRecord c {};
        c.id = id;
        c.kind = kind;
        c.a = a;
        c.b = b;
        c.axis = axis;
        c.aPoint = aPoint;
        c.bPoint = bPoint;
        doc.constraints.push_back(c);
        return ExecOk();
    }

    std::string_view TypeName() const override { return "Constraint"; }
    bool Valid() const override { return a != kNullFeature; }
};

// Forward declaration only — this is what breaks the containment cycle. No pointer,
// no unique_ptr, no type erasure.
struct SketchCmd;

// One history slot holding many commands: the unit of nesting AND the unit of undo.
// A whole sketch is one of these; so is a single dynamic-mirror gesture (line + twin +
// symmetry constraint), which is why that gesture collapses to one Ctrl+Z for free.
struct CompoundSketchCmd : SketchCmdBase {
    std::vector<SketchCmd> children; // legal: incomplete type, complete before use

    // All of these are DECLARED here and DEFINED at the bottom of the file. See the
    // header comment: an implicit (or in-class defaulted) destructor would instantiate
    // ~vector<SketchCmd>() while SketchCmd is still incomplete and fail to compile.
    CompoundSketchCmd();
    CompoundSketchCmd(const CompoundSketchCmd&);
    CompoundSketchCmd(CompoundSketchCmd&&) noexcept;
    CompoundSketchCmd& operator=(const CompoundSketchCmd&);
    CompoundSketchCmd& operator=(CompoundSketchCmd&&) noexcept;
    ~CompoundSketchCmd() override;

    ExecResult execute(SketchDocument& doc) const override;
    std::string_view TypeName() const override { return "Group"; }
    bool Valid() const override;
};

struct SketchCmd : PolymorphicVariant<SketchCmdBase, SketchLine, SketchArc, SketchCircle,
                       SketchDimensionCmd, SketchConstraintCmd, CompoundSketchCmd> {
    using PolymorphicVariant::PolymorphicVariant;
};

// ── out-of-line composite members (SketchCmd is complete from here down) ─────
//
// Only now is it safe to instantiate vector<SketchCmd>'s members, which is exactly what
// each of these does.

inline CompoundSketchCmd::CompoundSketchCmd() = default;
inline CompoundSketchCmd::CompoundSketchCmd(const CompoundSketchCmd&) = default;
inline CompoundSketchCmd::CompoundSketchCmd(CompoundSketchCmd&&) noexcept = default;
inline CompoundSketchCmd& CompoundSketchCmd::operator=(const CompoundSketchCmd&) = default;
inline CompoundSketchCmd& CompoundSketchCmd::operator=(CompoundSketchCmd&&) noexcept = default;
inline CompoundSketchCmd::~CompoundSketchCmd() = default;

inline ExecResult CompoundSketchCmd::execute(SketchDocument& doc) const
{
    // A child's failure fails the whole group: a half-executed composite would leave the
    // document in a state no command history describes.
    for (const SketchCmd& c : children) {
        ExecResult r = c.Get().execute(doc);
        if (!r.Ok()) {
            return r;
        }
    }
    return ExecOk();
}

inline bool CompoundSketchCmd::Valid() const
{
    if (children.empty()) {
        return false;
    }
    for (const SketchCmd& c : children) {
        if (!c.Get().Valid()) {
            return false;
        }
    }
    return true;
}

// Depth-first walk over a command tree, composites included. `fn` takes
// (const SketchCmdBase&, u32 depth). This is what the Explorer tree renders from.
template <typename Fn>
inline void WalkSketchCmds(const std::vector<SketchCmd>& cmds, Fn&& fn, u32 depth = 0)
{
    for (const SketchCmd& c : cmds) {
        fn(c.Get(), depth);
        if (const CompoundSketchCmd* g = c.As<CompoundSketchCmd>()) {
            WalkSketchCmds(g->children, fn, depth + 1);
        }
    }
}

// Total command count including nested children — the number of rows the tree shows.
inline u32 CountSketchCmds(const std::vector<SketchCmd>& cmds)
{
    u32 n = 0;
    WalkSketchCmds(cmds, [&](const SketchCmdBase&, u32) { ++n; });
    return n;
}
