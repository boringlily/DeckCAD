#pragma once
#include <array>
#include <string>
#include "raylib.h"

    // const uint32_t FONT_ID_BODY_16 = 0;

    // Font fonts[1];
    // fonts[FONT_ID_BODY_16] = LoadFontEx("./assets/fonts/Roboto-Regular.ttf", 48, 0, 400);
    // SetTextureFilter(fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);


enum class FontId : uint32_t
{
    Body
};

class Font
{
public:
    Font()

    std::array<Font, 0>;
}