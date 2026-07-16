#pragma once
#include "DTL.h"
#include "Unit.h"
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

// The parametric expression language.
//
//   10          10mm        2in        45deg          - literals, optional unit suffix
//   $width                                            - parameter reference
//   @Min(1, 2)  @Floor($w / 3)                        - builtin call
//   ($a + 2in) * 3 ^ 2                                - grouping, operators
//
// Mixed units are resolved through the canonical base (see Unit.h): `10mm + 2in`
// evaluates to 60.8mm. Dimensional rules are enforced at evaluation, so `10mm + 45deg`
// and `10mm * 10mm` are errors rather than silently-wrong numbers.
//
// EXCEPTION-FREE: the whole build is -fno-exceptions. Nothing here throws; every
// failure is a ParserError carrying a source span so the editor can underline it.
// (std::from_chars is used over std::stod for exactly this reason.)
//
// The AST is a flat vector of nodes referenced by index — no pointers, no unique_ptr,
// matching the command representation's ethos. Expression is therefore trivially
// copyable in the ways that matter and has no ownership subtleties.
namespace Param {

inline constexpr u32 kNullNode = u32_max;

// ── errors ───────────────────────────────────────────────────────────────────
struct ParserError {
    enum class Type : u8 {
        None,
        Empty, // no expression at all
        Invalid, // unparseable token / syntax
        UnknownParameter, // $name not in the engine
        UnknownFunction, // @Name not a builtin
        ParenthesisMismatch,
        UnitMismatch, // Length + Angle
        UnitNotAllowed, // Length * Length (no area type)
        BadArity, // @Floor(1, 2)
        DivideByZero,
        CyclicReference, // $a = $b + 1, $b = $a
    };

    Type type { Type::None };
    u32 pos { 0 }; // byte offset of the offending span
    u32 len { 0 }; // its length (0 = "at end of input")
    std::string_view msg {}; // static storage; safe to hand to Ui::TextParseResult

    constexpr bool Ok() const { return type == Type::None; }
};

inline ParserError MakeError(ParserError::Type t, u32 pos, u32 len, std::string_view msg)
{
    return ParserError { t, pos, len, msg };
}

// ── tokens ───────────────────────────────────────────────────────────────────
// Shared by the parser and the syntax highlighter so the two can never disagree
// about what a piece of text is.
struct Token {
    enum class Type : u8 {
        End,
        Number,
        ParamRef, // $name
        Func, // @Name
        Plus,
        Minus,
        Star,
        Slash,
        Caret,
        LParen,
        RParen,
        Comma,
        Invalid,
    };

    Type type { Type::End };
    u32 pos { 0 };
    u32 len { 0 };
    f64 value { 0 }; // Number: magnitude already converted to base units
    Unit unit { Unit::None }; // Number: the suffix as written (for display round-trip)
    std::string_view text {}; // ParamRef/Func: the name, sliced from the source
};

namespace detail {
    constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }
    constexpr bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    constexpr bool IsAlnum(char c) { return IsAlpha(c) || IsDigit(c); }
    constexpr bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
}

