#pragma once
#include "UiContext.h"
#include "UiTypes.h"
#include <string_view>

// Prebuilt checkbox bound to a caller-owned bool. The whole row is clickable (its
// decoration children opt out of hit-testing). Returns true on the frame it toggles.
namespace Ui {

inline bool Checkbox(bool& checked, std::string_view label, UiId id = kNullId)
{
    const ColorScheme& col = Colors();
    UiId rowId = id != kNullId ? id : HashId(label.data(), 7);
    bool hovered = IsHovered(rowId);

    LayoutConfig row {};
    row.direction = Direction::LeftToRight;
    row.gap = 8;
    row.align = AlignCross::Center;
    row.padding = { 4, 4, 4, 4 };
    row.sizing = { Fit(), Fit() };
    OpenElement(row, rowId);

    // Check box indicator.
    LayoutConfig box {};
    box.sizing = { Fixed(18), Fixed(18) };
    box.cornerRadius = 4;
    box.border = { 1, 1, 1, 1 };
    box.borderColor = hovered ? col.accentPrimary : col.borderBase;
    box.background = checked ? col.accentPrimary : col.bgLight;
    box.hitTestable = false;
    OpenElement(box, HashChild(rowId, 1));
    CloseElement();

    // Label.
    LayoutConfig lc {};
    lc.hitTestable = false;
    u32 t = OpenElement(lc, HashChild(rowId, 2));
    ConfigureText(t, label.data(), static_cast<u32>(label.size()), 0, 16, col.textBase);
    CloseElement();

    CloseElement(); // row

    bool toggled = false;
    if (IsClicked(rowId)) {
        checked = !checked;
        toggled = true;
    }
    return toggled;
}

} // namespace Ui
