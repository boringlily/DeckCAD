#pragma once
#include "DTL.h"
#include "UiId.h"
#include "UiTypes.h"
#include <type_traits>

// One fixed-size POD node per element, stored in a flat array in the arena. The
// element tree is intrusive: parent/child/sibling links are u32 indices into that
// array (kNullIndex == none), so building the tree allocates nothing beyond the
// next slot.
namespace Ui {

constexpr u32 kNullIndex = u32_max;

struct UiNode {
    UiId id { kNullId };
    LayoutConfig cfg {};

    // Intrusive tree links (indices into Context::nodes).
    u32 parent { kNullIndex };
    u32 firstChild { kNullIndex };
    u32 lastChild { kNullIndex };
    u32 nextSibling { kNullIndex };
    u32 childCount { 0 };

    // Text payload for Label nodes; textLen == 0 means "not a text node".
    const char* textPtr { nullptr };
    u32 textLen { 0 };
    u16 fontId { 0 };
    u16 fontSize { 16 };
    UiColor textColor { 0, 0, 0, 255 };
    bool textWrap { false }; // word-wrap to the node's resolved width.
    const TextStyleRun* styleRuns { nullptr }; // syntax-highlight runs (frame scratch).
    u32 styleRunCount { 0 };
    s32 caretByte { -1 }; // caret position for an editable text node; -1 = none.
    u32 selStart { 0 }; // selection range [selStart, selEnd); empty when equal.
    u32 selEnd { 0 };

    // Icon payload; iconId < 0 means "no icon".
    s32 iconId { -1 };
    UiColor iconTint { 255, 255, 255, 255 };

    // Custom draw callback (e.g. a 3D viewport), invoked at dispatch with the rect.
    CustomDrawFn customDraw { nullptr };
    void* customUser { nullptr };

    // Compositing layer: 0 is the base tree, each floating root gets a higher
    // layer so it draws above and wins hit-testing. Inherited by descendants.
    u16 layer { 0 };

    // Solver scratch, filled during Solve().
    Vec2 measured {}; // intrinsic size after the fit pass (clamped to fixed/view size).
    Vec2 contentSize {}; // natural content extent (may exceed measured for scroll containers).
    Rect rect {}; // final absolute box after the position pass.
};

static_assert(std::is_trivially_copyable_v<UiNode>, "UiNode must stay POD for arena storage");

} // namespace Ui
