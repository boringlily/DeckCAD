#pragma once
#include "DTL.h"
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