#include "UiContext.h"

#include "Dispatch.h"
#include "Emit.h"
#include "Input.h"
#include "Layout.h"

namespace Ui {

namespace {
    Context* g_current = nullptr;
}

bool Context::Init(const UiInitDesc& desc)
{
    if (!desc.buffer || desc.bufferBytes == 0 || desc.maxNodes == 0 || desc.maxCommands == 0) {
        return false;
    }

    arena.Init(desc.buffer, desc.bufferBytes);
    backend = desc.backend;
    idSeed = desc.idSeed;

    // Carve the persistent region (never reset): node array, open-element stack,
    // and the render command buffer.
    nodes = arena.AllocArray<UiNode>(desc.maxNodes);
    nodeCap = desc.maxNodes;
    nodeCount = 0;

    openCap = desc.maxNodes; // worst case: a fully linear tree.
    openStack = arena.AllocArray<u32>(openCap);
    openDepth = 0;

    commands.cmds = arena.AllocArray<RenderCommand>(desc.maxCommands);
    commands.cap = desc.maxCommands;
    commands.count = 0;

    floatCap = desc.maxFloats > 0 ? desc.maxFloats : 1;
    floatRoots = arena.AllocArray<u32>(floatCap);
    floatCount = 0;

    scrollCap = desc.maxScrollStates > 0 ? desc.maxScrollStates : 1;
    scrollStates = arena.AllocArray<ScrollState>(scrollCap);
    scrollCount = 0;

    if (!nodes || !openStack || !commands.cmds || !floatRoots || !scrollStates) {
        return false; // buffer too small for the persistent region.
    }

    // The remainder of the buffer is the transient region (per-frame scratch).
    transient.Init(arena.base + arena.offset, arena.size - arena.offset);
    return true;
}

void SetCurrent(Context* ctx) { g_current = ctx; }
Context* Current() { return g_current; }

void BeginFrame(Vec2 rootSize, const PointerState& pointer, const KeyboardState& keyboard)
{
    Context* ctx = g_current;
    if (!ctx) {
        return;
    }

    ctx->transient.Reset();
    ctx->nodeCount = 0;
    ctx->openDepth = 0;
    ctx->floatCount = 0;
    ctx->floatLayer = 0;
    ctx->commands.Clear();
    ctx->overflowed = false;
    ctx->rootSize = rootSize;
    ctx->input.pointer = pointer;
    ctx->input.keyboard = keyboard;

    // A press outside any element clears keyboard focus (e.g. clicking empty
    // space dismisses a text field). Re-set by the focused element when hit.
    if (pointer.pressed) {
        ctx->input.focusedId = kNullId;
    }

    // Implicit root: fills the window, top-to-bottom, stretching its child across
    // the cross axis so a single Grow child fills the whole window.
    LayoutConfig rootCfg {};
    rootCfg.sizing = { Fixed(rootSize.x), Fixed(rootSize.y) };
    rootCfg.direction = Direction::TopToBottom;
    rootCfg.align = AlignCross::Stretch;
    OpenElement(rootCfg, HashId(ctx, 0, ctx->idSeed));
}

u32 OpenElement(const LayoutConfig& cfg, UiId id)
{
    Context* ctx = g_current;
    if (!ctx) {
        return kNullIndex;
    }
    if (ctx->nodeCount >= ctx->nodeCap) {
        ctx->overflowed = true;
        return kNullIndex;
    }

    u32 index = ctx->nodeCount++;
    UiNode& node = ctx->nodes[index];
    node = UiNode {};
    node.id = id;
    node.cfg = cfg;

    u32 parentIndex = ctx->openDepth > 0 ? ctx->openStack[ctx->openDepth - 1] : kNullIndex;
    node.parent = parentIndex;

    if (cfg.floating.enabled) {
        // Floating elements are lifted out of normal flow: they keep a parent
        // reference (for anchoring) but are NOT linked as a flow child, so the
        // base passes never size or position them. They get their own layer and
        // are solved/emitted separately. Nested floats simply stack higher.
        node.layer = ++ctx->floatLayer;
        if (ctx->floatCount < ctx->floatCap) {
            ctx->floatRoots[ctx->floatCount++] = index;
        } else {
            ctx->overflowed = true;
        }
    } else if (parentIndex != kNullIndex) {
        node.layer = ctx->nodes[parentIndex].layer; // inherit compositing layer.
        UiNode& parent = ctx->nodes[parentIndex];
        if (parent.firstChild == kNullIndex) {
            parent.firstChild = index;
        } else {
            ctx->nodes[parent.lastChild].nextSibling = index;
        }
        parent.lastChild = index;
        parent.childCount++;
    }

    if (ctx->openDepth < ctx->openCap) {
        ctx->openStack[ctx->openDepth++] = index;
    }
    return index;
}

void ConfigureText(u32 node, const char* text, u32 len, u16 fontId, u16 fontSize, UiColor color, bool wrap)
{
    Context* ctx = g_current;
    if (!ctx || node == kNullIndex || node >= ctx->nodeCount) {
        return;
    }
    UiNode& n = ctx->nodes[node];
    n.textPtr = text;
    n.textLen = len;
    n.fontId = fontId;
    n.fontSize = fontSize;
    n.textColor = color;
    n.textWrap = wrap;
}

void ConfigureStyledText(u32 node, const char* text, u32 len, u16 fontId, u16 fontSize, UiColor color, const TextStyleRun* runs, u32 runCount)
{
    Context* ctx = g_current;
    if (!ctx || node == kNullIndex || node >= ctx->nodeCount) {
        return;
    }
    UiNode& n = ctx->nodes[node];
    n.textPtr = text;
    n.textLen = len;
    n.fontId = fontId;
    n.fontSize = fontSize;
    n.textColor = color;
    n.styleRuns = runs;
    n.styleRunCount = runCount;
}

void ConfigureCaret(u32 node, s32 caretByte)
{
    Context* ctx = g_current;
    if (!ctx || node == kNullIndex || node >= ctx->nodeCount) {
        return;
    }
    ctx->nodes[node].caretByte = caretByte;
}

void* AllocFrame(u64 bytes, u64 align)
{
    Context* ctx = g_current;
    return ctx ? ctx->transient.Alloc(bytes, align) : nullptr;
}

void ConfigureIcon(u32 node, s32 iconId, UiColor tint)
{
    Context* ctx = g_current;
    if (!ctx || node == kNullIndex || node >= ctx->nodeCount) {
        return;
    }
    UiNode& n = ctx->nodes[node];
    n.iconId = iconId;
    n.iconTint = tint;
}

void ConfigureSelection(u32 node, u32 selStart, u32 selEnd)
{
    Context* ctx = g_current;
    if (!ctx || node == kNullIndex || node >= ctx->nodeCount) {
        return;
    }
    UiNode& n = ctx->nodes[node];
    n.selStart = selStart;
    n.selEnd = selEnd;
}

void ConfigureCustom(u32 node, CustomDrawFn draw, void* user)
{
    Context* ctx = g_current;
    if (!ctx || node == kNullIndex || node >= ctx->nodeCount) {
        return;
    }
    UiNode& n = ctx->nodes[node];
    n.customDraw = draw;
    n.customUser = user;
}

void CloseElement()
{
    Context* ctx = g_current;
    if (!ctx || ctx->openDepth == 0) {
        return;
    }
    ctx->openDepth--;
}

void EndFrame()
{
    Context* ctx = g_current;
    if (!ctx || ctx->nodeCount == 0) {
        return;
    }

    Solve(*ctx);
    ResolveInput(*ctx);
    Emit(*ctx);
    Dispatch(ctx->commands, ctx->backend);
}

bool IsHovered(UiId id)
{
    Context* ctx = g_current;
    return ctx && id != kNullId && ctx->input.hotId == id;
}

bool IsClicked(UiId id)
{
    Context* ctx = g_current;
    return ctx && id != kNullId && ctx->input.hotId == id && ctx->input.pointer.pressed;
}

bool IsFocused(UiId id)
{
    Context* ctx = g_current;
    return ctx && id != kNullId && ctx->input.focusedId == id;
}

void SetFocus(UiId id)
{
    if (Context* ctx = g_current) {
        if (ctx->input.focusedId != id) {
            ctx->input.caret = u32_max; // sentinel: clamp to end on first edit/render.
            ctx->input.selectAnchor = u32_max;
        }
        ctx->input.focusedId = id;
    }
}

u32 CaretPos()
{
    Context* ctx = g_current;
    return ctx ? ctx->input.caret : 0;
}

void SetCaretPos(u32 byteIndex)
{
    if (Context* ctx = g_current) {
        ctx->input.caret = byteIndex;
    }
}

u32 SelectAnchorPos()
{
    Context* ctx = g_current;
    return ctx ? ctx->input.selectAnchor : 0;
}

void SetSelectAnchorPos(u32 byteIndex)
{
    if (Context* ctx = g_current) {
        ctx->input.selectAnchor = byteIndex;
    }
}

void SetClipboard(const char* text)
{
    Context* ctx = g_current;
    if (ctx && ctx->backend.clipboard.Set) {
        ctx->backend.clipboard.Set(ctx->backend.clipboard.user, text);
    }
}

const char* GetClipboard()
{
    Context* ctx = g_current;
    if (ctx && ctx->backend.clipboard.Get) {
        return ctx->backend.clipboard.Get(ctx->backend.clipboard.user);
    }
    return nullptr;
}

u32 CaretIndexAt(const char* text, u32 len, Rect textRect, u16 fontId, u16 fontSize, s32 caret, Vec2 point, bool wrap)
{
    Context* ctx = g_current;
    if (ctx && ctx->backend.text.CaretIndexAt) {
        return ctx->backend.text.CaretIndexAt(ctx->backend.text.user, text, len, textRect, fontId, fontSize, caret, point, wrap);
    }
    return len;
}

void ClearFocus()
{
    if (Context* ctx = g_current) {
        ctx->input.focusedId = kNullId;
    }
}

const KeyboardState& Keyboard()
{
    static KeyboardState empty {};
    Context* ctx = g_current;
    return ctx ? ctx->input.keyboard : empty;
}

const ColorScheme& Colors()
{
    static ColorScheme fallback {};
    Context* ctx = g_current;
    return ctx ? ctx->backend.colors : fallback;
}

ScrollState* AcquireScrollState(Context& ctx, UiId id)
{
    for (u32 i = 0; i < ctx.scrollCount; ++i) {
        if (ctx.scrollStates[i].id == id) {
            return &ctx.scrollStates[i];
        }
    }
    if (ctx.scrollCount < ctx.scrollCap) {
        ScrollState* s = &ctx.scrollStates[ctx.scrollCount++];
        *s = ScrollState {};
        s->id = id;
        return s;
    }
    return nullptr;
}

} // namespace Ui