// Lex `src` into tokens. Always terminates with an End token. Malformed input yields
// an Invalid token at the offending byte rather than failing the whole scan, so the
// highlighter can still colour everything around a typo.
inline std::vector<Token> Tokenize(std::string_view src)
{
    std::vector<Token> out;
    u32 i = 0;
    const u32 n = static_cast<u32>(src.size());

    while (i < n) {
        char c = src[i];
        if (detail::IsSpace(c)) {
            ++i;
            continue;
        }

        // Number, with an optional unit suffix glued to it.
        if (detail::IsDigit(c) || (c == '.' && i + 1 < n && detail::IsDigit(src[i + 1]))) {
            u32 start = i;
            while (i < n && (detail::IsDigit(src[i]) || src[i] == '.')) {
                ++i;
            }
            f64 v = 0;
            const char* first = src.data() + start;
            const char* last = src.data() + i;
            auto [ptr, ec] = std::from_chars(first, last, v);
            if (ec != std::errc {} || ptr != last) {
                out.push_back({ Token::Type::Invalid, start, i - start, 0, Unit::None, src.substr(start, i - start) });
                continue;
            }
            // Suffix. Two shapes, optionally separated from the number by spaces so
            // `5mm`, `5 mm` and `5 inches` all read the same:
            //   a symbol — 5" 6'   (one character; not letters, so scanned separately)
            //   a word   — 5mm 5in 5 inches  (a maximal letter run, matched WHOLE so
            //              "mm" != "m"+"m" and "inches" != "in"+junk)
            //
            // Scanned on a lookahead and only committed once it turns out to BE a
            // suffix: in `1 + 2` the space after `1` belongs to the expression, not to a
            // unit, so a failed lookahead must leave `i` untouched.
            u32 scan = i;
            while (scan < n && detail::IsSpace(src[scan])) {
                ++scan;
            }
            u32 sufStart = scan;
            u32 sufEnd = scan;
            if (sufEnd < n && IsUnitSymbol(src[sufEnd])) {
                ++sufEnd;
            } else {
                while (sufEnd < n && detail::IsAlpha(src[sufEnd])) {
                    ++sufEnd;
                }
            }

            Unit u = Unit::None;
            if (sufEnd > sufStart) {
                auto found = UnitFromSuffix(src.substr(sufStart, sufEnd - sufStart));
                if (!found.has_value()) {
                    // A word here can only ever have been a unit — the grammar has no
                    // bare identifiers (parameters are $-prefixed, calls @-prefixed) —
                    // so an unknown one is a typo, not something to hand back.
                    out.push_back({ Token::Type::Invalid, sufStart, sufEnd - sufStart, 0, Unit::None, src.substr(sufStart, sufEnd - sufStart) });
                    i = sufEnd;
                    continue;
                }
                u = *found;
                i = sufEnd; // commit: the spaces and the suffix belong to this number
            }
            out.push_back({ Token::Type::Number, start, i - start, ToBase(v, u), u, src.substr(start, i - start) });
            continue;
        }

        // $name — parameter reference.
        if (c == '$') {
            u32 start = i;
            ++i;
            u32 nameStart = i;
            while (i < n && detail::IsAlnum(src[i])) {
                ++i;
            }
            if (i == nameStart) { // a bare '$'
                out.push_back({ Token::Type::Invalid, start, 1, 0, Unit::None, src.substr(start, 1) });
                continue;
            }
            out.push_back({ Token::Type::ParamRef, start, i - start, 0, Unit::None, src.substr(nameStart, i - nameStart) });
            continue;
        }

        // @Name — builtin function call.
        if (c == '@') {
            u32 start = i;
            ++i;
            u32 nameStart = i;
            while (i < n && detail::IsAlnum(src[i])) {
                ++i;
            }
            if (i == nameStart) {
                out.push_back({ Token::Type::Invalid, start, 1, 0, Unit::None, src.substr(start, 1) });
                continue;
            }
            out.push_back({ Token::Type::Func, start, i - start, 0, Unit::None, src.substr(nameStart, i - nameStart) });
            continue;
        }

        Token::Type t = Token::Type::Invalid;
        switch (c) {
        case '+':
            t = Token::Type::Plus;
            break;
        case '-':
            t = Token::Type::Minus;
            break;
        case '*':
            t = Token::Type::Star;
            break;
        case '/':
            t = Token::Type::Slash;
            break;
        case '^':
            t = Token::Type::Caret;
            break;
        case '(':
            t = Token::Type::LParen;
            break;
        case ')':
            t = Token::Type::RParen;
            break;
        case ',':
            t = Token::Type::Comma;
            break;
        default:
            break;
        }
        out.push_back({ t, i, 1, 0, Unit::None, src.substr(i, 1) });
        ++i;
    }

    out.push_back({ Token::Type::End, n, 0, 0, Unit::None, {} });
    return out;
}

// ── builtins ─────────────────────────────────────────────────────────────────
enum class Func : u8 {
    Min,
    Max,
    Floor,
    Ceil,
    Round,
    Abs,
    Sqrt,
    Sin,
    Cos,
    Tan,
};

struct FuncInfo {
    std::string_view name;
    Func id;
    u32 minArgs;
    u32 maxArgs; // u32_max = variadic
};

// The builtin table. Add a row here and @Name works everywhere — parser, evaluator,
// and the editor's highlighter all read this.
inline constexpr FuncInfo kFuncs[] = {
    { "Min", Func::Min, 1, u32_max },
    { "Max", Func::Max, 1, u32_max },
    { "Floor", Func::Floor, 1, 1 },
    { "Ceil", Func::Ceil, 1, 1 },
    { "Round", Func::Round, 1, 1 },
    { "Abs", Func::Abs, 1, 1 },
    { "Sqrt", Func::Sqrt, 1, 1 },
    { "Sin", Func::Sin, 1, 1 },
    { "Cos", Func::Cos, 1, 1 },
    { "Tan", Func::Tan, 1, 1 },
};

