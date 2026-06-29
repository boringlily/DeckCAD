#pragma once
#include "RenderCommand.h"
#include "UiContext.h"

// Walk the solved tree depth-first and append render commands (background rect
// then text) into the context's command buffer.
namespace Ui {

void Emit(Context& ctx);

} // namespace Ui
