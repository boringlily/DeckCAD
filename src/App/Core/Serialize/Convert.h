#pragma once
#include "Document.h"
#include "Dto.h"
#include "ParameterEngine.h"
#include "SketchCmd.h"

#include <string>

// DTO <-> runtime conversion: the single place that knows how every command maps to its
// on-disk shape, and where FeatureId / UPID counters are preserved across a save/load.
//
// This is the "index-tagged read-back" the command design named. ToDto walks the live
// types via As<T>(); FromDto rebuilds them from the tagged variant. Geometry is never
// touched here — FromDto restores the command history and the next Evaluated() replays
// it, so a loaded file re-solves under whatever solver is current.
namespace Serialize {

// Everything about a scene that isn't the document or the parameter table.
struct SceneMeta {
    Param::Unit displayUnit { Param::Unit::Millimeter };
    std::string name;
};

// ── runtime -> DTO ─────────────────────────────────────────────────────────────

inline Vec2Dto ToDto(Geometry::Point2 p) { return { p.x, p.y }; }

inline SketchCmdDto ToDto(const SketchCmd& cmd); // fwd (recursive through groups)

inline SketchCmdDto ToSketchCmdDto(const SketchCmdBase& base, const SketchCmd& cmd)
{
    if (const auto* l = cmd.As<SketchLine>()) {
        return { LineDto { l->id, ToDto(l->a), ToDto(l->b), l->construction } };
    }
    if (const auto* a = cmd.As<SketchArc>()) {
        return { ArcDto { a->id, ToDto(a->center), a->radius, a->startAngle, a->endAngle, a->construction } };
    }
    if (const auto* c = cmd.As<SketchCircle>()) {
        return { CircleDto { c->id, ToDto(c->center), c->radius, c->construction } };
    }
    if (const auto* d = cmd.As<SketchDimensionCmd>()) {
        return { DimensionDto { d->id, d->kind, d->targetA, d->targetB, d->value } };
    }
    if (const auto* k = cmd.As<SketchConstraintCmd>()) {
        return { ConstraintDto { k->id, k->kind, k->a, k->b, k->axis, k->aPoint, k->bPoint } };
    }
    if (const auto* g = cmd.As<CompoundSketchCmd>()) {
        GroupDto out;
        out.id = g->id;
        out.children.reserve(g->children.size());
        for (const SketchCmd& child : g->children) {
            out.children.push_back(ToDto(child));
        }
        return { std::move(out) };
    }
    // Unreachable: the variant is closed. Fall back to a degenerate line so a future
    // unhandled alternative fails loud in a round-trip test rather than corrupting silently.
    return { LineDto { base.id } };
}

inline SketchCmdDto ToDto(const SketchCmd& cmd) { return ToSketchCmdDto(cmd.Get(), cmd); }

inline CommandDto ToDto(const Command& cmd)
{
    if (const auto* sf = cmd.As<SketchFeatureCommand>()) {
        SketchFeatureDto out;
        out.id = sf->id;
        out.plane = sf->plane;
        out.children.reserve(sf->children.size());
        for (const SketchCmd& child : sf->children) {
            out.children.push_back(ToDto(child));
        }
        return { std::move(out) };
    }
    return { SketchFeatureDto { cmd.Get().id } };
}

inline DocumentDto ToDto(const Document& doc, const Param::ParameterEngine& params, const SceneMeta& meta)
{
    DocumentDto out;
    out.version = kDcadVersion;
    out.featureNextId = doc.PeekNextId();
    out.paramNextId = params.PeekNextId();
    out.cursor = doc.Cursor();
    out.displayUnit = meta.displayUnit;
    out.sceneName = meta.name;

    out.history.reserve(doc.History().size());
    for (const Command& cmd : doc.History()) {
        out.history.push_back(ToDto(cmd));
    }

    out.parameters.reserve(params.Parameters().size());
    for (const Param::ParametricExpression& p : params.Parameters()) {
        out.parameters.push_back(ParameterDto { p.Id(), p.Name(), p.Text(), p.Owner(), p.DisplayUnit() });
    }

    return out;
}

// ── DTO -> runtime ─────────────────────────────────────────────────────────────

inline Geometry::Point2 FromDto(const Vec2Dto& v) { return { v.x, v.y }; }

inline SketchCmd FromDto(const SketchCmdDto& dto)
{
    return std::visit([](const auto& d) -> SketchCmd {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, LineDto>) {
            SketchLine l;
            l.id = d.id;
            l.a = FromDto(d.a);
            l.b = FromDto(d.b);
            l.construction = d.construction;
            return SketchCmd { l };
        } else if constexpr (std::is_same_v<T, ArcDto>) {
            SketchArc a;
            a.id = d.id;
            a.center = FromDto(d.center);
            a.radius = d.radius;
            a.startAngle = d.startAngle;
            a.endAngle = d.endAngle;
            a.construction = d.construction;
            return SketchCmd { a };
        } else if constexpr (std::is_same_v<T, CircleDto>) {
            SketchCircle c;
            c.id = d.id;
            c.center = FromDto(d.center);
            c.radius = d.radius;
            c.construction = d.construction;
            return SketchCmd { c };
        } else if constexpr (std::is_same_v<T, DimensionDto>) {
            SketchDimensionCmd dim;
            dim.id = d.id;
            dim.kind = d.kind;
            dim.targetA = d.targetA;
            dim.targetB = d.targetB;
            dim.value = d.value;
            return SketchCmd { dim };
        } else if constexpr (std::is_same_v<T, ConstraintDto>) {
            SketchConstraintCmd k;
            k.id = d.id;
            k.kind = d.kind;
            k.a = d.a;
            k.b = d.b;
            k.axis = d.axis;
            k.aPoint = d.aPoint;
            k.bPoint = d.bPoint;
            return SketchCmd { k };
        } else { // GroupDto
            CompoundSketchCmd g;
            g.id = d.id;
            g.children.reserve(d.children.size());
            for (const SketchCmdDto& child : d.children) {
                g.children.push_back(FromDto(child));
            }
            return SketchCmd { std::move(g) };
        }
    },
        dto.v);
}

inline Command FromDto(const CommandDto& dto)
{
    return std::visit([](const auto& d) -> Command {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, SketchFeatureDto>) {
            SketchFeatureCommand sf;
            sf.id = d.id;
            sf.plane = d.plane;
            sf.children.reserve(d.children.size());
            for (const SketchCmdDto& child : d.children) {
                sf.children.push_back(FromDto(child));
            }
            return Command { std::move(sf) };
        }
    },
        dto.v);
}

// Rebuild `doc`, `params`, and `meta` from a document DTO. Returns false only on a
// version the running build doesn't understand — malformed JSON was already rejected by
// ReadDcad before this is reached, so here the DTO is structurally valid.
inline bool FromDto(const DocumentDto& dto, Document& doc, Param::ParameterEngine& params, SceneMeta& meta)
{
    if (dto.version == 0 || dto.version > kDcadVersion) {
        return false;
    }

    // Parameters first: history references them by UPID, and Restore keeps the exact ids.
    params.ClearAll();
    for (const ParameterDto& p : dto.parameters) {
        params.RestoreParameter(p.uid, p.name, p.expression, p.owner, p.display);
    }
    params.RestoreNextId(dto.paramNextId);

    std::vector<Command> history;
    history.reserve(dto.history.size());
    for (const CommandDto& c : dto.history) {
        history.push_back(FromDto(c));
    }
    doc.Restore(std::move(history), dto.cursor, dto.featureNextId);

    meta.displayUnit = dto.displayUnit;
    meta.name = dto.sceneName;
    return true;
}

} // namespace Serialize