inline const FuncInfo* FindFunc(std::string_view name)
{
    for (const FuncInfo& f : kFuncs) {
        if (f.name == name) {
            return &f;
        }
    }
    return nullptr;
}

// ── AST ──────────────────────────────────────────────────────────────────────
struct ExprNode {
    enum class Kind : u8 { Literal,
        ParamRef,
        Negate,
        Binary,
        Call };

    Kind kind { Kind::Literal };
    u8 op { 0 }; // Binary: '+' '-' '*' '/' '^'
    Func func { Func::Min }; // Call
    Unit unit { Unit::None }; // Literal: the suffix as written
    f64 value { 0 }; // Literal: magnitude in base units
    u32 ref { 0 }; // ParamRef: index into Expression::refs
    u32 lhs { kNullNode }; // Binary/Negate/Call(first arg slot)
    u32 rhs { kNullNode }; // Binary
    u32 argCount { 0 }; // Call
    u32 pos { 0 }; // source span, for error reporting
    u32 len { 0 };
};

// How an evaluator resolves `$name`. POD function-pointer rather than an abstract
// class, matching the Ui backend's vtable style: clean under -fno-exceptions, needs
// no RTTI, and lets ParameterEngine stay unknown to this header.
struct ParamResolver {
    void* user { nullptr };
    bool (*Resolve)(void* user, std::string_view name, Quantity& out, ParserError& err) { nullptr };
};

struct EvalResult {
    Quantity value {};
    ParserError error {};
    bool Ok() const { return error.Ok(); }
};

class Expression {
public:
    Expression() = default;

    static Expression Parse(std::string_view text);

    bool Ok() const { return error.Ok(); }
    bool Empty() const { return root == kNullNode; }
    const ParserError& Error() const { return error; }
    const std::string& Text() const { return source; }

    // Every distinct `$name` mentioned, in first-seen order. This is the dependency
    // edge set the engine uses to build its graph.
    const std::vector<std::string>& References() const { return refs; }

    EvalResult Evaluate(const ParamResolver& resolver) const;

private:
    friend struct Parser;

    EvalResult EvalNode(u32 idx, const ParamResolver& resolver) const;

    std::vector<ExprNode> nodes;
    std::vector<u32> args; // flattened call-argument node indices
    std::vector<std::string> refs; // owned copies of referenced parameter names
    std::string source; // owned copy; token text_views point into the caller's buffer
    u32 root { kNullNode };
    ParserError error {};
};

// ── parser (Pratt / precedence climbing) ─────────────────────────────────────
struct Parser {
    const std::vector<Token>& toks;
    Expression& e;
    u32 i { 0 };
    bool failed { false };

    const Token& Peek() const { return toks[i]; }
    const Token& Next() { return toks[i++]; }

    void Fail(ParserError::Type t, const Token& at, std::string_view msg)
    {
        if (!failed) {
            failed = true;
            e.error = MakeError(t, at.pos, at.len, msg);
        }
    }

    u32 Add(ExprNode n)
    {
        e.nodes.push_back(n);
        return static_cast<u32>(e.nodes.size() - 1);
    }

    u32 RefIndex(std::string_view name)
    {
        for (u32 k = 0; k < e.refs.size(); ++k) {
            if (e.refs[k] == name) {
                return k;
            }
        }
        e.refs.emplace_back(name);
        return static_cast<u32>(e.refs.size() - 1);
    }

    // Binding power of a binary operator; 0 = not one.
    static u32 Precedence(Token::Type t)
    {
        switch (t) {
        case Token::Type::Plus:
        case Token::Type::Minus:
            return 1;
        case Token::Type::Star:
        case Token::Type::Slash:
            return 2;
        case Token::Type::Caret:
            return 3;
        default:
            return 0;
        }
    }

    static u8 OpChar(Token::Type t)
    {
        switch (t) {
        case Token::Type::Plus:
            return '+';
        case Token::Type::Minus:
            return '-';
        case Token::Type::Star:
            return '*';
        case Token::Type::Slash:
            return '/';
        case Token::Type::Caret:
            return '^';
        default:
            return 0;
        }
    }

