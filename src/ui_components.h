#pragma once
#include "clay.h"
#include "raylib.h"

const Clay_Color COLOR_LIGHT = (Clay_Color) { 224, 215, 210, 255 };
const Clay_Color COLOR_RED = (Clay_Color) { 200, 100, 100, 255 };
const Clay_Color COLOR_GREEN = (Clay_Color) { 100, 200, 100, 255 };
const Clay_Color COLOR_BLUE = (Clay_Color) { 100, 100, 200, 255 };

inline Clay_Dimensions get_screen_size()
{
    return Clay_Dimensions { .width = static_cast<float>(GetRenderWidth()),
        .height = static_cast<float>(GetRenderHeight()) };
}

inline bool HasScreenSizeChanged()
{
    static Clay_Dimensions last_screen_size {};
    Clay_Dimensions screen_size = get_screen_size();
    bool changed = (last_screen_size.width != screen_size.width || last_screen_size.height != screen_size.height);
    last_screen_size = screen_size;
    return changed;
}

inline Clay_Sizing layoutExpand = {
    .width = CLAY_SIZING_GROW(),
    .height = CLAY_SIZING_GROW()
};

inline Clay_Sizing layoutExpandMinWidth(float min, float max = 0)
{
    return {
        .width = CLAY_SIZING_GROW(min, max),
        .height = CLAY_SIZING_GROW()
    };
};

constexpr int WindowMinWidth = 1024;
constexpr int WindowMinHeight = 800;