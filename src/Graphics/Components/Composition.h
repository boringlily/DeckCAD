#pragma once
#include "Component.h"
#include "UiContext.h"
#include "UiTypes.h"
#include <string_view>

// Lightweight composition helpers for building widgets and stamping repetitive
// layout without subclassing a Panel per element. These are thin wrappers over
// the OpenElement/CloseElement primitives.
namespace Ui {

// A horizontal stack. Pair with EndRow().
inline void BeginRow(UiId id, f32 gap = 8, AlignCross align = AlignCross::Center)
{
    LayoutConfig c {};
    c.direction = Direction::LeftToRight;
    c.gap = gap;
    c.align = align;
    c.sizing = { Fit(), Fit() };
    OpenElement(c, id);
}
inline void EndRow() { CloseElement(); }

// A vertical stack. Pair with EndColumn().
inline void BeginColumn(UiId id, f32 gap = 8, AlignCross align = AlignCross::Start)
{
    LayoutConfig c {};
    c.direction = Direction::TopToBottom;
    c.gap = gap;
    c.align = align;
    c.sizing = { Fit(), Fit() };
    OpenElement(c, id);
}
inline void EndColumn() { CloseElement(); }

// A flexible gap that eats remaining space on the parent's main axis (only has
// effect inside a Grow-sized parent). Non-interactive.
inline void Spacer(UiId id = kNullId)
{
    static const char marker = 0;
    LayoutConfig c {};
    c.sizing = { Grow(), Grow() };
    c.hitTestable = false;
    OpenElement(c, id != kNullId ? id : HashId(&marker, 0));
    CloseElement();
}

// A standalone text run. Color alpha 0 means "use the theme's base text color".
inline void Text(std::string_view s, u16 fontSize = 16, UiColor color = { 0, 0, 0, 0 }, UiId id = kNullId)
{
    if (color.a == 0) {
        color = Colors().textBase;
    }
    LayoutConfig c {};
    c.hitTestable = false;
    u32 n = OpenElement(c, id != kNullId ? id : HashId(s.data(), 5));
    ConfigureText(n, s.data(), static_cast<u32>(s.size()), 0, fontSize, color);
    CloseElement();
}

// Word-wrapping text. Width Grow, so it fills the parent's inner width when the
// parent stretches (or grows) it; the height then reflows to fit. Use inside a
// width-constrained container (Fixed width, or a column with align Stretch).
inline void Paragraph(std::string_view s, u16 fontSize = 16, UiColor color = { 0, 0, 0, 0 }, UiId id = kNullId)
{
    if (color.a == 0) {
        color = Colors().textBase;
    }
    LayoutConfig c {};
    c.hitTestable = false;
    c.sizing = { Grow(), Fit() };
    u32 n = OpenElement(c, id != kNullId ? id : HashId(s.data(), 41));
    ConfigureText(n, s.data(), static_cast<u32>(s.size()), 0, fontSize, color, /*wrap*/ true);
    CloseElement();
}

} // namespace Ui
