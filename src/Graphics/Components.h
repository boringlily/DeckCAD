#pragma once
#include "Graphics.h"

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

inline Clay_BoundingBox GetBoundingBox(Clay_ElementId& clayId)
{
    auto clayData = Clay_GetElementData(clayId);
    return clayData.boundingBox;
}

inline Clay_Dimensions GetDimensions(Clay_ElementId& clayId)
{
    auto boundingBox = GetBoundingBox(clayId);
    return Clay_Dimensions { boundingBox.width, boundingBox.height };
}

/// @brief Check if the size of the current component has changed.
/// @param previousDimension Pass in a statefull reference for what the last size was, value is updated if size did change.
/// @return true if the previous dimension is different from the new one.
inline bool CheckDimensionChanged(Clay_ElementId& clayId, Clay_Dimensions& previousDimension)
{
    Clay_Dimensions newSize = GetDimensions(clayId);
    bool changed { previousDimension.width != newSize.width || previousDimension.height != newSize.height };
    if (changed) {
        previousDimension = newSize;
    }
    return changed;
}

#ifdef __cplusplus
extern "C" {
#endif

GRAPHICS_API
Clay_CustomElementConfig ClayCustom_TextureRenderConfig(RenderTexture& render_texture);

GRAPHICS_API
void DrawIconElement(IconId iconId, StyleColor color);

#ifdef __cplusplus
}
#endif
