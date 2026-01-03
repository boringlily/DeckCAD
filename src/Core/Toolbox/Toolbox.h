#pragma once
#include "DumbTypes.h"

// Forward Declaration
struct Scene;
struct Toolbox {
    /// @brief Bit flags that represent which contexts a tool belongs to.
    enum Context : u8 {
        Solid = 0b01,
        Sketch = 0b10,
    };

    /// @brief ToolStatus is used by tool functions to notify the toolbox when to close the tool and return to toolset context.
    enum ToolStatus : u8 {
        active,
        done
    };

    using ToolFunctionPointer = void (*)(Scene& scene);
    Toolbox() {};

    Context context { Solid };
    u32 active_toolset { 0 };
    ToolFunctionPointer active_tool { nullptr };
    ToolStatus active_tool_status { ToolStatus::done };
};