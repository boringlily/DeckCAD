#pragma once
#include "Arena.h"
#include "DTL.h"
#include "IBackend.h"
#include "RenderCommand.h"
#include "Ui.export.h"
#include "UiId.h"
#include "UiNode.h"
#include "UiTypes.h"

// The per-window Ui context. Owns the arena views over the user buffer, the flat
// node array, the open-element stack, persistent input state, and the bound
// backend. One context is active at a time (set via SetCurrent); the Components
// sugar targets the current context, while the primitives below take it directly.
namespace Ui {

struct PointerState {
    Vec2 pos {};
    Vec2 wheel {}; // scroll wheel delta this frame.
    bool down { false };
    bool pressed { false }; // went down this frame.
    bool released { false }; // went up this frame.
};

// Persistent per-element scroll offset (along the element's main axis), keyed by
// id so it survives the immediate-mode rebuild.
struct ScrollState {
    UiId id { kNullId };
    f32 offset { 0 };
};

// Per-frame keyboard input, filled by the app from its platform layer. The
// focused element (e.g. a text field) consumes it during build.
struct KeyboardState {
    u32 typed[16] {}; // unicode codepoints typed this frame.
    u32 typedCount { 0 };
    bool backspace { false };
    bool del { false };
    bool left { false };
    bool right { false };
    bool up { false };
    bool down { false };
    bool home { false };
    bool end { false };
    bool enter { false };
    bool tab { false };
    bool escape { false };
    bool shift { false }; // modifier: extend selection on caret movement.
    bool copy { false }; // Ctrl+C.
    bool cut { false }; // Ctrl+X.
    bool paste { false }; // Ctrl+V.
    bool selectAll { false }; // Ctrl+A.
};

struct InputState {
    PointerState pointer {};
    KeyboardState keyboard {};
    UiId hotId { kNullId }; // element under the pointer (resolved at EndFrame).
    u16 hotLayer { 0 }; // layer of the hovered element.
    Rect hotRect {}; // rect of the hovered element (for click-to-position in inputs).
    // Snapshot of the hot element's ancestor ids (nearest-first), captured at
    // ResolveInput while the tree is complete. IsHoverWithin consults this instead
    // of walking the node array by index: the array is reset (nodeCount = 0) every
    // BeginFrame and is only partially built when hover is queried mid-build, so a
    // stored index would be meaningless. Ids are stable across frames, so this is
    // safe. Depth cap covers any real tree; deeper ancestors are root containers
    // that are not queried for subtree hover (and are simply omitted if exceeded).
    static constexpr u32 kMaxHoverDepth = 64;
    UiId hotPath[kMaxHoverDepth] {}; // ancestor ids of hotId, nearest-first.
    u32 hotPathCount { 0 };
    UiId activeId { kNullId }; // element holding the press.
    UiId focusedId { kNullId }; // element with keyboard focus.
    u32 caret { 0 }; // caret byte index within the focused text field.
    u32 selectAnchor { 0 }; // selection anchor; selection is [min(caret,anchor), max).
};

struct UiInitDesc {
    void* buffer { nullptr };
    u64 bufferBytes { 0 };
    u32 maxNodes { 0 };
    u32 maxCommands { 0 };
    u32 maxFloats { 64 }; // max floating roots per frame.
    u32 maxScrollStates { 64 }; // max distinct scroll containers tracked.
    UiBackend backend {};
    u32 idSeed { 0 };
};

struct Context {
    Arena arena {}; // whole user buffer.
    Arena transient {}; // sub-arena, reset each frame (string copies, scratch).

    UiNode* nodes { nullptr };
    u32 nodeCount { 0 };
    u32 nodeCap { 0 };

    u32* openStack { nullptr };
    u32 openDepth { 0 };
    u32 openCap { 0 };

    // Floating roots collected this frame, in open order (ascending layer).
    u32* floatRoots { nullptr };
    u32 floatCount { 0 };
    u32 floatCap { 0 };
    u16 floatLayer { 0 }; // running layer counter; reset each frame.

    // Persistent scroll offsets (NOT reset each frame), keyed by element id.
    ScrollState* scrollStates { nullptr };
    u32 scrollCount { 0 };
    u32 scrollCap { 0 };

