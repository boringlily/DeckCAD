#pragma once
#include "Button.h"
#include "Composition.h"
#include "UiContext.h"
#include "UiTypes.h"
#include <string_view>

// Prebuilt modal dialog on the floating-panel mechanism: a full-screen dimmed
// backdrop (its own layer, captures clicks) with a centered dialog box.
//
//   if (Ui::BeginModal(open, id)) {
//       Ui::Text("Title", 20);
//       ... dialog body ...
//       Ui::EndModal();
//   }
//
// Pressing the backdrop (outside the dialog) or Escape closes it.
namespace Ui {

inline bool BeginModal(bool& open, UiId id = kNullId, f32 width = 360)
{
    if (!open) {
        return false;
    }
    const ColorScheme& col = Colors();
    UiId backdropId = id != kNullId ? id : HashId(&open, 31);

    LayoutConfig backdrop {};
    backdrop.floating.enabled = true;
    backdrop.floating.placement = FloatPlacement::ScreenFill;
    backdrop.sizing = { Grow(), Grow() };
    backdrop.justify = Justify::Center;
    backdrop.align = AlignCross::Center;
    backdrop.background = UiColor { 0, 0, 0, 140 };
    OpenElement(backdrop, backdropId);

    LayoutConfig dialog {};
    dialog.direction = Direction::TopToBottom;
    dialog.sizing = { Fixed(width), Fit() };
    dialog.padding = { 20, 20, 18, 18 };
    dialog.gap = 12;
    dialog.align = AlignCross::Stretch; // stretch children to the dialog width (wrapping body).
    dialog.cornerRadius = 8;
    dialog.background = col.bgLight;
    dialog.border = { 1, 1, 1, 1 };
    dialog.borderColor = col.borderBase;
    OpenElement(dialog, HashChild(backdropId, 1));

    // Dismiss on backdrop press (the dialog occludes the center) or Escape.
    if (IsClicked(backdropId) || Keyboard().escape) {
        open = false;
    }
    return true;
}

inline void EndModal()
{
    CloseElement(); // dialog
    CloseElement(); // backdrop
}

// Convenience: title + message + an OK button. Returns true when OK is pressed
// (and closes the dialog). Build your own via BeginModal/EndModal for custom body.
inline bool MessageBox(std::string_view title, std::string_view message, bool& open, UiId id = kNullId)
{
    UiId seed = id != kNullId ? id : HashId(title.data(), 23);
    if (!BeginModal(open, seed)) {
        return false;
    }
    Text(title, 20);
    Paragraph(message, 16, Colors().textMuted);

    LayoutConfig bar {};
    bar.direction = Direction::LeftToRight;
    bar.sizing = { Grow(), Fit() };
    bar.justify = Justify::End;
    bar.gap = 8;
    OpenElement(bar, HashChild(seed, 9));
    bool ok = inlineButton("OK", HashChild(seed, 10));
    CloseElement();

    EndModal();
    if (ok) {
        open = false;
    }
    return ok;
}

} // namespace Ui
