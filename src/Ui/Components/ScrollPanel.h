#pragma once
#include "UiContext.h"
#include "UiTypes.h"
#include <string_view>

// Prebuilt vertical scroll container. Content taller than `height` is clipped and
// scrolled with the mouse wheel (offset persists across frames, keyed by id).
//
//   Ui::BeginScrollPanel(id, 160);
//   for (...) Ui::ListItem(label, label == selected, itemId);
//   Ui::EndScrollPanel();
//
// width <= 0 means Grow (fill the parent's cross axis).
namespace Ui {

inline void BeginScrollPanel(UiId id, f32 height, f32 width = 0, f32 gap = 4)
{
    const ColorScheme& col = Colors();
    LayoutConfig c {};
    c.direction = Direction::TopToBottom;
    c.sizing = { width > 0 ? Fixed(width) : Grow(), Fixed(height) };
    c.gap = gap;
    c.padding = { 8, 8, 8, 8 };
    c.scroll = true;
    c.align = AlignCross::Stretch; // items fill the panel width.
    c.background = col.bgLight;
    c.border = { 1, 1, 1, 1 };
    c.borderColor = col.borderBase;
    c.cornerRadius = 6;
    OpenElement(c, id);
}

inline void EndScrollPanel() { CloseElement(); }

// A selectable, full-width list row. Returns true on the frame it is clicked.
inline bool ListItem(std::string_view label, bool selected, UiId id)
{
    const ColorScheme& col = Colors();
    bool hovered = IsHovered(id);

    LayoutConfig item {};
    item.direction = Direction::LeftToRight;
    item.align = AlignCross::Center;
    item.padding = { 10, 10, 8, 8 };
    item.sizing = { Grow(), Fit() };
    item.cornerRadius = 4;
    item.background = selected ? col.accentPrimary : (hovered ? col.bgBase : UiColor { 0, 0, 0, 0 });
    OpenElement(item, id);

    LayoutConfig lc {};
    lc.hitTestable = false;
    u32 t = OpenElement(lc, HashChild(id, 1));
    ConfigureText(t, label.data(), static_cast<u32>(label.size()), 0, 16, col.textBase);
    CloseElement();

    CloseElement(); // item
    return IsClicked(id);
}

} // namespace Ui
