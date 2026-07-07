#pragma once
#include "DTL.h"
#include "raylib.h"

namespace Inputs {
enum class Mouse : u32 {
    Left = MOUSE_BUTTON_LEFT,
    Middle = MOUSE_BUTTON_MIDDLE,
    Right = MOUSE_BUTTON_RIGHT,
};

inline bool MousePressed(Mouse button)
{
    return IsMouseButtonPressed(static_cast<s32>(button));
}

inline bool MouseHeld(Mouse button)
{
    return IsMouseButtonDown(static_cast<s32>(button));
}
}
