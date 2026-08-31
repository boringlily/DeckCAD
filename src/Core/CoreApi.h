#pragma once
#include "App.h"
#include "AppState.h"

#if defined(PLATFORM_WIN32) && defined(DECKCAD_CORE_SHARED)
#define DECKCAD_CORE_API __declspec(dllexport)
#else
#define DECKCAD_CORE_API
#endif

/**
 * @brief C-linkage entry points used to hot-reload deckcad_core as a shared library.
 * @note extern "C" avoids C++ name mangling; Platform::DynamicLibrary looks these up
 * by plain name. Both entry points also take the caller's ImGuiContext: Dear ImGui
 * keeps its state in a plain global (GImGui) inside imgui.cpp, and that translation
 * unit is compiled separately into the executable and into this shared library --
 * each ends up with its own independent, uninitialized copy of that global.
 * ImGui::SetCurrentContext() points this library's copy at the context the executable
 * already created; see Dear ImGui's own guidance on using it across DLL boundaries.
 */
extern "C" {

/**
 * @brief Seeds a freshly loaded AppState with an initial scene.
 * @note Only touches *app_ptr when it has no scenes yet; calling this again
 * after a reload leaves existing state alone.
 */
DECKCAD_CORE_API void CoreAppInit(Core::AppState* app_ptr, ImGuiContext* imgui_context_ptr);

/**
 * @brief Builds one frame.
 * @note Stands in for Core::BuildFrame() when Core is loaded as a hot-reloadable
 * shared library.
 */
DECKCAD_CORE_API void CoreBuildFrame(Core::FrameContext* frame_ptr, ImGuiContext* imgui_context_ptr);

} // extern "C"
