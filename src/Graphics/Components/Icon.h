#pragma once
#include "Component.h"
#include "UiTypes.h"

// Icon leaf component. Draws a square icon resolved by the backend's IconBackend
// from an integer id (e.g. an app-side IconId enum).
namespace Ui {

class Icon : public Component {
public:
    virtual f32 Size() const { return 24; }
    // Default to the theme's base text color so monochrome icons read like text.
    // Override (e.g. to white) to show a multi-color icon in its own colors.
    virtual UiColor Tint() const { return Colors().textBase; }

    void Draw(s32 iconId, UiId id = kNullId)
    {
        id_ = id != kNullId ? id : HashId(this, 0);
        LayoutConfig c {};
        c.sizing = { Fixed(Size()), Fixed(Size()) };
        c.hitTestable = false;
        u32 n = OpenElement(c, id_);
        ConfigureIcon(n, iconId, Tint());
        CloseElement();
        node_ = kNullIndex;
    }
};

} // namespace Ui
