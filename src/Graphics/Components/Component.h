#pragma once
#include "UiContext.h"
#include "UiId.h"
#include "UiNode.h"

// Base plumbing shared by the OOP component wrappers. A user keeps a single
// long-lived instance of a subclass; it stores only the id/node of the element it
// is *currently* marking, never per-frame tree data (that lives in the arena).
namespace Ui {

class Component {
public:
    virtual ~Component() = default;

protected:
    u32 node_ { kNullIndex }; // arena node index while this element is open.
    UiId id_ { kNullId }; // id of the most recently opened element.
};

// RAII scope guard giving the `if (auto s = panel.BeginScope()) { ... }` feel,
// closing the element when the guard leaves scope.
struct Scope {
    bool active_ { false };
    explicit Scope(bool active)
        : active_ { active }
    {
    }
    ~Scope()
    {
        if (active_) {
            CloseElement();
        }
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    explicit operator bool() const { return true; }
};

} // namespace Ui
