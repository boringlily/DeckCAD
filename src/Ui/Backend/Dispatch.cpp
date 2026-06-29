#include "Dispatch.h"

namespace Ui {

void Dispatch(const RenderCommandBuffer& buffer, const UiBackend& backend)
{
    // Track the active scissor so it can be suspended around a custom callback
    // (which may switch render targets - a stale scissor would clip its output).
    Rect scissorRect {};
    bool scissorOn = false;

    for (u32 i = 0; i < buffer.count; ++i) {
        const RenderCommand& c = buffer.cmds[i];
        switch (c.type) {
        case CmdType::Rect:
            if (backend.draw.FillRect) {
                backend.draw.FillRect(backend.draw.user, c.box, c.color, c.cornerRadius);
            }
            break;
        case CmdType::Text:
            // Wrapped (multi-line) takes priority and carries its own caret; the
            // single-line styled path handles highlight runs + caret for one line.
            if (c.textWrap && backend.text.DrawWrapped) {
                backend.text.DrawWrapped(backend.text.user, c.text, c.textLen, c.box, c.fontId, c.fontSize, c.color, c.caretByte, c.selStart, c.selEnd, backend.colors.selection);
            } else if (((c.styleRuns && c.styleRunCount > 0) || c.caretByte >= 0 || c.selEnd > c.selStart) && backend.text.DrawStyled) {
                backend.text.DrawStyled(backend.text.user, c.text, c.textLen, c.box, c.fontId, c.fontSize, c.color, c.styleRuns, c.styleRunCount, c.caretByte, c.selStart, c.selEnd, backend.colors.selection);
            } else if (backend.text.Draw) {
                backend.text.Draw(backend.text.user, c.text, c.textLen,
                    Vec2 { c.box.x, c.box.y }, c.fontId, c.fontSize, c.color);
            }
            break;
        case CmdType::Image:
            if (backend.image.Draw) {
                backend.image.Draw(backend.image.user, c.imageHandle, c.box, c.color);
            }
            break;
        case CmdType::Icon:
            if (backend.icon.Draw) {
                backend.icon.Draw(backend.icon.user, static_cast<u32>(c.iconId), c.box, c.color);
            }
            break;
        case CmdType::Border:
            if (backend.draw.Border) {
                backend.draw.Border(backend.draw.user, c.box, c.borderWidth, c.color, c.cornerRadius);
            }
            break;
        case CmdType::Custom:
            // User-supplied draw (e.g. a 3D viewport). The active scissor (if any)
            // is suspended around it so a callback that switches render targets is
            // not clipped by a stale window-space scissor; it is restored after.
            if (c.customDraw) {
                if (scissorOn && backend.draw.ScissorEnd) {
                    backend.draw.ScissorEnd(backend.draw.user);
                }
                c.customDraw(c.customUser, c.box);
                if (scissorOn && backend.draw.ScissorStart) {
                    backend.draw.ScissorStart(backend.draw.user, scissorRect);
                }
            }
            break;
        case CmdType::ScissorStart:
            if (backend.draw.ScissorStart) {
                backend.draw.ScissorStart(backend.draw.user, c.box);
            }
            scissorRect = c.box;
            scissorOn = true;
            break;
        case CmdType::ScissorEnd:
            if (backend.draw.ScissorEnd) {
                backend.draw.ScissorEnd(backend.draw.user);
            }
            scissorOn = false;
            break;
        }
    }
}

} // namespace Ui
