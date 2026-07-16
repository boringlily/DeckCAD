#pragma once
#include "DTL.h"

struct Scene;

// Right-hand toolbox state.
//
// The value field lives here rather than in a static: Ui::InputLabel edits a
// caller-owned buffer that must survive the immediate-mode rebuild, and an App.dll
// hot-reload would wipe a static mid-gesture.
inline constexpr u32 kToolValueCap = 128;

struct Toolbox {
    u32 active_toolset { 0 };

    // The expression typed before applying a dimension.
    char value[kToolValueCap] {};
    u32 valueLen { 0 };

    void ClearValue()
    {
        value[0] = '\0';
        valueLen = 0;
    }
};
