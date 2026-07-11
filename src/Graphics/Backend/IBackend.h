#pragma once
#include "DTL.h"
#include "UiTypes.h"

// Backend abstraction as POD function-pointer vtable structs (not abstract
// classes): clean under -fno-exceptions, needs no RTTI, and is trivially
// swappable for a future Raylib+Angle/GLES backend without a virtual ABI.
//
// Each sub-backend carries an opaque `user` pointer (e.g. a loaded Font array)
// that the framework passes back into every call.
namespace Ui {

struct TextMetrics {
    f32 width { 0 };
    f32 height { 0 };
};

struct TextBackend {
    void* user { nullptr };
    TextMetrics (*Measure)(void* user, const char* text, u32 len, u16 fontId, u16 fontSize) { nullptr };
    void (*Draw)(void* user, const char* text, u32 len, Vec2 pos, u16 fontId, u16 fontSize, UiColor color) { nullptr };
    // Word-wrap variants (optional). Measure returns the wrapped block size for the
    // given max width; Draw renders wrapped lines inside `box`. If null, the
    // framework falls back to the single-line Measure/Draw.
    TextMetrics (*MeasureWrapped)(void* user, const char* text, u32 len, u16 fontId, u16 fontSize, f32 maxWidth) { nullptr };
    // Wrapped draw with an optional editing caret (caretByte >= 0) placed at the
    // right wrapped line + x, and an optional selection highlight [selStart, selEnd).
    void (*DrawWrapped)(void* user, const char* text, u32 len, Rect box, u16 fontId, u16 fontSize, UiColor color, s32 caretByte, u32 selStart, u32 selEnd, UiColor selColor) { nullptr };
    // Draw single-line text with per-run colors + underlines (syntax highlighting),
    // an optional caret (caretByte >= 0), and an optional selection highlight. Runs
    // not covering a glyph fall back to `defaultColor`.
    void (*DrawStyled)(void* user, const char* text, u32 len, Rect box, u16 fontId, u16 fontSize, UiColor defaultColor, const TextStyleRun* runs, u32 runCount, s32 caretByte, u32 selStart, u32 selEnd, UiColor selColor) { nullptr };
    // Map a point (window coords) to the nearest byte boundary in a text box. Used
    // for click-to-position / drag-select. `caret` lets single-line text account for
    // its horizontal scroll; `wrap` selects multi-line mapping.
    u32 (*CaretIndexAt)(void* user, const char* text, u32 len, Rect box, u16 fontId, u16 fontSize, s32 caret, Vec2 point, bool wrap) { nullptr };
};

struct ImageBackend {
    void* user { nullptr };
    void (*Draw)(void* user, void* imageHandle, Rect dst, UiColor tint) { nullptr };
};

// System clipboard access (text), for cut/copy/paste in text inputs.
struct ClipboardBackend {
    void* user { nullptr };
    void (*Set)(void* user, const char* text) { nullptr };
    const char* (*Get)(void* user) { nullptr };
};

// Deferred past the slice; defined so the vtable shape is stable.
struct IconBackend {
    void* user { nullptr };
    void (*Draw)(void* user, u32 iconId, Rect dst, UiColor tint) { nullptr };
};

struct DrawBackend {
    void* user { nullptr };
    void (*FillRect)(void* user, Rect box, UiColor color, f32 cornerRadius) { nullptr };
    void (*Border)(void* user, Rect box, Edges width, UiColor color, f32 cornerRadius) { nullptr };
    void (*ScissorStart)(void* user, Rect box) { nullptr };
    void (*ScissorEnd)(void* user) { nullptr };
};

// The app's single theme/palette: the one place color values live. Every themed
// component reads it via Ui::Colors(), and Graphics::Initialize() hands it to
// MakeBackend() as-is (there used to be a second, Clay-era palette in
// Graphics/Style.h kept in sync by a hand-written translation at startup; that
// shim is gone, and these defaults are now the single, non-drifting source of
// truth — swap them, or add a named alternate ColorScheme constant, to add a
// new theme).
struct ColorScheme {
    UiColor bgDark { 210, 210, 210, 255 };
    UiColor bgBase { 239, 239, 239, 255 };
    UiColor bgLight { 255, 255, 255, 255 };
    UiColor textBase { 13, 13, 13, 255 };
    UiColor textMuted { 128, 128, 128, 255 };
    UiColor accentPrimary { 172, 153, 255, 255 };
    UiColor accentSecondary { 151, 71, 255, 255 };
    UiColor borderBase { 102, 102, 102, 255 };
    UiColor alertDanger { 255, 127, 127, 255 };
    UiColor alertWarning { 255, 225, 127, 255 };
    UiColor alertSuccess { 149, 255, 127, 255 };
    UiColor alertInfo { 127, 212, 255, 255 };
    UiColor selection { 172, 153, 255, 110 }; // text-selection highlight (semi-transparent).
};

struct UiBackend {
    DrawBackend draw {};
    TextBackend text {};
    ImageBackend image {};
    IconBackend icon {};
    ClipboardBackend clipboard {};
    ColorScheme colors {};
};

} // namespace Ui
