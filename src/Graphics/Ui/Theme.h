#pragma once
#include "DeckMath.h"
#include "Types.h"

namespace Ui {

/// Colour token for the UI. Authored as 0-255 sRGB bytes to keep the palette
/// readable, converted to the 0-1 floats ImGui wants at use sites.
struct Color {
    u8 r { 255 }, g { 255 }, b { 255 }, a { 255 };

    constexpr Color() = default;
    constexpr Color(u8 red, u8 green, u8 blue, u8 alpha = 255)
        : r { red }
        , g { green }
        , b { blue }
        , a { alpha }
    {
    }
    /// Grey shorthand.
    constexpr explicit Color(u8 value)
        : r { value }
        , g { value }
        , b { value }
        , a { 255 }
    {
    }

    constexpr DeckMath::Vector4 toVector4() const
    {
        return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    }

    constexpr Color withAlpha(u8 alpha) const { return Color { r, g, b, alpha }; }
};

/// Semantic palette. Carried over from the original Clay theme so the app keeps
/// its identity through the renderer change.
struct Palette {
    Color background_dark;
    Color background_base;
    Color background_light;
    Color text_base;
    Color text_muted;
    Color border_base;
    Color border_muted;
    Color accent_primary;
    Color accent_secondary;
    Color alert_danger;
    Color alert_warning;
    Color alert_success;
    Color alert_info;
    Color viewport_background;
};

inline constexpr Palette LIGHT_THEME {
    .background_dark = Color { 210 },
    .background_base = Color { 239 },
    .background_light = Color { 255 },
    .text_base = Color { 13 },
    .text_muted = Color { 128 },
    .border_base = Color { 102 },
    .border_muted = Color { 153 },
    .accent_primary = Color { 172, 153, 255 },
    .accent_secondary = Color { 151, 71, 255 },
    .alert_danger = Color { 255, 127, 127 },
    .alert_warning = Color { 255, 225, 127 },
    .alert_success = Color { 149, 255, 127 },
    .alert_info = Color { 127, 212, 255 },
    .viewport_background = Color { 246 },
};

inline constexpr Palette DARK_THEME {
    .background_dark = Color { 24, 24, 28 },
    .background_base = Color { 34, 34, 40 },
    .background_light = Color { 48, 48, 56 },
    .text_base = Color { 232, 232, 238 },
    .text_muted = Color { 146, 146, 158 },
    .border_base = Color { 70, 70, 82 },
    .border_muted = Color { 54, 54, 64 },
    .accent_primary = Color { 151, 118, 255 },
    .accent_secondary = Color { 186, 160, 255 },
    .alert_danger = Color { 235, 106, 106 },
    .alert_warning = Color { 235, 195, 98 },
    .alert_success = Color { 122, 214, 122 },
    .alert_info = Color { 104, 184, 235 },
    .viewport_background = Color { 41, 42, 48 },
};

/// The palette currently in effect. Mutable so a settings panel can swap it.
inline Palette gui_theme = DARK_THEME;

} // namespace Ui
