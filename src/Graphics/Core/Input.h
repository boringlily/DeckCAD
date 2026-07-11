#pragma once
#include "UiContext.h"

// Pointer hit-testing. Runs after layout: finds the topmost node whose rect
// contains the pointer and records its id as the hot id for next frame.
namespace Ui {

void ResolveInput(Context& ctx);

} // namespace Ui
