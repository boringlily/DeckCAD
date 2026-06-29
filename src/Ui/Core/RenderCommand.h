#pragma once
#include "DTL.h"
#include "UiTypes.h"
#include <type_traits>

// The solver produces a flat list of these; the backend dispatch consumes them.
// Kept POD and stored in a fixed-size buffer carved from the persistent arena.
namespace Ui {

enum class CmdType : u8 { Rect,
    Text,
    Image,
    Icon,
    Border,
    Custom,
    ScissorStart,
    ScissorEnd };

struct RenderCommand {
    CmdType type { CmdType::Rect };
    Rect box {};
    UiColor color {};
    f32 cornerRadius { 0 };

    // Text payload (valid for CmdType::Text).
    const char* text { nullptr };
    u32 textLen { 0 };
    u16 fontId { 0 };
    u16 fontSize { 16 };
    bool textWrap { false };
    const TextStyleRun* styleRuns { nullptr };
    u32 styleRunCount { 0 };
    s32 caretByte { -1 };
    u32 selStart { 0 }; // selection highlight range [selStart, selEnd).
    u32 selEnd { 0 };

    // Image payload (valid for CmdType::Image).
    void* imageHandle { nullptr };

    // Icon payload (valid for CmdType::Icon); color is the tint.
    s32 iconId { -1 };

    // Border payload (valid for CmdType::Border); color is the border color.
    Edges borderWidth {};

    // Custom payload (valid for CmdType::Custom).
    CustomDrawFn customDraw { nullptr };
    void* customUser { nullptr };
};

static_assert(std::is_trivially_copyable_v<RenderCommand>, "RenderCommand must stay POD for buffer storage");

struct RenderCommandBuffer {
    RenderCommand* cmds { nullptr };
    u32 count { 0 };
    u32 cap { 0 };

    bool Push(const RenderCommand& c)
    {
        if (count >= cap) {
            return false;
        }
        cmds[count++] = c;
        return true;
    }

    void Clear() { count = 0; }
};

} // namespace Ui
