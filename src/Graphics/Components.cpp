#pragma once
#include "Components.h"

enum ClayCustomCommands : uint32_t {
    DrawRenderTexture,
};

Texture& GetIcon(IconId iconId);

#ifdef __cplusplus
extern "C" {
#endif

GRAPHICS_API
Clay_CustomElementConfig ClayCustom_TextureRenderConfig(RenderTexture& render_texture)
{
    return Clay_CustomElementConfig {
        .customData = &render_texture,
        .customCommandId = DrawRenderTexture
    };
}

GRAPHICS_API
void DrawIconElement(IconId iconId, StyleColor color)
{
    CLAY({
        .layout = { .sizing = { .width = CLAY_SIZING_FIXED(24), .height = CLAY_SIZING_FIXED(24) } },
        .backgroundColor = color,
        .image = { .imageData = &GetIcon(iconId) },
    });
}

#ifdef __cplusplus
}
#endif