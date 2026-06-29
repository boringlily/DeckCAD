#pragma once
#include "DTL.h"

// Core POD value types for the Ui framework. Depends only on DTL.h scalar aliases.
// Everything here is trivially copyable so it can live directly inside arena nodes.
namespace Ui {

struct Vec2 {
    f32 x { 0 };
    f32 y { 0 };
};

struct Rect {
    f32 x { 0 };
    f32 y { 0 };
    f32 w { 0 };
    f32 h { 0 };
};

// User callback for a custom-draw region (e.g. a 3D viewport): invoked at dispatch
// with the element's final rect. The `user` pointer is supplied alongside it.
using CustomDrawFn = void (*)(void* user, Rect rect);

// Mirrors the layout of Graphics' StyleColor so themes translate 1:1.
struct UiColor {
    u8 r { 255 };
    u8 g { 255 };
    u8 b { 255 };
    u8 a { 255 };
};

// A colorized/decorated run over a slice of text, produced by a user TextParser
// for syntax highlighting and inline error marking.
enum class TextDecoration : u8 { None = 0,
    Underline = 1,
    Error = 2 };

struct TextStyleRun {
    u32 start { 0 }; // byte offset into the text.
    u32 length { 0 };
    UiColor color {};
    TextDecoration decoration { TextDecoration::None };
};

// Output a TextParser fills: style runs (sorted, non-overlapping, in-bounds) over
// the text, plus an optional one-line diagnostic to show under the field.
struct TextParseResult {
    TextStyleRun* runs { nullptr }; // framework-provided scratch buffer.
    u32 runCount { 0 }; // parser sets this.
    u32 runCap { 0 }; // capacity of `runs`.
    const char* message { nullptr }; // optional status/error text (stable storage).
    bool hasError { false };
};

// User-supplied parser/colorizer/styler bound to an editable text box. Called each
// frame with the current text; fills `out` with highlight runs and diagnostics.
struct TextParser {
    void* user { nullptr };
    void (*Parse)(void* user, const char* text, u32 len, TextParseResult* out) { nullptr };
};

enum class Axis : u8 { X = 0,
    Y = 1 };

enum class Direction : u8 { LeftToRight = 0,
    TopToBottom = 1 };

// How a single axis of an element is sized.
//   Fit     - shrink-wrap to children / text (intrinsic).
//   Grow    - expand to fill remaining space in the parent.
//   Fixed   - exact pixel size (value).
//   Percent - fraction (0..1) of the parent's inner size (value).
enum class SizeKind : u8 { Fit,
    Grow,
    Fixed,
    Percent };

// Main-axis distribution. Vertical slice wires Start only; rest reserved.
enum class Justify : u8 { Start,
    Center,
    End,
    SpaceBetween };

// Cross-axis alignment. Vertical slice wires Start / Center / Stretch.
enum class AlignCross : u8 { Start,
    Center,
    End,
    Stretch };

struct SizeAxis {
    SizeKind kind { SizeKind::Fit };
    f32 value { 0 }; // Fixed: px, Percent: 0..1, ignored for Fit/Grow.
    f32 min { 0 };
    f32 max { 0 }; // 0 == unbounded.
};

struct Sizing {
    SizeAxis w {};
    SizeAxis h {};
};

struct Edges {
    f32 left { 0 };
    f32 right { 0 };
    f32 top { 0 };
    f32 bottom { 0 };
};

// Where a floating element anchors. BelowAnchor/AboveAnchor/RightOfAnchor attach
// to the element's flow parent (dropdowns, tooltips); ScreenCenter / ScreenFill /
// ScreenPosition ignore the parent and use the window (modals, overlays).
enum class FloatPlacement : u8 { BelowAnchor,
    AboveAnchor,
    RightOfAnchor,
    ScreenCenter,
    ScreenFill,
    ScreenPosition };

// Marks an element as floating: lifted out of normal flow, laid out and drawn on
// its own layer above the base tree (see Layout/Emit/Input float handling).
struct FloatConfig {
    bool enabled { false };
    FloatPlacement placement { FloatPlacement::BelowAnchor };
    Vec2 offset {}; // added to the computed anchor origin.
    bool clampToScreen { true }; // keep the box inside the window.
};

// One LayoutConfig is produced per element at Begin() and copied into its node.
struct LayoutConfig {
    Sizing sizing {};
    Direction direction { Direction::LeftToRight };
    Edges padding {};
    f32 gap { 0 };
    Justify justify { Justify::Start };
    AlignCross align { AlignCross::Start };
    UiColor background { 0, 0, 0, 0 }; // a == 0 -> no rectangle emitted.
    f32 cornerRadius { 0 };
    Edges border {}; // per-side border width (0 -> no border on that side).
    UiColor borderColor { 0, 0, 0, 0 };
    bool clip { false }; // clip children to this element's box (scissor).
    bool scroll { false }; // scroll overflowing content along the main axis (implies clip).
    bool hitTestable { true }; // false -> ignored by hit-testing; clicks fall through to the parent.
    FloatConfig floating {};
};

// Convenience constructors for the common sizing cases (keeps user code terse).
constexpr SizeAxis Fit(f32 min = 0, f32 max = 0) { return { SizeKind::Fit, 0, min, max }; }
constexpr SizeAxis Grow(f32 min = 0, f32 max = 0) { return { SizeKind::Grow, 0, min, max }; }
constexpr SizeAxis Fixed(f32 px) { return { SizeKind::Fixed, px, 0, 0 }; }
constexpr SizeAxis Percent(f32 frac) { return { SizeKind::Percent, frac, 0, 0 }; }

} // namespace Ui
