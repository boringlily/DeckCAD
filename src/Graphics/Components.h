#pragma once
#include "Graphics.h"

static constexpr Clay_Sizing LAYOUT_EXPAND {
    .width = CLAY_SIZING_GROW(),
    .height = CLAY_SIZING_GROW()
};

static constexpr Clay_ChildAlignment ALIGN_CENTER {
    .x = CLAY_ALIGN_X_CENTER,
    .y = CLAY_ALIGN_Y_CENTER
};

static constexpr Clay_Sizing LAYOUT_EXPAND_MIN_MAX_WIDTH(float min, float max = 0)
{
    return {
        .width = CLAY_SIZING_GROW(min, max),
        .height = CLAY_SIZING_GROW()
    };
};

static constexpr Clay_Padding LAYOUT_PADDING_SIDES_AND_TOP(u16 side, u16 top)
{
    return { .left = side, .right = side, .top = top, .bottom = top };
};

// padding left right is 8u, padding top, bottom = 4u
static constexpr Clay_Padding LAYOUT_PADDING_RECTANGLE_MEDIUM { LAYOUT_PADDING_SIDES_AND_TOP(8u, 4u) };

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
void DrawIcon(IconId iconId, StyleColor color);

GRAPHICS_API
void DrawIconWithBg(IconId iconId, StyleColor icon_color, StyleColor bg_color);

#ifdef __cplusplus
}
#endif
