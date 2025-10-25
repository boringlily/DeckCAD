#pragma once
#include "Components.h"

enum ClayCustomCommands : uint32_t {
    DrawRenderTexture,
};

Texture& GetIcon(IconId icon_id);

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
void DrawIcon(IconId icon_id, StyleColor color)
{
    CLAY({
        .layout = { .sizing = { .width = CLAY_SIZING_FIXED(24), .height = CLAY_SIZING_FIXED(24) } },
        .backgroundColor = color,
        .image = { .imageData = &GetIcon(icon_id) },
    });
}

GRAPHICS_API
void DrawIconWithBg(IconId icon_id, StyleColor icon_color, StyleColor bg_color)
{
    CLAY({
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(32), .height = CLAY_SIZING_FIXED(32) },
            .childAlignment = ALIGN_CENTER,
        },
        .backgroundColor = bg_color,
        .cornerRadius = { 8u },
    })
    {
        DrawIcon(icon_id, icon_color);
    }
}

#ifdef __cplusplus
}
#endif