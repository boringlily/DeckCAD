#include "CoreApi.h"

void CoreAppInit(Core::AppState* app_ptr, ImGuiContext* imgui_context_ptr)
{
    ImGui::SetCurrentContext(imgui_context_ptr);
    if (app_ptr->scenes.empty()) {
        app_ptr->newScene();
    }
}

void CoreBuildFrame(Core::FrameContext* frame_ptr, ImGuiContext* imgui_context_ptr)
{
    ImGui::SetCurrentContext(imgui_context_ptr);
    Core::BuildFrame(*frame_ptr);
}
