#pragma once
#include "UiContext.h"

// Three-pass flexbox solver. Allocates nothing: it only writes the `measured` and
// `rect` scratch fields on the nodes already in the arena.
namespace Ui {

void Solve(Context& ctx); // root is node index 0.

} // namespace Ui
