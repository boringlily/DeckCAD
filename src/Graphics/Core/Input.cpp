#include "Input.h"

namespace Ui {
namespace {

    bool Contains(const Rect& r, Vec2 p)
    {
        return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
    }

    // A node is clipped out for hit-testing if the pointer lies outside any
    // clipping/scrolling ancestor (mirrors the scissor applied when drawing).
    bool ClippedOut(const Context& ctx, const UiNode& n, Vec2 p)
    {
        for (u32 a = n.parent; a != kNullIndex; a = ctx.nodes[a].parent) {
            const UiNode& anc = ctx.nodes[a];
            if ((anc.cfg.clip || anc.cfg.scroll) && !Contains(anc.rect, p)) {
                return true;
            }
        }
        return false;
    }

} // namespace

// Resolve the hovered element after layout. The winner is the containing node
// with the greatest (layer, index): a higher layer (floating) beats the base
// tree, and within a layer the later-built (topmost) node wins. Nodes are
// appended parent-before-child, so index order already encodes draw order.
void ResolveInput(Context& ctx)
{
    Vec2 p = ctx.input.pointer.pos;
    UiId hot = kNullId;
    u16 hotLayer = 0;
    Rect hotRect {};
    u32 hotIndex = kNullIndex;
    bool found = false;

    for (u32 i = 0; i < ctx.nodeCount; ++i) {
        const UiNode& n = ctx.nodes[i];
        if (!n.cfg.hitTestable || !Contains(n.rect, p) || ClippedOut(ctx, n, p)) {
            continue;
        }
        // (layer, index) ordering: i increases monotonically, so a later node at
        // the same-or-higher layer always replaces the current pick.
        if (!found || n.layer >= hotLayer) {
            hot = n.id;
            hotLayer = n.layer;
            hotRect = n.rect;
            hotIndex = i;
            found = true;
        }
    }
    ctx.input.hotId = hot;
    ctx.input.hotLayer = hotLayer;
    ctx.input.hotRect = hotRect;

    // Snapshot the hot node's ancestor ids now, while the tree is fully built, so
    // IsHoverWithin can answer "is `id` an ancestor of the hovered element?" next
    // frame without touching the node array (which is rebuilt each frame). parent
    // index is strictly less than its child's, so this walk always terminates.
    ctx.input.hotPathCount = 0;
    if (hotIndex != kNullIndex) {
        for (u32 a = ctx.nodes[hotIndex].parent;
             a != kNullIndex && ctx.input.hotPathCount < InputState::kMaxHoverDepth;
             a = ctx.nodes[a].parent) {
            ctx.input.hotPath[ctx.input.hotPathCount++] = ctx.nodes[a].id;
        }
    }

    if (ctx.input.pointer.pressed) {
        ctx.input.activeId = hot;
    } else if (ctx.input.pointer.released) {
        ctx.input.activeId = kNullId;
    }
}

} // namespace Ui
