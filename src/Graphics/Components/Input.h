#pragma once
#include "UiContext.h"
#include "UiTypes.h"
#include <cstring>
#include <string_view>

// The two editable text components.
//
//   InputLabel - single line. Grows or scrolls sideways (the caret stays in view).
//                Optional TextParser for syntax highlight + inline errors.
//   InputBox   - multi-line editor. Word-wraps, vertical-scrolls, multi-line caret.
//
// Both edit a caller-owned char buffer (`len` updated in place, one byte reserved
// for the null terminator) and share the focused-field caret + selection anchor.
// Full editing: click to position, drag / shift+arrows to select, Home/End, mouse,
// Ctrl+A/C/X/V (clipboard). Return true on the frame the text changes.
namespace Ui {

namespace detail {
    inline u32 LineStartOf(const char* buf, u32 pos)
    {
        while (pos > 0 && buf[pos - 1] != '\n') {
            --pos;
        }
        return pos;
    }
    inline u32 LineEndOf(const char* buf, u32 len, u32 pos)
    {
        while (pos < len && buf[pos] != '\n') {
            ++pos;
        }
        return pos;
    }
    inline void InsertAt(char* buf, u32& len, u32& caret, char ch)
    {
        if (caret > len) {
            caret = len; // defensive: never memmove a negative (underflowed) length.
        }
        std::memmove(buf + caret + 1, buf + caret, len - caret);
        buf[caret] = ch;
        ++len;
        ++caret;
    }
    inline u32 Lo(u32 a, u32 b) { return a < b ? a : b; }
    inline u32 Hi(u32 a, u32 b) { return a > b ? a : b; }

    // Delete the selection [min,max); collapse caret + anchor to its start.
    inline bool DeleteSel(char* buf, u32& len, u32& caret, u32& anchor)
    {
        if (caret == anchor) {
            return false;
        }
        u32 lo = Lo(caret, anchor);
        u32 hi = Hi(caret, anchor);
        if (hi > len) {
            hi = len; // defensive: keep the copy length non-negative.
        }
        if (lo > hi) {
            lo = hi;
        }
        std::memmove(buf + lo, buf + hi, len - hi);
        len -= (hi - lo);
        caret = anchor = lo;
        return true;
    }

