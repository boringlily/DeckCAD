#pragma once
#include "Component.h"
#include "UiTypes.h"
#include <string_view>

// Text leaf component. Subclass and override the style hooks, then Draw() text.
// A label opens, configures, and closes a node in a single call (it has no
// children).
namespace Ui {

class Label : public Component {
public:
    virtual u16 FontId() const { return 0; }
    virtual u16 FontSize() const { return 16; }
    virtual UiColor TextColor() const { return UiColor { 0, 0, 0, 255 }; }
    virtual LayoutConfig Layout() const { return LayoutConfig {}; } // Fit by default.

    // Single string_view entry point: a raw (ptr,len) caller passes
    // std::string_view{ptr, len}. Keeping one overload avoids the literal-plus-id
    // ambiguity where an id would otherwise bind to a `len` parameter.
    void Draw(std::string_view s, UiId id = kNullId)
    {
        id_ = id != kNullId ? id : HashId(this, 0);
        u32 n = OpenElement(Layout(), id_);
        ConfigureText(n, s.data(), static_cast<u32>(s.size()), FontId(), FontSize(), TextColor());
        CloseElement();
        node_ = kNullIndex;
    }
};

} // namespace Ui
