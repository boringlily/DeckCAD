#pragma once
#include "DTL.h"
#include "IBackend.h"
#include "raylib.h"

// Default backend: binds the Ui vtable to raylib draw/text calls. Designed so a
// Raylib+Angle/GLES variant can replace this single translation unit later.
namespace Ui::Raylib {

// Caller-owned persistent state referenced by the backend vtable. The font and
// icon arrays must outlive the backend (the framework only stores a pointer).
struct State {
    Font* fonts { nullptr };
    u32 fontCount { 0 };
    Texture2D* icons { nullptr };
    u32 iconCount { 0 };
};

UiBackend MakeBackend(State* state, ColorScheme colors = {});

// Load an icon texture, preferring `<basePathNoExt>.svg` (rasterized at pixelSize,
// requires the UI_ENABLE_SVG build option + the nanosvg submodule) over
// `<basePathNoExt>.png`. Returns a texture with .id == 0 if neither file exists.
Texture2D LoadIcon(const char* basePathNoExt, int pixelSize);

} // namespace Ui::Raylib
