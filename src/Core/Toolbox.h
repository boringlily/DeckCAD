#pragma once
#include "Types.h"

namespace Core {

struct FrameContext;

/// Tool selection state for one scene.
struct Toolbox {
    /// Bit flags for which modelling contexts a tool belongs to.
    enum ContextFlags : u8 {
        Solid = 0b01,
        Sketch = 0b10,
    };

    enum class ToolStatus : u8 {
        Idle,
        Active,
    };

    /// A tool draws its own UI each frame and closes itself by calling dismissTool().
    using ToolFunction = void (*)(FrameContext&);

    ContextFlags context { Solid };
    u32 active_toolset { 0 };
    ToolFunction active_tool { nullptr };
    ToolStatus status { ToolStatus::Idle };

    void activateTool(ToolFunction tool)
    {
        active_tool = tool;
        status = ToolStatus::Active;
    }

    void dismissTool()
    {
        active_tool = nullptr;
        status = ToolStatus::Idle;
    }

    bool hasActiveTool() const { return status == ToolStatus::Active && active_tool != nullptr; }
};

} // namespace Core