    RenderCommandBuffer commands {};

    InputState input {};
    UiBackend backend {};
    u32 idSeed { 0 };
    Vec2 rootSize {};
    bool overflowed { false };

    UI_API bool Init(const UiInitDesc& desc);
};

// Lifecycle / current-context management.
UI_API void SetCurrent(Context* ctx);
UI_API Context* Current();

UI_API void BeginFrame(Vec2 rootSize, const PointerState& pointer, const KeyboardState& keyboard = {});
UI_API void EndFrame(); // solve -> resolve input -> emit -> dispatch.

// Element primitives (used by Components; rarely called directly by end users).
// Returns the node index, or kNullIndex on overflow (subtree is then skipped).
UI_API u32 OpenElement(const LayoutConfig& cfg, UiId id);
UI_API void ConfigureText(u32 node, const char* text, u32 len, u16 fontId, u16 fontSize, UiColor color, bool wrap = false);
UI_API void ConfigureStyledText(u32 node, const char* text, u32 len, u16 fontId, u16 fontSize, UiColor color, const TextStyleRun* runs, u32 runCount);
UI_API void ConfigureCaret(u32 node, s32 caretByte); // show an editing caret at a byte index.
UI_API void ConfigureSelection(u32 node, u32 selStart, u32 selEnd); // highlight [selStart, selEnd).
UI_API void ConfigureIcon(u32 node, s32 iconId, UiColor tint);
UI_API void ConfigureCustom(u32 node, CustomDrawFn draw, void* user); // custom draw region (e.g. 3D viewport).
UI_API void CloseElement();

// Allocate frame-scoped scratch from the transient arena (valid until the next
// BeginFrame). Returns nullptr on overflow. Used e.g. for parser style runs.
UI_API void* AllocFrame(u64 bytes, u64 align = 16);
template <typename T>
inline T* AllocFrameArray(u32 count) { return static_cast<T*>(AllocFrame(static_cast<u64>(sizeof(T)) * count, alignof(T))); }

// Hover/active queries for a given id (read last frame's resolved hit-test).
// IsHovered is EXACT: true only when the pointer is directly over `id` (the single
// resolved hot node). IsHoverWithin is SUBTREE: true when the hot node is `id` or a
// structural descendant of it — use it for a container that should stay "hovered"
// while the pointer is over one of its own child controls (e.g. a list row that
// reveals a delete button), so the child's presence never steals the row's hover.
UI_API bool IsHovered(UiId id);
UI_API bool IsHoverWithin(UiId id);
UI_API bool IsClicked(UiId id);

// Keyboard focus.
UI_API bool IsFocused(UiId id);
UI_API void SetFocus(UiId id);
UI_API void ClearFocus();
UI_API const KeyboardState& Keyboard();

// Caret position (byte index) within the focused text field. Reset to "end" when
// focus moves to a different element (text components clamp it to their length).
UI_API u32 CaretPos();
UI_API void SetCaretPos(u32 byteIndex);

// Selection anchor (the fixed end of a selection; the caret is the moving end).
UI_API u32 SelectAnchorPos();
UI_API void SetSelectAnchorPos(u32 byteIndex);

// Clipboard (delegates to the bound backend).
UI_API void SetClipboard(const char* text);
UI_API const char* GetClipboard();

// Map a point (window coords) to a byte index in a text box (for click-to-position
// / drag-select). `textRect` is the text's box; `caret` lets single-line fields
// account for their horizontal scroll; `wrap` selects multi-line mapping.
UI_API u32 CaretIndexAt(const char* text, u32 len, Rect textRect, u16 fontId, u16 fontSize, s32 caret, Vec2 point, bool wrap);

// The bound backend's theme colors (used by the prebuilt components).
UI_API const ColorScheme& Colors();

// Find or create the persistent scroll state for an id (used by the solver and
// scrolling components). Returns nullptr only if the table is full.
UI_API ScrollState* AcquireScrollState(Context& ctx, UiId id);

// Drop ALL persistent per-id scroll offsets. Called by the host after an App.dll
// hot-reload: element ids can churn across a reload (until every element uses
// content-stable NameIds), and the scroll table never evicts on its own.
UI_API void ClearScrollStates(Context& ctx);

} // namespace Ui
