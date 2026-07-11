#pragma once
#include "Label.h"
#include "Panel.h"
#include <string_view>

// Prebuilt prototyping components built by composition over Panel + Label.
//   Button      - calls a callback when clicked (event style).
//   inlineButton - returns true the frame it is clicked (simpler inline logic),
//                  mirroring the existing ToolSelectButton(...) pattern.
namespace Ui {

class Button : public Panel {
public:
    // Default look: shrink-wrap to the label with a little padding.
    LayoutConfig Layout() const override
    {
        LayoutConfig c {};
        c.sizing = { Fit(), Fit() };
        c.padding = { 10, 10, 6, 6 };
        c.align = AlignCross::Center;
        return c;
    }
    UiColor Color() const override
    {
        return Hovered() ? Colors().accentPrimary : Colors().bgDark;
    }
    virtual u16 FontId() const { return 0; }
    virtual u16 FontSize() const { return 16; }
    virtual UiColor LabelColor() const { return Colors().textBase; }

    template <typename Fn>
    void Draw(std::string_view label, Fn&& onClick, UiId id = kNullId)
    {
        Begin(id);
        LayoutConfig labelCfg {};
        labelCfg.hitTestable = false; // let the button itself capture the click.
        u32 textNode = OpenElement(labelCfg, HashChild(id_, HashId(label.data(), 0)));
        ConfigureText(textNode, label.data(), static_cast<u32>(label.size()), FontId(), FontSize(), LabelColor());
        CloseElement();
        if (Clicked()) {
            onClick();
        }
        End();
    }
};

// Stamp a default-styled button; true on the frame it is clicked.
inline bool inlineButton(std::string_view label, UiId id = kNullId)
{
    static Button proto;
    bool clicked = false;
    proto.Draw(
        label, [&clicked]() { clicked = true; }, id != kNullId ? id : HashId(label.data(), 0));
    return clicked;
}

} // namespace Ui
