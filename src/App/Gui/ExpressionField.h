#pragma once
#include "Expression.h"
#include "ParameterEngine.h"
#include "Ui.h"
#include <string_view>

// An editable parametric-expression field: Ui::InputLabel wired to the real parser.
//
// Ui::InputLabel already takes a TextParser hook for syntax highlighting and inline
// error badges — the Ui demo shipped a toy tokenizer against it described as "exactly
// the hook a real equation editor would plug a proper parser into". This is that proper
// parser, so highlighting can never drift from evaluation: both go through
// Param::Tokenize and Param::Expression.
//
// The field validates against the LIVE table, so `$width` colours as a reference the
// moment that parameter exists and reports "unknown parameter" while it doesn't.
namespace ExprField {

// Bound to a TextParser as its `user`. Non-owning.
struct ParserBinding {
    const Param::ParameterEngine* engine { nullptr };
};

namespace detail {

    inline Ui::UiColor ColorFor(Param::Token::Type t, const Ui::ColorScheme& col)
    {
        switch (t) {
        case Param::Token::Type::Number:
            return Ui::UiColor { 90, 160, 220, 255 };
        case Param::Token::Type::ParamRef:
            return Ui::UiColor { 180, 120, 230, 255 };
        case Param::Token::Type::Func:
            return Ui::UiColor { 90, 200, 150, 255 };
        case Param::Token::Type::Plus:
        case Param::Token::Type::Minus:
        case Param::Token::Type::Star:
        case Param::Token::Type::Slash:
        case Param::Token::Type::Caret:
        case Param::Token::Type::Comma:
            return Ui::UiColor { 220, 150, 80, 255 };
        case Param::Token::Type::LParen:
        case Param::Token::Type::RParen:
            return col.textMuted;
        case Param::Token::Type::Invalid:
            return Ui::UiColor { 220, 64, 64, 255 };
        case Param::Token::Type::End:
            break;
        }
        return col.textBase;
    }

    // Does [aStart, aStart+aLen) overlap [bStart, bStart+bLen)?
    inline bool Overlaps(u32 aStart, u32 aLen, u32 bStart, u32 bLen)
    {
        if (aLen == 0 || bLen == 0) {
            return false;
        }
        return aStart < bStart + bLen && bStart < aStart + aLen;
    }

    inline void Parse(void* user, const char* text, u32 len, Ui::TextParseResult* out)
    {
        const ParserBinding* bind = static_cast<const ParserBinding*>(user);
        const Ui::ColorScheme& col = Ui::Colors();
        out->runCount = 0;
        out->hasError = false;
        out->message = nullptr;

        std::string_view src { text, len };

        // 1. Colour every token. Tokenize never bails at the first bad byte, so a typo
        //    mid-expression still leaves everything around it highlighted.
        std::vector<Param::Token> toks = Param::Tokenize(src);
        u32 n = 0;
        for (const Param::Token& t : toks) {
            if (t.type == Param::Token::Type::End || t.len == 0 || n >= out->runCap) {
                continue;
            }
            out->runs[n++] = Ui::TextStyleRun {
                t.pos, t.len, ColorFor(t.type, col), Ui::TextDecoration::None
            };
        }
        out->runCount = n;

        if (len == 0) {
            return; // an empty field is not an error, just empty
        }

        // 2. Parse and evaluate against the live table for the diagnostic.
        Param::Expression e = Param::Expression::Parse(src);
        Param::ParserError err = e.Error();
        if (e.Ok() && bind && bind->engine) {
            Param::EvalResult r = bind->engine->EvaluateText(src);
            if (!r.Ok()) {
                err = r.error;
            }
        }

        if (err.Ok()) {
            return;
        }

        out->hasError = true;
        out->message = err.msg.data(); // static storage — safe to hand to the framework

        // 3. Mark the offending span. Runs must stay sorted and non-overlapping, so
        //    recolour the runs the error covers rather than appending a new one on top.
        bool marked = false;
        for (u32 k = 0; k < out->runCount; ++k) {
            Ui::TextStyleRun& r = out->runs[k];
            if (Overlaps(r.start, r.length, err.pos, err.len)) {
                r.color = Ui::UiColor { 220, 64, 64, 255 };
                r.decoration = Ui::TextDecoration::Error;
                marked = true;
            }
        }
        // An error with no span (or one past the end, e.g. "1 +") still shows its badge
        // via hasError; there is simply no text to underline.
        (void)marked;
    }

} // namespace detail

inline Ui::TextParser MakeParser(ParserBinding& binding)
{
    return Ui::TextParser { &binding, &detail::Parse };
}

// A single-line expression field. Returns true on the frame the text changes.
inline bool Field(char* buf, u32& len, u32 cap, std::string_view placeholder,
    Ui::UiId id, ParserBinding& binding)
{
    Ui::TextParser parser = MakeParser(binding);
    return Ui::InputLabel(buf, len, cap, placeholder, id, parser);
}

} // namespace ExprField
