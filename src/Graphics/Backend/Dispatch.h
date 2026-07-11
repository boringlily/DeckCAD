#pragma once
#include "IBackend.h"
#include "RenderCommand.h"
#include "Graphics.export.h"

namespace Ui {

// Walk the render command buffer and issue backend calls. Mirrors the switch in
// the existing Graphics::Render.
GRAPHICS_API void Dispatch(const RenderCommandBuffer& buffer, const UiBackend& backend);

} // namespace Ui
