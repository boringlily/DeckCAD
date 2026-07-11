#pragma once
#include "Component.h"
#include "UiTypes.h"

// Abstract base component: a rectangle with positioning and color. The user
// subclasses Panel, overrides Layout()/Color() to describe their element, and
// keeps a single instance which they Begin()/End() around children.
namespace Ui {

class Panel : public Component {
public:
    // Required: describe sizing / direction / padding / gap / alignment.
    virtual LayoutConfig Layout() const = 0;
    // Optional: background color. Defaults to transparent (no rectangle).
    virtual UiColor Color() const { return UiColor { 0, 0, 0, 0 }; }

    // Open this element. Pass an explicit id when stamping the same instance more
    // than once (e.g. HashId(this, i) in a loop) so ids stay distinct and stable.
    Panel& Begin(UiId id = kNullId)
    {
        id_ = id != kNullId ? id : HashId(this, 0);
        LayoutConfig cfg = Layout();
        cfg.background = Color();
        node_ = OpenElement(cfg, id_);
        return *this;
    }

    Scope BeginScope(UiId id = kNullId)
    {
        Begin(id);
        return Scope { true };
    }

    void End()
    {
        CloseElement();
        node_ = kNullIndex;
    }

    // Read last frame's resolved hit-test for this element.
    bool Hovered() const { return IsHovered(id_); }
    bool Clicked() const { return IsClicked(id_); }
};

} // namespace Ui
