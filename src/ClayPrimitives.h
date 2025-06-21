#pragma once
#include "clay.h"
#include "raylib.h"

const Clay_Color COLOR_LIGHT_GREY = (Clay_Color) { 235, 235, 235, 255 };
const Clay_Color COLOR_RED = (Clay_Color) { 200, 100, 100, 255 };
const Clay_Color COLOR_GREEN = (Clay_Color) { 100, 200, 100, 255 };
const Clay_Color COLOR_BLUE = (Clay_Color) { 100, 100, 200, 255 };

static constexpr Clay_Sizing LAYOUT_EXPAND {
    .width = CLAY_SIZING_GROW(),
    .height = CLAY_SIZING_GROW()
};

static constexpr Clay_Sizing LAYOUT_EXPAND_MIN_MAX_WIDTH(float min, float max = 0)
{
    return {
        .width = CLAY_SIZING_GROW(min, max),
        .height = CLAY_SIZING_GROW()
    };
};

constexpr int WINDOW_MIN_WIDTH = 1024;
constexpr int WINDOW_MIN_HEIGHT = 800;