    u32 ParsePrimary()
    {
        const Token& t = Peek();
        switch (t.type) {
        case Token::Type::Number: {
            Next();
            f64 value = t.value; // already in base units
            u32 len = t.len;

            // Feet-and-inches: `5' 6"` is five foot six, not a syntax error. This is how
            // imperial lengths are actually written, and a deck is dimensioned in them.
            //
            // The ONLY implicit addition in the grammar, and deliberately narrow: it
            // fires for a Foot literal followed immediately by an Inch literal, and
            // nothing else. `5mm 6mm` and `5' 6` stay errors, so a missing operator is
            // still caught rather than being silently read as a sum.
            if (t.unit == Unit::Foot
                && Peek().type == Token::Type::Number
                && Peek().unit == Unit::Inch) {
                const Token& inches = Next();
                value += inches.value; // both already in mm
                len = (inches.pos + inches.len) - t.pos;
            }

            ExprNode n {};
            n.kind = ExprNode::Kind::Literal;
            n.value = value;
            n.unit = t.unit;
            n.pos = t.pos;
            n.len = len;
            return Add(n);
        }
        case Token::Type::ParamRef: {
            Next();
            ExprNode n {};
            n.kind = ExprNode::Kind::ParamRef;
            n.ref = RefIndex(t.text);
            n.pos = t.pos;
            n.len = t.len;
            return Add(n);
        }
        case Token::Type::Minus: { // unary minus
            Next();
            u32 operand = ParseUnary();
            if (failed) {
                return kNullNode;
            }
            ExprNode n {};
            n.kind = ExprNode::Kind::Negate;
            n.lhs = operand;
            n.pos = t.pos;
            n.len = t.len;
            return Add(n);
        }
        case Token::Type::Plus: { // unary plus: accepted, no-op
            Next();
            return ParseUnary();
        }
        case Token::Type::LParen: {
            Next();
            u32 inner = ParseExpr(1);
            if (failed) {
                return kNullNode;
            }
            if (Peek().type != Token::Type::RParen) {
                Fail(ParserError::Type::ParenthesisMismatch, Peek(), "expected ')'");
                return kNullNode;
            }
            Next();
            return inner;
        }
        case Token::Type::Func:
            return ParseCall();
        case Token::Type::RParen:
            Fail(ParserError::Type::ParenthesisMismatch, t, "unmatched ')'");
            return kNullNode;
        case Token::Type::End:
            Fail(ParserError::Type::Invalid, t, "unexpected end of expression");
            return kNullNode;
        default:
            Fail(ParserError::Type::Invalid, t, "unexpected token");
            return kNullNode;
        }
    }

    u32 ParseUnary() { return ParsePrimary(); }

    u32 ParseCall()
    {
        const Token& name = Next(); // Func
        const FuncInfo* info = FindFunc(name.text);
        if (!info) {
            Fail(ParserError::Type::UnknownFunction, name, "unknown function");
            return kNullNode;
        }
        if (Peek().type != Token::Type::LParen) {
            Fail(ParserError::Type::Invalid, Peek(), "expected '(' after function name");
            return kNullNode;
        }
        Next(); // (

        std::vector<u32> collected;
        if (Peek().type != Token::Type::RParen) {
            for (;;) {
                u32 a = ParseExpr(1);
                if (failed) {
                    return kNullNode;
                }
                collected.push_back(a);
                if (Peek().type == Token::Type::Comma) {
                    Next();
                    continue;
                }
                break;
            }
        }
        if (Peek().type != Token::Type::RParen) {
            Fail(ParserError::Type::ParenthesisMismatch, Peek(), "expected ')' to close call");
            return kNullNode;
        }
        const Token& close = Next();

        u32 count = static_cast<u32>(collected.size());
        if (count < info->minArgs || (info->maxArgs != u32_max && count > info->maxArgs)) {
            Fail(ParserError::Type::BadArity, name, "wrong number of arguments");
            return kNullNode;
        }

        u32 first = static_cast<u32>(e.args.size());
        for (u32 a : collected) {
            e.args.push_back(a);
        }

        ExprNode n {};
        n.kind = ExprNode::Kind::Call;
        n.func = info->id;
        n.lhs = first;
        n.argCount = count;
        n.pos = name.pos;
        n.len = (close.pos + close.len) - name.pos;
        return Add(n);
    }

