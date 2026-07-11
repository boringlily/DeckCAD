#pragma once
#include "UiContext.h"
#include "UiTypes.h"
#include <string_view>

// Prebuilt dropdown built on the floating-panel mechanism. Usage:
//
//   if (Ui::BeginDropdown(open, "Choose", id)) {
//       if (Ui::DropdownItem("Alpha")) { open = false; /* ... */ }
//       if (Ui::DropdownItem("Beta"))  { open = false; /* ... */ }
//       Ui::EndDropdown();
//   }
//
// The menu is a floating element anchored below the trigger, so it draws above
// and hit-tests before the base tree. Clicking the base while open closes it.
namespace Ui {

inline bool BeginDropdown(bool& open, std::string_view label, UiId id = kNullId)
{
    const ColorScheme& col = Colors();
    UiId tid = id != kNullId ? id : HashId(label.data(), 13);
    bool hovered = IsHovered(tid);

    LayoutConfig trigger {};
    trigger.direction = Direction::LeftToRight;
    trigger.gap = 10;
    trigger.align = AlignCross::Center;
    trigger.padding = { 10, 10, 6, 6 };
    trigger.sizing = { Fit(), Fit() };
    trigger.cornerRadius = 4;
    trigger.background = hovered ? col.bgDark : col.bgBase;
    trigger.border = { 1, 1, 1, 1 };
    trigger.borderColor = col.borderBase;
    OpenElement(trigger, tid);

    LayoutConfig lc {};
    lc.hitTestable = false;
    u32 t = OpenElement(lc, HashChild(tid, 1));
    ConfigureText(t, label.data(), static_cast<u32>(label.size()), 0, 16, col.textBase);
    CloseElement();

    LayoutConfig ac {};
    ac.hitTestable = false;
    const char* arrow = open ? "^" : "v";
    u32 a = OpenElement(ac, HashChild(tid, 2));
    ConfigureText(a, arrow, 1, 0, 16, col.textMuted);
    CloseElement();

    if (IsClicked(tid)) {
        open = !open;
    }
    // Dismiss when the user presses on the base layer outside the trigger/menu.
    if (open) {
        Context* ctx = Current();
        if (ctx && ctx->input.pointer.pressed && ctx->input.hotLayer == 0 && !IsHovered(tid)) {
            open = false;
        }
    }

    if (open) {
        LayoutConfig menu {};
        menu.floating.enabled = true;
        menu.floating.placement = FloatPlacement::BelowAnchor;
        menu.floating.offset = { 0, 4 };
        menu.direction = Direction::TopToBottom;
        menu.gap = 2;
        menu.padding = { 4, 4, 4, 4 };
        menu.sizing = { Fixed(200), Fit() };
        menu.background = col.bgLight;
        menu.cornerRadius = 6;
        menu.border = { 1, 1, 1, 1 };
        menu.borderColor = col.borderBase;
        OpenElement(menu, HashChild(tid, 3));
        return true;
    }

    CloseElement(); // trigger
    return false;
}

inline void EndDropdown()
{
    CloseElement(); // menu
    CloseElement(); // trigger
}

// A selectable row inside an open dropdown. Returns true on the frame it is clicked.
inline bool DropdownItem(std::string_view label, UiId id = kNullId)
{
    const ColorScheme& col = Colors();
    UiId iid = id != kNullId ? id : HashId(label.data(), 17);
    bool hovered = IsHovered(iid);

    LayoutConfig item {};
    item.direction = Direction::LeftToRight;
    item.align = AlignCross::Center;
    item.padding = { 8, 8, 6, 6 };
    item.sizing = { Grow(), Fit() };
    item.cornerRadius = 4;
    item.background = hovered ? col.accentPrimary : UiColor { 0, 0, 0, 0 };
    OpenElement(item, iid);

    LayoutConfig lc {};
    lc.hitTestable = false;
    u32 t = OpenElement(lc, HashChild(iid, 1));
    ConfigureText(t, label.data(), static_cast<u32>(label.size()), 0, 16, col.textBase);
    CloseElement();

    CloseElement(); // item
    return IsClicked(iid);
}

} // namespace Ui
