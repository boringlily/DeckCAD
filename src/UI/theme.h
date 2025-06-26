#pragma once
#include "DumbTypes.h"
#include "clay.h"
#include "raylib.h"

namespace UI
{

struct Color{
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;

    Clay_Color ToClayColor();
    Clay_Color ToRaylibColor();
};

struct ColorTheme
{
    Color BgDark;
    Color BgBase;
    Color BgLight;

    Color TextBase;
    Color TextMuted;

    Color BorderBase;
    Color BorderMuted;

    Color AccentPrimary;
    Color AccentSecondary;

    Color AlertError;
    Color AlertWarning;
    Color AlertSuccess;
    Color AlertInfo;
};

};
