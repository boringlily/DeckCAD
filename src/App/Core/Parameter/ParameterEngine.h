#pragma once
#include "DTL.h"
#include "Expression.h"
#include "Unit.h"
#include <string>
#include <string_view>
#include <vector>

// Storage, dependency tracking, and evaluation for every parametric expression in a
// scene — both the named parameters the user types into the ParameterTable and the
// anonymous ones a dimension creates.
//
// Consumers only ever hold a UPID: geometry never owns an expression, so renaming or
// re-typing a parameter can't dangle anything. UPIDs come from a monotonic counter,
// NOT the storage index, so removing a parameter never renumbers the survivors.
//
// Evaluation is pull-based and memoized against a generation counter: any mutation
// bumps the generation, which invalidates every cached value at once. That gives
// correct dependent re-evaluation ordering without maintaining an explicit
// topological sort, and makes cycle detection a property of the walk (a visiting
// stack) rather than a separate graph pass.
namespace Param {

using UPID = u32;
inline constexpr UPID kNullUpid = u32_max;

// A dimension's expression records which sketch it belongs to; a user parameter is
// scene-global and has no owner.
inline constexpr u32 kNoOwner = u32_max;

class ParametricExpression {
public:
    ParametricExpression() = default;
    ParametricExpression(UPID id, std::string name, std::string_view text, u32 owner)
        : uid { id }
        , name { std::move(name) }
        , owner { owner }
        , expr { Expression::Parse(text) }
    {
    }

    UPID Id() const { return uid; }
    const std::string& Name() const { return name; }
    const std::string& Text() const { return expr.Text(); }
    const Expression& Expr() const { return expr; }
    u32 Owner() const { return owner; }
    bool IsDimension() const { return owner != kNoOwner; }

    // The unit this value is *displayed* in. Purely presentational — the stored value
    // is always in base units.
    Unit DisplayUnit() const { return display; }
    void SetDisplayUnit(Unit u) { display = u; }

private:
    friend class ParameterEngine;

    UPID uid { kNullUpid };
    std::string name;
    u32 owner { kNoOwner };
    Expression expr;
    Unit display { Unit::None };

    // Memoized evaluation, valid only while cachedGen == engine.generation.
    mutable EvalResult cached {};
    mutable u32 cachedGen { u32_max };
    mutable bool evaluating { false };
};

class ParameterEngine {
public:
    // Create a parameter. `name` may be empty for an anonymous dimension value.
    // Returns kNullUpid if the name is already taken (names must be unique to be
    // referenceable). The expression is parsed here but not evaluated — a parse error
    // is visible via Value(id), so the row still exists and stays editable.
    UPID Create(std::string_view name, std::string_view expression, u32 owner = kNoOwner)
    {
        if (!name.empty() && FindByName(name) != kNullUpid) {
            return kNullUpid;
        }
        UPID id = next_id++;
        parameters.emplace_back(id, std::string(name), expression, owner);
        Touch();
        return id;
    }

    // Create a dimension value owned by `sketch`, auto-named D1, D2, ... so it can be
    // referenced from other expressions.
    UPID CreateDimension(std::string_view expression, u32 sketch)
    {
        std::string n;
        for (u32 k = static_cast<u32>(parameters.size()) + 1;; ++k) {
            n = "D" + std::to_string(k);
            if (FindByName(n) == kNullUpid) {
                break;
            }
        }
        return Create(n, expression, sketch);
    }

    bool SetExpression(UPID id, std::string_view expression)
    {
        ParametricExpression* p = Mutable(id);
        if (!p) {
            return false;
        }
        p->expr = Expression::Parse(expression);
        Touch();
        return true;
    }

    bool Rename(UPID id, std::string_view name)
    {
        ParametricExpression* p = Mutable(id);
        if (!p) {
            return false;
        }
        UPID existing = FindByName(name);
        if (existing != kNullUpid && existing != id) {
            return false;
        }
        p->name.assign(name);
        Touch();
        return true;
    }

    bool SetDisplayUnit(UPID id, Unit u)
    {
        ParametricExpression* p = Mutable(id);
        if (!p) {
            return false;
        }
        p->display = u;
        Touch();
        return true;
    }

    bool Remove(UPID id)
    {
        for (u32 k = 0; k < parameters.size(); ++k) {
            if (parameters[k].uid == id) {
                parameters.erase(parameters.begin() + k);
                Touch();
                return true;
            }
        }
        return false;
    }

    // Drop every expression owned by `sketch` — called when a sketch is deleted so its
    // dimension values don't outlive it.
    //
    // kNoOwner is refused: it is the owner value every user parameter carries, so
    // honouring it would wipe the entire ParameterTable instead of one sketch's
    // dimensions.
    u32 RemoveOwnedBy(u32 owner)
    {
        if (owner == kNoOwner) {
            return 0;
        }
        u32 removed = 0;
        for (u32 k = 0; k < parameters.size();) {
            if (parameters[k].owner == owner) {
                parameters.erase(parameters.begin() + k);
                ++removed;
            } else {
                ++k;
            }
        }
        if (removed) {
            Touch();
        }
        return removed;
    }

    const ParametricExpression* Get(UPID id) const
    {
        for (const ParametricExpression& p : parameters) {
            if (p.uid == id) {
                return &p;
            }
        }
        return nullptr;
    }

    // ── load ───────────────────────────────────────────────────────────────────
    // The id counter, for serialization: it must round-trip so a parameter created after
    // a load can never reuse a loaded UPID.
    UPID PeekNextId() const { return next_id; }

