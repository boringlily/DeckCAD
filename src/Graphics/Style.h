#pragma once
#include "DTL.h"

// Graphics' asset vocabulary: identifies which loaded font/icon asset an element
// wants. Graphics::Initialize() loads the actual Font/Texture2D resources named
// here (LoadAppFonts / LoadUiIcons in Graphics.cpp) at boot, independent of
// App.dll — so these ids live here rather than in App, the one place both
// Graphics and App can reach them.
//
// Colors/theming are NOT here. The single palette every component reads is
// Ui::ColorScheme (Ui/Backend/IBackend.h), via Ui::Colors().
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
