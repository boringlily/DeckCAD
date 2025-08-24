#pragma once
#include "DumbTypes.h"
#include "clay.h"
#include "raylib.h"
#include <array>

struct StyleColor {
    u8 red { 255 };
    u8 green { 255 };
    u8 blue { 255 };
    u8 alpha { 255 };

    constexpr StyleColor(u8 r, u8 g, u8 b)
        : red { r }
        , green { g }
        , blue { b }
        , alpha { 255u } {};
    constexpr StyleColor(u8 sameValue)
        : red { sameValue }
        , green { sameValue }
        , blue { sameValue }
        , alpha { 255u } {};
    constexpr StyleColor() {};

    constexpr operator Clay_Color() const { return ToClayColor(); }

    inline Clay_Color ToClayColor() const
    {
        return (Clay_Color) {
            .r = static_cast<float>(red),
            .g = static_cast<float>(green),
            .b = static_cast<float>(blue),
            .a = static_cast<float>(alpha)
        };
    }
};

struct CanvasColorTheme {
    StyleColor background;
};

struct ClayColorTheme {
    StyleColor BgDark;
    StyleColor BgBase;
    StyleColor BgLight;
    StyleColor TextBase;
    StyleColor TextMuted;
    StyleColor BorderBase;
    StyleColor BorderMuted;
    StyleColor AccentPrimary;
    StyleColor AccentSecondary;
    StyleColor AlertDanger;
    StyleColor AlertWarning;
    StyleColor AlertSuccess;
    StyleColor AlertInfo;
};

static constexpr ClayColorTheme SIMPLE_LIGHT_THEME = {
    .BgDark = StyleColor { 210 },
    .BgBase = StyleColor { 239 },
    .BgLight = StyleColor { 255 },
    .TextBase = StyleColor { 13 },
    .TextMuted = StyleColor { 128 },
    .BorderBase = StyleColor { 102 },
    .BorderMuted = StyleColor { 153 },
    .AccentPrimary = StyleColor { 172, 153, 255 },
    .AccentSecondary = StyleColor { 151, 71, 255 },
    .AlertDanger = StyleColor { 255, 127, 127 },
    .AlertWarning = StyleColor { 255, 225, 127 },
    .AlertSuccess = StyleColor { 149, 255, 127 },
    .AlertInfo = StyleColor { 127, 212, 255 },
};

inline ClayColorTheme GuiTheme = SIMPLE_LIGHT_THEME;

enum class FontId : u8 {
    Regular,
    Medium,
    MediumItalic,
    Semibold,
};
struct ClayTextStyles {
    Clay_TextElementConfig title;
    Clay_TextElementConfig subtitle;
    Clay_TextElementConfig body;
    Clay_TextElementConfig caption;
    Clay_TextElementConfig buttonActive;
    Clay_TextElementConfig buttonMuted;
};

static inline ClayTextStyles TextStyle = {
    .title = { .textColor = GuiTheme.TextBase, .fontId = static_cast<u8>(FontId::Semibold), .fontSize = 20, .wrapMode = CLAY_TEXT_WRAP_NONE },
    .subtitle = { .textColor = GuiTheme.TextBase, .fontId = static_cast<u8>(FontId::Semibold), .fontSize = 18, .wrapMode = CLAY_TEXT_WRAP_NONE },
    .body = { .textColor = GuiTheme.TextBase, .fontId = static_cast<u8>(FontId::Regular), .fontSize = 14 },
    .caption = {
        .textColor = GuiTheme.TextBase,
        .fontId = static_cast<u8>(FontId::MediumItalic),
        .fontSize = 12,
    },
    .buttonActive = { .textColor = GuiTheme.TextBase, .fontId = static_cast<u8>(FontId::Regular), .fontSize = 14, .wrapMode = CLAY_TEXT_WRAP_NONE },
    .buttonMuted = { .textColor = GuiTheme.TextMuted, .fontId = static_cast<u8>(FontId::Regular), .fontSize = 14, .wrapMode = CLAY_TEXT_WRAP_NONE, .textAlignment = CLAY_TEXT_ALIGN_CENTER },
};

#define Make_Icons(DO)  \
    DO(Check)           \
    DO(Exit)            \
    DO(Home)            \
    DO(Parameters)      \
    DO(Plus)            \
    DO(Project)         \
    DO(ProjectSettings) \
    DO(Settings)        \
    DO(Unknown)

#define MAKE_ENUM(VAR) VAR,
enum IconId {
    Make_Icons(MAKE_ENUM)
};