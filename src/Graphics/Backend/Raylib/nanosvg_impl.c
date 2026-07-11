// Compiles the nanosvg + nanosvgrast implementations as C, so their symbols have C
// linkage matching the `extern "C"` declarations the C++ icon loader includes.
// Only built when UI_ENABLE_SVG is set (see src/Graphics/CMakeLists.txt).
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
