#include "Emit.h"

namespace Ui {
namespace {

    void EmitNode(Context& ctx, u32 index)
    {
        const UiNode& n = ctx.nodes[index];

        // Background rectangle (skipped when fully transparent).
        if (n.cfg.background.a > 0) {
            RenderCommand cmd {};
            cmd.type = CmdType::Rect;
            cmd.box = n.rect;
            cmd.color = n.cfg.background;
            cmd.cornerRadius = n.cfg.cornerRadius;
            ctx.commands.Push(cmd);
        }

        // Border on top of the background.
        if (n.cfg.borderColor.a > 0 && (n.cfg.border.left > 0 || n.cfg.border.right > 0 || n.cfg.border.top > 0 || n.cfg.border.bottom > 0)) {
            RenderCommand cmd {};
            cmd.type = CmdType::Border;
            cmd.box = n.rect;
            cmd.color = n.cfg.borderColor;
            cmd.borderWidth = n.cfg.border;
            cmd.cornerRadius = n.cfg.cornerRadius;
            ctx.commands.Push(cmd);
        }

        // Custom draw region (e.g. a 3D viewport) over the background, under any
        // icon/text on the same node.
        if (n.customDraw) {
            RenderCommand cmd {};
            cmd.type = CmdType::Custom;
            cmd.box = n.rect;
            cmd.customDraw = n.customDraw;
            cmd.customUser = n.customUser;
            ctx.commands.Push(cmd);
        }

        // Icon fills the node's content box.
        if (n.iconId >= 0) {
            RenderCommand cmd {};
            cmd.type = CmdType::Icon;
            cmd.box = n.rect;
            cmd.color = n.iconTint;
            cmd.iconId = n.iconId;
            ctx.commands.Push(cmd);
        }

        // Text on top of the background.
        if (n.textLen > 0) {
            RenderCommand cmd {};
            cmd.type = CmdType::Text;
            cmd.box = n.rect;
            cmd.color = n.textColor;
            cmd.text = n.textPtr;
            cmd.textLen = n.textLen;
            cmd.fontId = n.fontId;
            cmd.fontSize = n.fontSize;
            cmd.textWrap = n.textWrap;
            cmd.styleRuns = n.styleRuns;
            cmd.styleRunCount = n.styleRunCount;
            cmd.caretByte = n.caretByte;
            cmd.selStart = n.selStart;
            cmd.selEnd = n.selEnd;
            ctx.commands.Push(cmd);
        }

        // Children render above the parent, optionally clipped to this box.
        bool clip = n.cfg.clip || n.cfg.scroll;
        if (clip) {
            RenderCommand cmd {};
            cmd.type = CmdType::ScissorStart;
            cmd.box = n.rect;
            ctx.commands.Push(cmd);
        }
        for (u32 c = n.firstChild; c != kNullIndex; c = ctx.nodes[c].nextSibling) {
            EmitNode(ctx, c);
        }
        if (clip) {
            RenderCommand cmd {};
            cmd.type = CmdType::ScissorEnd;
            ctx.commands.Push(cmd);
        }
    }

} // namespace

void Emit(Context& ctx)
{
    if (ctx.nodeCount == 0) {
        return;
    }
    // Base tree first (floating roots are not flow children, so they are skipped
    // here), then each floating root on top in open order (ascending layer).
    EmitNode(ctx, 0);
    for (u32 i = 0; i < ctx.floatCount; ++i) {
        EmitNode(ctx, ctx.floatRoots[i]);
    }
}

} // namespace Ui
