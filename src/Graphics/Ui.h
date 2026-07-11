#pragma once

// Umbrella public header for the Ui framework. Include this to get the core
// context, the component surface, and the default Raylib backend factory.
//
// A flexbox immediate-mode GUI: subclass the abstract components (Panel, Label),
// keep one long-lived instance each, and mark element boundaries with Begin()/End().
// The layout engine allocates nothing of its own - it bump-allocates over a
// user-supplied buffer passed to Context::Init.

#include "Core/UiTypes.h"
#include "Core/UiId.h"
#include "Core/UiNode.h"
#include "Core/Arena.h"
#include "Core/RenderCommand.h"
#include "Core/UiContext.h"
#include "Core/Layout.h"
#include "Core/Emit.h"
#include "Core/Input.h"

#include "Backend/IBackend.h"
#include "Backend/Dispatch.h"
#include "Backend/Raylib/RaylibBackend.h"
#include "Backend/Raylib/Canvas3D.h"

#include "Components/Component.h"
#include "Components/Panel.h"
#include "Components/Label.h"
#include "Components/Icon.h"
#include "Components/Button.h"
#include "Components/Composition.h"
#include "Components/Checkbox.h"
#include "Components/Dropdown.h"
#include "Components/MessageBox.h"
#include "Components/ScrollPanel.h"
#include "Components/Input.h"