    // Precedence climbing. `^` is right-associative; everything else left.
    u32 ParseExpr(u32 minPrec)
    {
        u32 lhs = ParseUnary();
        if (failed) {
            return kNullNode;
        }
        for (;;) {
            Token::Type t = Peek().type;
            u32 prec = Precedence(t);
            if (prec == 0 || prec < minPrec) {
                break;
            }
            const Token& opTok = Next();
            u32 nextMin = (t == Token::Type::Caret) ? prec : prec + 1;
            u32 rhs = ParseExpr(nextMin);
            if (failed) {
                return kNullNode;
            }
            ExprNode n {};
            n.kind = ExprNode::Kind::Binary;
            n.op = OpChar(t);
            n.lhs = lhs;
            n.rhs = rhs;
            n.pos = opTok.pos;
            n.len = opTok.len;
            lhs = Add(n);
        }
        return lhs;
    }
};

inline Expression Expression::Parse(std::string_view text)
{
    Expression e;
    e.source.assign(text);

    // Tokenize the OWNED copy: Token::text slices its buffer, and refs are copied out
    // during parse, so nothing outlives `source`.
    std::vector<Token> toks = Tokenize(e.source);

    // Surface a bad token before the parser reports something less specific.
    for (const Token& t : toks) {
        if (t.type == Token::Type::Invalid) {
            e.error = MakeError(ParserError::Type::Invalid, t.pos, t.len, "invalid character or unit");
            return e;
        }
    }

    if (toks.size() == 1) { // just End
        e.error = MakeError(ParserError::Type::Empty, 0, 0, "empty expression");
        return e;
    }

    Parser p { toks, e };
    u32 root = p.ParseExpr(1);
    if (p.failed) {
        return e;
    }
    if (p.Peek().type != Token::Type::End) {
        e.error = MakeError(ParserError::Type::Invalid, p.Peek().pos, p.Peek().len, "trailing input");
        return e;
    }
    e.root = root;
    return e;
}

// ── evaluation ───────────────────────────────────────────────────────────────
namespace detail {

    // Dimensional rules for binary operators. This is where `10mm + 45deg` and
    // `10mm * 10mm` are rejected.
    inline bool ApplyBinary(u8 op, Quantity a, Quantity b, u32 pos, u32 len, Quantity& out, ParserError& err)
    {
        switch (op) {
        case '+':
        case '-': {
            if (a.kind != b.kind) {
                err = MakeError(ParserError::Type::UnitMismatch, pos, len, "cannot add or subtract different kinds");
                return false;
            }
            out = { op == '+' ? a.value + b.value : a.value - b.value, a.kind };
            return true;
        }
        case '*': {
            if (a.kind == QuantityKind::Number) {
                out = { a.value * b.value, b.kind };
                return true;
            }
            if (b.kind == QuantityKind::Number) {
                out = { a.value * b.value, a.kind };
                return true;
            }
            err = MakeError(ParserError::Type::UnitNotAllowed, pos, len, "cannot multiply two dimensioned values");
            return false;
        }
        case '/': {
            if (b.value == 0.0) {
                err = MakeError(ParserError::Type::DivideByZero, pos, len, "division by zero");
                return false;
            }
            if (b.kind == QuantityKind::Number) {
                out = { a.value / b.value, a.kind };
                return true;
            }
            if (a.kind == b.kind) { // like/like cancels to a ratio
                out = { a.value / b.value, QuantityKind::Number };
                return true;
            }
            err = MakeError(ParserError::Type::UnitNotAllowed, pos, len, "cannot divide these kinds");
            return false;
        }
        case '^': {
            if (a.kind != QuantityKind::Number || b.kind != QuantityKind::Number) {
                err = MakeError(ParserError::Type::UnitNotAllowed, pos, len, "exponent requires plain numbers");
                return false;
            }
            out = { std::pow(a.value, b.value), QuantityKind::Number };
            return true;
        }
        default:
            err = MakeError(ParserError::Type::Invalid, pos, len, "unknown operator");
            return false;
        }
    }

} // namespace detail