    // Insert one parameter with an EXPLICIT uid, bypassing the auto-assign in Create().
    // Serialization must preserve exact UPIDs — geometry references dimension values by
    // UPID, and removals leave gaps that Create()'s sequential ids could never reproduce.
    // Restoring parameters in file order plus RestoreNextId() reconstructs the table
    // exactly. Refuses a uid or name already present (a corrupt/duplicate entry is
    // dropped, not allowed to shadow a good one).
    bool RestoreParameter(UPID uid, std::string_view name, std::string_view expression,
        u32 owner, Unit display)
    {
        if (uid == kNullUpid || Get(uid) != nullptr) {
            return false;
        }
        if (!name.empty() && FindByName(name) != kNullUpid) {
            return false;
        }
        parameters.emplace_back(uid, std::string(name), expression, owner);
        parameters.back().display = display;
        Touch();
        return true;
    }

    // Restore the id counter after all parameters are inserted. Clamped so it can never
    // land at or below an existing uid, whatever the file claimed.
    void RestoreNextId(UPID nextId)
    {
        next_id = nextId;
        for (const ParametricExpression& p : parameters) {
            if (p.uid != kNullUpid && p.uid >= next_id) {
                next_id = p.uid + 1;
            }
        }
        Touch();
    }

    // Drop everything — used before restoring into a reused engine.
    void ClearAll()
    {
        parameters.clear();
        next_id = 0;
        Touch();
    }

    UPID FindByName(std::string_view name) const
    {
        if (name.empty()) {
            return kNullUpid;
        }
        for (const ParametricExpression& p : parameters) {
            if (p.name == name) {
                return p.uid;
            }
        }
        return kNullUpid;
    }

    const std::vector<ParametricExpression>& Parameters() const { return parameters; }

    // The UPIDs this expression directly depends on. Unresolvable names are skipped —
    // they surface as UnknownParameter at evaluation, not here.
    std::vector<UPID> Dependencies(UPID id) const
    {
        std::vector<UPID> out;
        const ParametricExpression* p = Get(id);
        if (!p) {
            return out;
        }
        for (const std::string& name : p->Expr().References()) {
            UPID dep = FindByName(name);
            if (dep != kNullUpid) {
                out.push_back(dep);
            }
        }
        return out;
    }

    // Every parameter that (directly or transitively) depends on `id`. This is the
    // dirty set when `id` changes.
    std::vector<UPID> Dependents(UPID id) const
    {
        std::vector<UPID> out;
        for (const ParametricExpression& p : parameters) {
            if (p.uid == id) {
                continue;
            }
            if (DependsOn(p.uid, id)) {
                out.push_back(p.uid);
            }
        }
        return out;
    }

    // The value of a stored parameter, memoized until the next mutation.
    EvalResult Value(UPID id) const
    {
        const ParametricExpression* p = Get(id);
        if (!p) {
            EvalResult r {};
            r.error = MakeError(ParserError::Type::UnknownParameter, 0, 0, "no such parameter");
            return r;
        }
        if (p->cachedGen == generation) {
            return p->cached;
        }
        // A parameter reached while it is already being evaluated closes a cycle.
        if (p->evaluating) {
            EvalResult r {};
            r.error = MakeError(ParserError::Type::CyclicReference, 0, 0, "cyclic parameter reference");
            return r;
        }

        p->evaluating = true;
        ParamResolver resolver { const_cast<ParameterEngine*>(this), &ResolveThunk };
        EvalResult r = p->Expr().Evaluate(resolver);
        p->evaluating = false;

        // Don't cache a cycle verdict: it depends on who started the walk, so caching
        // it would wrongly poison the participant that gets evaluated first next time.
        if (r.error.type != ParserError::Type::CyclicReference) {
            p->cached = r;
            p->cachedGen = generation;
        }
        return r;
    }

    // Evaluate free text against the current table without storing it — used for live
    // validation while the user is still typing into a field.
    EvalResult EvaluateText(std::string_view text) const
    {
        Expression e = Expression::Parse(text);
        ParamResolver resolver { const_cast<ParameterEngine*>(this), &ResolveThunk };
        return e.Evaluate(resolver);
    }

    u32 Generation() const { return generation; }

private:
    ParametricExpression* Mutable(UPID id)
    {
        for (ParametricExpression& p : parameters) {
            if (p.uid == id) {
                return &p;
            }
        }
        return nullptr;
    }

    bool DependsOn(UPID from, UPID target) const
    {
        std::vector<UPID> stack { from };
        std::vector<UPID> seen;
        while (!stack.empty()) {
            UPID cur = stack.back();
            stack.pop_back();
            bool already = false;
            for (UPID s : seen) {
                if (s == cur) {
                    already = true;
                    break;
                }
            }
            if (already) {
                continue; // also stops a cyclic graph from looping forever here
            }
            seen.push_back(cur);
            for (UPID dep : Dependencies(cur)) {
                if (dep == target) {
                    return true;
                }
                stack.push_back(dep);
            }
        }
        return false;
    }

    // Invalidate every memoized value at once.
    void Touch() { ++generation; }

    static bool ResolveThunk(void* user, std::string_view name, Quantity& out, ParserError& err)
    {
        const ParameterEngine* self = static_cast<const ParameterEngine*>(user);
        UPID id = self->FindByName(name);
        if (id == kNullUpid) {
            err = MakeError(ParserError::Type::UnknownParameter, 0, 0, "unknown parameter");
            return false;
        }
        EvalResult r = self->Value(id);
        if (!r.Ok()) {
            err = r.error;
            // Report the failure at the *referencing* site, not inside the referee.
            err.pos = 0;
            err.len = 0;
            return false;
        }
        out = r.value;
        return true;
    }

    std::vector<ParametricExpression> parameters;
    UPID next_id { 0 };
    u32 generation { 1 };
};

} // namespace Param