    // Process mouse + keyboard for a focused text field. Edits buf/len/caret/anchor
    // in place. `multiline` enables Enter, Up/Down, and per-line Home/End + wrapped
    // hit-testing. Returns true if the text changed.
    inline bool HandleTextInput(UiId fid, char* buf, u32& len, u32 cap, u32& caret, u32& anchor, bool multiline)
    {
        bool changed = false;
        Context* ctx = Current();
        if (!ctx) {
            return false;
        }

        // Mouse: click positions the caret; drag extends the selection. The text box
        // is the hovered element's rect inset by the input padding {8,8,6,6}.
        const PointerState& p = ctx->input.pointer;
        Rect box = ctx->input.hotRect;
        Rect textRect { box.x + 8.0f, box.y + 6.0f, box.w - 16.0f, box.h - 12.0f };
        if (IsClicked(fid)) {
            SetFocus(fid);
            u32 idx = CaretIndexAt(buf, len, textRect, 0, 16, static_cast<s32>(caret), p.pos, multiline);
            caret = anchor = idx;
        } else if (IsFocused(fid) && p.down && ctx->input.hotId == fid) {
            caret = CaretIndexAt(buf, len, textRect, 0, 16, static_cast<s32>(caret), p.pos, multiline);
        }
        // Keep caret/anchor in bounds (e.g. the u32_max focus-gain sentinel).
        if (caret > len) {
            caret = len;
        }
        if (anchor > len) {
            anchor = len;
        }

        if (!IsFocused(fid)) {
            return false; // keyboard only when focused.
        }

        const KeyboardState& kb = Keyboard();
        bool shift = kb.shift;

        if (kb.selectAll) {
            anchor = 0;
            caret = len;
        }

        // Clipboard.
        if ((kb.copy || kb.cut) && caret != anchor) {
            u32 lo = Lo(caret, anchor);
            u32 hi = Hi(caret, anchor);
            char tmp[1024];
            u32 n = (hi - lo) < sizeof(tmp) - 1 ? (hi - lo) : static_cast<u32>(sizeof(tmp) - 1);
            std::memcpy(tmp, buf + lo, n);
            tmp[n] = '\0';
            SetClipboard(tmp);
            if (kb.cut) {
                DeleteSel(buf, len, caret, anchor);
                changed = true;
            }
        }
        if (kb.paste) {
            const char* clip = GetClipboard();
            if (clip) {
                if (caret != anchor) {
                    DeleteSel(buf, len, caret, anchor);
                }
                for (const char* s = clip; *s; ++s) {
                    char ch = *s;
                    bool ok = (static_cast<unsigned char>(ch) >= 32 && static_cast<unsigned char>(ch) < 127)
                        || (multiline && ch == '\n');
                    if (ok && len + 1 < cap) {
                        InsertAt(buf, len, caret, ch);
                        changed = true;
                    }
                }
                anchor = caret;
            }
        }

        // Typed characters / newline (selection is replaced).
        bool inserted = false;
        for (u32 i = 0; i < kb.typedCount; ++i) {
            u32 cp = kb.typed[i];
            if (cp >= 32 && cp < 127 && len + 1 < cap) {
                if (caret != anchor) {
                    DeleteSel(buf, len, caret, anchor);
                }
                InsertAt(buf, len, caret, static_cast<char>(cp));
                changed = true;
                inserted = true;
            }
        }
        if (multiline && kb.enter && len + 1 < cap) {
            if (caret != anchor) {
                DeleteSel(buf, len, caret, anchor);
            }
            InsertAt(buf, len, caret, '\n');
            changed = true;
            inserted = true;
        }
        if (inserted) {
            anchor = caret;
        }

        if (kb.backspace) {
            if (caret != anchor) {
                DeleteSel(buf, len, caret, anchor);
                changed = true;
            } else if (caret > 0) {
                std::memmove(buf + caret - 1, buf + caret, len - caret);
                --len;
                --caret;
                changed = true;
            }
            anchor = caret;
        }
        if (kb.del) {
            if (caret != anchor) {
                DeleteSel(buf, len, caret, anchor);
                changed = true;
            } else if (caret < len) {
                std::memmove(buf + caret, buf + caret + 1, len - caret - 1);
                --len;
                changed = true;
            }
            anchor = caret;
        }

        // Caret movement. Without Shift a movement collapses any selection.
        if (kb.left) {
            if (caret != anchor && !shift) {
                caret = Lo(caret, anchor);
            } else if (caret > 0) {
                --caret;
            }
            if (!shift) {
                anchor = caret;
            }
        }
        if (kb.right) {
            if (caret != anchor && !shift) {
                caret = Hi(caret, anchor);
            } else if (caret < len) {
                ++caret;
            }
            if (!shift) {
                anchor = caret;
            }
        }
        if (multiline && kb.up) {
            u32 ls = LineStartOf(buf, caret);
            if (ls > 0) {
                u32 col = caret - ls;
                u32 ps = LineStartOf(buf, ls - 1);
                u32 pl = (ls - 1) - ps;
                caret = ps + (col < pl ? col : pl);
            } else {
                caret = 0;
            }
            if (!shift) {
                anchor = caret;
            }
        }
        if (multiline && kb.down) {
            u32 le = LineEndOf(buf, len, caret);
            if (le < len) {
                u32 col = caret - LineStartOf(buf, caret);
                u32 ns = le + 1;
                u32 nl = LineEndOf(buf, len, ns) - ns;
                caret = ns + (col < nl ? col : nl);
            } else {
                caret = len;
            }
            if (!shift) {
                anchor = caret;
            }
        }
        if (kb.home) {
            caret = multiline ? LineStartOf(buf, caret) : 0;
            if (!shift) {
                anchor = caret;
            }
        }
        if (kb.end) {
            caret = multiline ? LineEndOf(buf, len, caret) : len;
            if (!shift) {
                anchor = caret;
            }
        }

        if (len < cap) {
            buf[len] = '\0';
        }
        return changed;
    }
}

// ---- InputLabel: single line --------------------------------------------------

inline bool InputLabel(char* buf, u32& len, u32 cap, std::string_view placeholder,
    UiId id = kNullId, const TextParser& parser = {})
{
    const ColorScheme& col = Colors();
    UiId fid = id != kNullId ? id : HashId(buf, 11);
    bool focused = IsFocused(fid);

    u32 caret = CaretPos();
    if (caret > len) {
        caret = len;
    }
    u32 anchor = SelectAnchorPos();
    if (anchor > len) {
        anchor = len;
    }

    // Optional parser -> syntax-highlight runs + diagnostic (frame-scoped scratch).
    TextStyleRun* runs = nullptr;
    u32 runCount = 0;
    const char* message = nullptr;
    bool hasError = false;
    if (parser.Parse && len > 0) {
        constexpr u32 kRunCap = 128;
        runs = AllocFrameArray<TextStyleRun>(kRunCap);
        if (runs) {
            TextParseResult res {};
            res.runs = runs;
            res.runCap = kRunCap;
            parser.Parse(parser.user, buf, len, &res);
            runCount = res.runCount;
            message = res.message;
            hasError = res.hasError;
        }
    }

    LayoutConfig column {};
    column.direction = Direction::TopToBottom;
    column.gap = 3;
    column.align = AlignCross::Stretch;
    column.sizing = { Grow(), Fit() };
    OpenElement(column, HashChild(fid, 0));

    LayoutConfig box {};
    box.sizing = { Grow(), Fixed(32) };
    box.padding = { 8, 8, 6, 6 };
    box.direction = Direction::LeftToRight;
    box.align = AlignCross::Center;
    box.cornerRadius = 4;
    box.background = col.bgLight;
    box.border = { 1, 1, 1, 1 };
    box.borderColor = hasError ? col.alertDanger : (focused ? col.accentPrimary : col.borderBase);
    box.clip = true;
    box.gap = 6; // space between the text and the error indicator (parser fields).
    OpenElement(box, fid);

    LayoutConfig tc {};
    tc.hitTestable = false;
    tc.sizing = { Grow(), Fit() };
    u32 t = OpenElement(tc, HashChild(fid, 1));
    if (len > 0) {
        if (runs && runCount > 0) {
            ConfigureStyledText(t, buf, len, 0, 16, col.textBase, runs, runCount);
        } else {
            ConfigureText(t, buf, len, 0, 16, col.textBase);
        }
    } else {
        ConfigureText(t, placeholder.data(), static_cast<u32>(placeholder.size()), 0, 16, col.textMuted);
    }
    if (focused) {
        ConfigureCaret(t, static_cast<s32>(caret));
        if (caret != anchor) {
            ConfigureSelection(t, detail::Lo(caret, anchor), detail::Hi(caret, anchor));
        }
    }
    CloseElement();

    // Error indicator: a fixed-size slot INSIDE the box, reserved for every
    // parser-backed field (empty until an error occurs). Because the space is
    // always held, toggling an error only fills the badge in - the box height and
    // everything below it never move, so no layout reallocation. On error it shows
    // a red "!" badge; the full diagnostic is a floating tooltip (built after the
    // box) revealed on hover, so the message never grows the field either.
    const bool hasParser = parser.Parse != nullptr;
    UiId errId = HashChild(fid, 4);
    if (hasParser) {
        LayoutConfig ind {};
        ind.sizing = { Fixed(16), Fixed(16) };
        ind.justify = Justify::Center;
        ind.align = AlignCross::Center;
        ind.hitTestable = hasError; // only a hover target when there is an error
        if (hasError) {
            ind.background = col.alertDanger;
            ind.cornerRadius = 8; // circular badge
        }
        OpenElement(ind, errId);
        if (hasError) {
            LayoutConfig ex {};
            ex.hitTestable = false;
            u32 exn = OpenElement(ex, HashChild(fid, 5));
            ConfigureText(exn, "!", 1, 0, 13, UiColor { 255, 255, 255, 255 });
            CloseElement();
        }
        CloseElement(); // ind
    }
    CloseElement(); // box

    // Floating diagnostic: shown below the box while the field is focused (so it is
    // visible as you type) or the "!" badge is hovered. Floating -> drawn on its own
    // layer, lifted out of flow, so it never resizes the field or shifts the layout.
    if (hasParser && hasError && message && (focused || IsHovered(errId))) {
        LayoutConfig tip {};
        tip.floating.enabled = true;
        tip.floating.placement = FloatPlacement::BelowAnchor;
        tip.floating.offset = { 0, 4 };
        tip.direction = Direction::TopToBottom;
        tip.padding = { 8, 8, 6, 6 };
        tip.sizing = { Fixed(240), Fit() };
        tip.background = col.bgDark;
        tip.cornerRadius = 4;
        tip.border = { 1, 1, 1, 1 };
        tip.borderColor = col.alertDanger;
        OpenElement(tip, HashChild(fid, 6));
        LayoutConfig mt {};
        mt.hitTestable = false;
        mt.sizing = { Grow(), Fit() };
        u32 mm = OpenElement(mt, HashChild(fid, 7));
        ConfigureText(mm, message, static_cast<u32>(std::strlen(message)), 0, 13,
            col.alertDanger, /*wrap*/ true);
        CloseElement();
        CloseElement(); // tip
    }
    CloseElement(); // column

    bool changed = detail::HandleTextInput(fid, buf, len, cap, caret, anchor, /*multiline*/ false);
    if (IsFocused(fid)) {
        SetCaretPos(caret);
        SetSelectAnchorPos(anchor);
    }
    return changed;
}

// ---- InputBox: multi-line -----------------------------------------------------

inline bool InputBox(char* buf, u32& len, u32 cap, std::string_view placeholder,
    UiId id = kNullId, f32 height = 96)
{
    const ColorScheme& col = Colors();
    UiId fid = id != kNullId ? id : HashId(buf, 12);
    bool focused = IsFocused(fid);

    u32 caret = CaretPos();
    if (caret > len) {
        caret = len;
    }
    u32 anchor = SelectAnchorPos();
    if (anchor > len) {
        anchor = len;
    }

    LayoutConfig box {};
    box.sizing = { Grow(), Fixed(height) };
    box.padding = { 8, 8, 6, 6 };
    box.direction = Direction::TopToBottom;
    box.align = AlignCross::Stretch;
    box.cornerRadius = 4;
    box.background = col.bgLight;
    box.border = { 1, 1, 1, 1 };
    box.borderColor = focused ? col.accentPrimary : col.borderBase;
    box.scroll = true;
    OpenElement(box, fid);

    LayoutConfig tc {};
    tc.hitTestable = false;
    tc.sizing = { Grow(), Fit() };
    u32 t = OpenElement(tc, HashChild(fid, 1));
    if (len > 0) {
        ConfigureText(t, buf, len, 0, 16, col.textBase, /*wrap*/ true);
    } else {
        ConfigureText(t, placeholder.data(), static_cast<u32>(placeholder.size()), 0, 16, col.textMuted, /*wrap*/ true);
    }
    if (focused) {
        ConfigureCaret(t, static_cast<s32>(caret));
        if (caret != anchor) {
            ConfigureSelection(t, detail::Lo(caret, anchor), detail::Hi(caret, anchor));
        }
    }
    CloseElement();
    CloseElement(); // box

    bool changed = detail::HandleTextInput(fid, buf, len, cap, caret, anchor, /*multiline*/ true);
    if (IsFocused(fid)) {
        SetCaretPos(caret);
        SetSelectAnchorPos(anchor);
    }
    return changed;
}

} // namespace Ui