inline EvalResult Expression::EvalNode(u32 idx, const ParamResolver& resolver) const
{
    EvalResult r {};
    if (idx == kNullNode || idx >= nodes.size()) {
        r.error = MakeError(ParserError::Type::Invalid, 0, 0, "malformed expression");
        return r;
    }
    const ExprNode& n = nodes[idx];

    switch (n.kind) {
    case ExprNode::Kind::Literal:
        r.value = { n.value, KindOf(n.unit) };
        return r;

    case ExprNode::Kind::ParamRef: {
        if (!resolver.Resolve) {
            r.error = MakeError(ParserError::Type::UnknownParameter, n.pos, n.len, "no parameter table available");
            return r;
        }
        ParserError err {};
        Quantity q {};
        if (!resolver.Resolve(resolver.user, refs[n.ref], q, err)) {
            // A resolver that reported nothing specific still gets a located error.
            if (err.Ok()) {
                err = MakeError(ParserError::Type::UnknownParameter, n.pos, n.len, "unknown parameter");
            } else if (err.len == 0) {
                err.pos = n.pos;
                err.len = n.len;
            }
            r.error = err;
            return r;
        }
        r.value = q;
        return r;
    }

    case ExprNode::Kind::Negate: {
        EvalResult a = EvalNode(n.lhs, resolver);
        if (!a.Ok()) {
            return a;
        }
        r.value = { -a.value.value, a.value.kind };
        return r;
    }

    case ExprNode::Kind::Binary: {
        EvalResult a = EvalNode(n.lhs, resolver);
        if (!a.Ok()) {
            return a;
        }
        EvalResult b = EvalNode(n.rhs, resolver);
        if (!b.Ok()) {
            return b;
        }
        if (!detail::ApplyBinary(n.op, a.value, b.value, n.pos, n.len, r.value, r.error)) {
            return r;
        }
        return r;
    }

    case ExprNode::Kind::Call: {
        std::vector<Quantity> vals;
        vals.reserve(n.argCount);
        for (u32 k = 0; k < n.argCount; ++k) {
            EvalResult a = EvalNode(args[n.lhs + k], resolver);
            if (!a.Ok()) {
                return a;
            }
            vals.push_back(a.value);
        }

        switch (n.func) {
        case Func::Min:
        case Func::Max: {
            Quantity best = vals[0];
            for (u32 k = 1; k < vals.size(); ++k) {
                if (vals[k].kind != best.kind) {
                    r.error = MakeError(ParserError::Type::UnitMismatch, n.pos, n.len, "arguments must all be the same kind");
                    return r;
                }
                bool take = (n.func == Func::Min) ? (vals[k].value < best.value) : (vals[k].value > best.value);
                if (take) {
                    best = vals[k];
                }
            }
            r.value = best;
            return r;
        }
        case Func::Floor:
            r.value = { std::floor(vals[0].value), vals[0].kind };
            return r;
        case Func::Ceil:
            r.value = { std::ceil(vals[0].value), vals[0].kind };
            return r;
        case Func::Round:
            r.value = { std::round(vals[0].value), vals[0].kind };
            return r;
        case Func::Abs:
            r.value = { std::fabs(vals[0].value), vals[0].kind };
            return r;
        case Func::Sqrt: {
            if (vals[0].kind != QuantityKind::Number) {
                r.error = MakeError(ParserError::Type::UnitNotAllowed, n.pos, n.len, "@Sqrt requires a plain number");
                return r;
            }
            if (vals[0].value < 0.0) {
                r.error = MakeError(ParserError::Type::Invalid, n.pos, n.len, "@Sqrt of a negative number");
                return r;
            }
            r.value = { std::sqrt(vals[0].value), QuantityKind::Number };
            return r;
        }
        case Func::Sin:
        case Func::Cos:
        case Func::Tan: {
            // Angle arrives in radians (base); a plain number is taken as radians too.
            if (vals[0].kind == QuantityKind::Length) {
                r.error = MakeError(ParserError::Type::UnitNotAllowed, n.pos, n.len, "trig requires an angle");
                return r;
            }
            f64 x = vals[0].value;
            f64 y = (n.func == Func::Sin) ? std::sin(x) : (n.func == Func::Cos) ? std::cos(x)
                                                                                : std::tan(x);
            r.value = { y, QuantityKind::Number };
            return r;
        }
        }
        r.error = MakeError(ParserError::Type::UnknownFunction, n.pos, n.len, "unknown function");
        return r;
    }
    }

    r.error = MakeError(ParserError::Type::Invalid, n.pos, n.len, "malformed node");
    return r;
}

inline EvalResult Expression::Evaluate(const ParamResolver& resolver) const
{
    EvalResult r {};
    if (!error.Ok()) {
        r.error = error;
        return r;
    }
    if (root == kNullNode) {
        r.error = MakeError(ParserError::Type::Empty, 0, 0, "empty expression");
        return r;
    }
    return EvalNode(root, resolver);
}

} // namespace Param
