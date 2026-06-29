#include "Layout.h"

namespace Ui {
namespace {

    constexpr f32 kScrollSpeed = 30.0f;

    f32 Clamp(f32 v, f32 min, f32 max)
    {
        if (v < min) {
            v = min;
        }
        if (max > 0.0f && v > max) {
            v = max;
        }
        return v;
    }

    bool Contains(const Rect& r, Vec2 p)
    {
        return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
    }

    bool Horizontal(const UiNode& n) { return n.cfg.direction == Direction::LeftToRight; }

    // Axis accessors: "main" follows the layout direction, "cross" is perpendicular.
    f32 MainOf(const UiNode& n, Vec2 v) { return Horizontal(n) ? v.x : v.y; }
    f32 CrossOf(const UiNode& n, Vec2 v) { return Horizontal(n) ? v.y : v.x; }

    const SizeAxis& MainAxis(const UiNode& n) { return Horizontal(n) ? n.cfg.sizing.w : n.cfg.sizing.h; }
    const SizeAxis& CrossAxis(const UiNode& n) { return Horizontal(n) ? n.cfg.sizing.h : n.cfg.sizing.w; }

    f32 PadMain(const UiNode& n)
    {
        const Edges& p = n.cfg.padding;
        return Horizontal(n) ? p.left + p.right : p.top + p.bottom;
    }
    f32 PadCross(const UiNode& n)
    {
        const Edges& p = n.cfg.padding;
        return Horizontal(n) ? p.top + p.bottom : p.left + p.right;
    }

    Vec2 FromMainCross(const UiNode& n, f32 main, f32 cross)
    {
        return Horizontal(n) ? Vec2 { main, cross } : Vec2 { cross, main };
    }

    f32 ResolveFit(const SizeAxis& axis, f32 content)
    {
        switch (axis.kind) {
        case SizeKind::Fixed:
            return Clamp(axis.value, axis.min, axis.max);
        case SizeKind::Fit:
        case SizeKind::Grow:
        case SizeKind::Percent:
        default:
            return Clamp(content, axis.min, axis.max);
        }
    }

    // Pass 1: bottom-up intrinsic sizing. Fixed resolves here; Grow/Percent take
    // their content size as a minimum and finalize in pass 2.
    void MeasureFit(Context& ctx, u32 index)
    {
        UiNode& n = ctx.nodes[index];
        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            MeasureFit(ctx, c);
        }

        f32 contentMain = 0;
        f32 contentCross = 0;

        if (n.textLen > 0 && ctx.backend.text.Measure) {
            TextMetrics m = ctx.backend.text.Measure(ctx.backend.text.user, n.textPtr, n.textLen, n.fontId, n.fontSize);
            contentMain = Horizontal(n) ? m.width : m.height;
            contentCross = Horizontal(n) ? m.height : m.width;
        } else {
            u32 count = 0;
            for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
                const UiNode& child = ctx.nodes[c];
                contentMain += MainOf(n, child.measured);
                f32 cc = CrossOf(n, child.measured);
                if (cc > contentCross) {
                    contentCross = cc;
                }
                count++;
            }
            if (count > 1) {
                contentMain += n.cfg.gap * static_cast<f32>(count - 1);
            }
        }

        f32 main = ResolveFit(MainAxis(n), contentMain + PadMain(n));
        f32 cross = ResolveFit(CrossAxis(n), contentCross + PadCross(n));
        n.measured = FromMainCross(n, main, cross);
        // Natural content extent, before clamping to a fixed/view size. A scroll
        // container's content may exceed its measured box; the difference is the
        // scroll range.
        n.contentSize = FromMainCross(n, contentMain + PadMain(n), contentCross + PadCross(n));
    }

    // Pass 2: top-down. Distribute free main-axis space to Grow children, resolve
    // Percent against the now-known parent inner size, and apply cross Stretch.
    void GrowShrink(Context& ctx, u32 index)
    {
        UiNode& n = ctx.nodes[index];
        if (n.firstChild == kNullIndex) {
            return;
        }

        f32 innerMain = MainOf(n, n.measured) - PadMain(n);
        f32 innerCross = CrossOf(n, n.measured) - PadCross(n);
        bool stretch = n.cfg.align == AlignCross::Stretch;

        // Resolve per-child cross sizing and tally used main space.
        f32 used = 0;
        u32 growCount = 0;
        u32 count = 0;
        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            UiNode& child = ctx.nodes[c];

            // Main-axis Percent resolves against parent inner main.
            const SizeAxis& childMain = (n.cfg.direction == Direction::LeftToRight) ? child.cfg.sizing.w : child.cfg.sizing.h;
            const SizeAxis& childCross = (n.cfg.direction == Direction::LeftToRight) ? child.cfg.sizing.h : child.cfg.sizing.w;

            if (childMain.kind == SizeKind::Percent) {
                f32 v = Clamp(childMain.value * innerMain, childMain.min, childMain.max);
                child.measured = FromMainCross(n, v, CrossOf(n, child.measured));
            }
            // Cross-axis Percent / Stretch.
            f32 crossSize = CrossOf(n, child.measured);
            if (childCross.kind == SizeKind::Percent) {
                crossSize = Clamp(childCross.value * innerCross, childCross.min, childCross.max);
            } else if (stretch && childCross.kind != SizeKind::Fixed) {
                crossSize = Clamp(innerCross, childCross.min, childCross.max);
            }
            child.measured = FromMainCross(n, MainOf(n, child.measured), crossSize);

            used += MainOf(n, child.measured);
            if (childMain.kind == SizeKind::Grow) {
                growCount++;
            }
            count++;
        }
        if (count > 1) {
            used += n.cfg.gap * static_cast<f32>(count - 1);
        }

        // Distribute leftover main space equally among Grow children.
        f32 free = innerMain - used;
        if (free > 0.0f && growCount > 0) {
            f32 each = free / static_cast<f32>(growCount);
            for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
                UiNode& child = ctx.nodes[c];
                const SizeAxis& childMain = (n.cfg.direction == Direction::LeftToRight) ? child.cfg.sizing.w : child.cfg.sizing.h;
                if (childMain.kind == SizeKind::Grow) {
                    f32 m = Clamp(MainOf(n, child.measured) + each, childMain.min, childMain.max);
                    child.measured = FromMainCross(n, m, CrossOf(n, child.measured));
                }
            }
        }

        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            GrowShrink(ctx, c);
        }
    }

    // Pass 3: top-down. Convert sizes into absolute rects (justify=Start, gap,
    // padding, align Start/Center/End on the cross axis).
    void Position(Context& ctx, u32 index, Vec2 origin)
    {
        UiNode& n = ctx.nodes[index];
        n.rect = Rect { origin.x, origin.y, n.measured.x, n.measured.y };
        if (n.firstChild == kNullIndex) {
            return;
        }

        bool horizontal = Horizontal(n);
        f32 innerOriginMain = horizontal ? origin.x + n.cfg.padding.left : origin.y + n.cfg.padding.top;
        f32 innerOriginCross = horizontal ? origin.y + n.cfg.padding.top : origin.x + n.cfg.padding.left;
        f32 innerMain = MainOf(n, n.measured) - PadMain(n);
        f32 innerCross = CrossOf(n, n.measured) - PadCross(n);

        // Sum the children's main extent so justify can place the free space.
        f32 childrenMain = 0;
        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            childrenMain += MainOf(n, ctx.nodes[c].measured);
        }
        if (n.childCount > 1) {
            childrenMain += n.cfg.gap * static_cast<f32>(n.childCount - 1);
        }

        f32 freeMain = innerMain - childrenMain;
        f32 cursorMain = innerOriginMain;
        f32 betweenGap = n.cfg.gap;
        switch (n.cfg.justify) {
        case Justify::Center:
            cursorMain += freeMain * 0.5f;
            break;
        case Justify::End:
            cursorMain += freeMain;
            break;
        case Justify::SpaceBetween:
            if (n.childCount > 1 && freeMain > 0) {
                betweenGap += freeMain / static_cast<f32>(n.childCount - 1);
            }
            break;
        case Justify::Start:
        default:
            break;
        }

        // Scroll: shift content along the main axis by the persisted offset, which
        // is advanced by the wheel while the pointer is over the container and
        // clamped to the overflow range.
        if (n.cfg.scroll) {
            f32 viewMain = MainOf(n, n.measured);
            f32 contentMain = MainOf(n, n.contentSize);
            f32 range = contentMain - viewMain;
            if (range < 0) {
                range = 0;
            }
            ScrollState* ss = AcquireScrollState(ctx, n.id);
            f32 off = ss ? ss->offset : 0;
            if (range > 0 && Contains(n.rect, ctx.input.pointer.pos)) {
                off -= ctx.input.pointer.wheel.y * kScrollSpeed;
            }
            off = off < 0 ? 0 : (off > range ? range : off);
            if (ss) {
                ss->offset = off;
            }
            cursorMain -= off;
        }

        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            UiNode& child = ctx.nodes[c];
            f32 childMain = MainOf(n, child.measured);
            f32 childCross = CrossOf(n, child.measured);

            f32 crossOffset = 0;
            switch (n.cfg.align) {
            case AlignCross::Center:
                crossOffset = (innerCross - childCross) * 0.5f;
                break;
            case AlignCross::End:
                crossOffset = innerCross - childCross;
                break;
            case AlignCross::Start:
            case AlignCross::Stretch:
            default:
                crossOffset = 0;
                break;
            }

            Vec2 childOrigin = horizontal
                ? Vec2 { cursorMain, innerOriginCross + crossOffset }
                : Vec2 { innerOriginCross + crossOffset, cursorMain };
            Position(ctx, c, childOrigin);

            cursorMain += childMain + betweenGap;
        }
    }

    // Height-for-width pass: runs after widths are final. Wrapping text now knows
    // its width, so its height is re-measured; Fit-height containers then recompute
    // their height from the (possibly taller) children. Bottom-up.
    void WrapPass(Context& ctx, u32 index)
    {
        UiNode& n = ctx.nodes[index];
        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            WrapPass(ctx, c);
        }

        if (n.textWrap && n.textLen > 0 && ctx.backend.text.MeasureWrapped) {
            TextMetrics m = ctx.backend.text.MeasureWrapped(ctx.backend.text.user,
                n.textPtr, n.textLen, n.fontId, n.fontSize, n.measured.x);
            n.measured.y = m.height;
            n.contentSize.y = m.height;
            return;
        }

        if (n.firstChild != kNullIndex) {
            bool tb = n.cfg.direction == Direction::TopToBottom;
            f32 contentH = 0;
            u32 count = 0;
            for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
                f32 cy = ctx.nodes[c].measured.y;
                if (tb) {
                    contentH += cy;
                } else if (cy > contentH) {
                    contentH = cy;
                }
                ++count;
            }
            if (tb && count > 1) {
                contentH += n.cfg.gap * static_cast<f32>(count - 1);
            }
            contentH += n.cfg.padding.top + n.cfg.padding.bottom;
            n.contentSize.y = contentH;
            if (n.cfg.sizing.h.kind == SizeKind::Fit) {
                n.measured.y = Clamp(contentH, n.cfg.sizing.h.min, n.cfg.sizing.h.max);
            }
        }
    }

    f32 ResolveFloatAxis(SizeKind kind, f32 value, f32 measuredAxis, f32 screenAxis)
    {
        switch (kind) {
        case SizeKind::Grow:
            return screenAxis;
        case SizeKind::Percent:
            return value * screenAxis;
        case SizeKind::Fit:
        case SizeKind::Fixed:
        default:
            return measuredAxis;
        }
    }

    // Solve one floating root: size it (Grow/Percent resolve against the window),
    // then place it relative to its anchor (flow parent) or the screen.
    void SolveFloat(Context& ctx, u32 index, Vec2 screen)
    {
        UiNode& n = ctx.nodes[index];
        MeasureFit(ctx, index);

        n.measured.x = ResolveFloatAxis(n.cfg.sizing.w.kind, n.cfg.sizing.w.value, n.measured.x, screen.x);
        n.measured.y = ResolveFloatAxis(n.cfg.sizing.h.kind, n.cfg.sizing.h.value, n.measured.y, screen.y);

        Rect anchor = (n.parent != kNullIndex)
            ? ctx.nodes[n.parent].rect
            : Rect { 0, 0, screen.x, screen.y };
        Vec2 size = n.measured;
        Vec2 o {};
        switch (n.cfg.floating.placement) {
        case FloatPlacement::BelowAnchor:
            o = { anchor.x, anchor.y + anchor.h };
            break;
        case FloatPlacement::AboveAnchor:
            o = { anchor.x, anchor.y - size.y };
            break;
        case FloatPlacement::RightOfAnchor:
            o = { anchor.x + anchor.w, anchor.y };
            break;
        case FloatPlacement::ScreenCenter:
            o = { (screen.x - size.x) * 0.5f, (screen.y - size.y) * 0.5f };
            break;
        case FloatPlacement::ScreenFill:
        case FloatPlacement::ScreenPosition:
        default:
            o = { 0, 0 };
            break;
        }
        o.x += n.cfg.floating.offset.x;
        o.y += n.cfg.floating.offset.y;

        if (n.cfg.floating.clampToScreen && n.cfg.floating.placement != FloatPlacement::ScreenFill) {
            f32 maxX = screen.x - size.x;
            f32 maxY = screen.y - size.y;
            if (o.x > maxX) {
                o.x = maxX;
            }
            if (o.y > maxY) {
                o.y = maxY;
            }
            if (o.x < 0) {
                o.x = 0;
            }
            if (o.y < 0) {
                o.y = 0;
            }
        }

        GrowShrink(ctx, index);
        WrapPass(ctx, index);
        Position(ctx, index, o);
    }

} // namespace

void Solve(Context& ctx)
{
    if (ctx.nodeCount == 0) {
        return;
    }
    MeasureFit(ctx, 0);
    GrowShrink(ctx, 0);
    WrapPass(ctx, 0);
    Position(ctx, 0, Vec2 { 0, 0 });

    // Floating roots are solved after the base tree so their anchors (flow
    // parents) already have final rects. Order is open order (ascending layer).
    for (u32 i = 0; i < ctx.floatCount; ++i) {
        SolveFloat(ctx, ctx.floatRoots[i], ctx.rootSize);
    }
}

} // namespace Ui
