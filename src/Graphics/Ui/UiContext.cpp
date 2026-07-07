#include "UiContext.h"
#include "Assets.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_wgpu.h>

#include <filesystem>

namespace Ui {
namespace UiContextInternal {

    ImVec4 ToImVec4(Color color)
    {
        DeckMath::Vector4 v = color.toVector4();
        return ImVec4 { v.x, v.y, v.z, v.w };
    }

} // namespace UiContextInternal
using namespace UiContextInternal;

Context::~Context()
{
    shutdownResources();
}

bool Context::initializeResources(SDL_Window* window_ptr, const Gpu::Context& gpu_ref, f32 display_scale, std::string& out_error_ref)
{
    display_scale_ = display_scale > 0.0f ? display_scale : 1.0f;

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        out_error_ref = "ImGui::CreateContext failed";
        return false;
    }
    initialized_ = true;

    ImGuiIO& io_ref = ImGui::GetIO();
    io_ref.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io_ref.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // CAD app wants a persistent docked layout by default
    io_ref.IniFilename = nullptr; // layout is owned by the app, not a stray ini file

    if (!ImGui_ImplSDL3_InitForOther(window_ptr)) {
        out_error_ref = "ImGui_ImplSDL3_InitForOther failed";
        return false;
    }
    sdl_backend_ready_ = true;

    ImGui_ImplWGPU_InitInfo initialization_information;
    initialization_information.Device = gpu_ref.getDevice().Get();
    initialization_information.NumFramesInFlight = 3;
    initialization_information.RenderTargetFormat = static_cast<WGPUTextureFormat>(gpu_ref.getSurfaceFormat());
    initialization_information.DepthStencilFormat = WGPUTextureFormat_Undefined;

    if (!ImGui_ImplWGPU_Init(&initialization_information)) {
        out_error_ref = "ImGui_ImplWGPU_Init failed";
        return false;
    }
    wgpu_backend_ready_ = true;

    // --- Fonts --------------------------------------------------------------
    // Sizes are in logical points. ImGui works in points here (the SDL3 backend
    // reports DisplaySize in points and the density in DisplayFramebufferScale)
    // and, because the WebGPU backend advertises RendererHasTextures, ImGui
    // rasterizes glyphs at the display's real density on its own. Scaling the
    // font size or the style by the DPI factor here would double-apply it.
    const f32 base_size = 16.0f;
    const std::string regular_path = Platform::Assets::Resolve("fonts/Nunito/static/Nunito-Regular.ttf");
    const std::string title_path = Platform::Assets::Resolve("fonts/Nunito/static/Nunito-SemiBold.ttf");

    std::error_code ec;
    if (std::filesystem::exists(regular_path, ec)) {
        font_regular_ptr_ = io_ref.Fonts->AddFontFromFileTTF(regular_path.c_str(), base_size);
    }
    if (std::filesystem::exists(title_path, ec)) {
        font_title_ptr_ = io_ref.Fonts->AddFontFromFileTTF(title_path.c_str(), 20.0f);
    }
    if (!font_regular_ptr_) {
        // missing font isn't fatal — falls back to ImGui's built-in font
        font_regular_ptr_ = io_ref.Fonts->AddFontDefault();
    }
    io_ref.FontDefault = font_regular_ptr_;

    applyTheme();
    return true;
}

void Context::applyTheme()
{
    if (!initialized_) {
        return;
    }

    ImGuiStyle& style_ref = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style_ref.WindowRounding = 6.0f;
    style_ref.ChildRounding = 6.0f;
    style_ref.FrameRounding = 4.0f;
    style_ref.PopupRounding = 6.0f;
    style_ref.ScrollbarRounding = 6.0f;
    style_ref.GrabRounding = 4.0f;
    style_ref.TabRounding = 6.0f;
    style_ref.WindowBorderSize = 1.0f;
    style_ref.FrameBorderSize = 0.0f;
    style_ref.WindowPadding = ImVec2(8, 8);
    style_ref.FramePadding = ImVec2(8, 4);
    style_ref.ItemSpacing = ImVec2(8, 6);
    style_ref.ItemInnerSpacing = ImVec2(6, 4);
    style_ref.IndentSpacing = 18.0f;
    style_ref.ScrollbarSize = 12.0f;
    style_ref.GrabMinSize = 10.0f;

    const Palette& theme_ref = gui_theme;
    ImVec4* colors_ptr = style_ref.Colors;

    colors_ptr[ImGuiCol_Text] = ToImVec4(theme_ref.text_base);
    colors_ptr[ImGuiCol_TextDisabled] = ToImVec4(theme_ref.text_muted);
    colors_ptr[ImGuiCol_WindowBg] = ToImVec4(theme_ref.background_base);
    colors_ptr[ImGuiCol_ChildBg] = ToImVec4(theme_ref.background_base);
    colors_ptr[ImGuiCol_PopupBg] = ToImVec4(theme_ref.background_light);
    colors_ptr[ImGuiCol_Border] = ToImVec4(theme_ref.border_muted);
    colors_ptr[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    colors_ptr[ImGuiCol_FrameBg] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_FrameBgHovered] = ToImVec4(theme_ref.background_light);
    colors_ptr[ImGuiCol_FrameBgActive] = ToImVec4(theme_ref.accent_primary.withAlpha(120));

    colors_ptr[ImGuiCol_TitleBg] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_TitleBgActive] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_TitleBgCollapsed] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_MenuBarBg] = ToImVec4(theme_ref.background_dark);

    colors_ptr[ImGuiCol_ScrollbarBg] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_ScrollbarGrab] = ToImVec4(theme_ref.border_muted);
    colors_ptr[ImGuiCol_ScrollbarGrabHovered] = ToImVec4(theme_ref.border_base);
    colors_ptr[ImGuiCol_ScrollbarGrabActive] = ToImVec4(theme_ref.accent_primary);

    colors_ptr[ImGuiCol_CheckMark] = ToImVec4(theme_ref.accent_primary);
    colors_ptr[ImGuiCol_SliderGrab] = ToImVec4(theme_ref.accent_primary);
    colors_ptr[ImGuiCol_SliderGrabActive] = ToImVec4(theme_ref.accent_secondary);

    colors_ptr[ImGuiCol_Button] = ToImVec4(theme_ref.background_light);
    colors_ptr[ImGuiCol_ButtonHovered] = ToImVec4(theme_ref.accent_primary.withAlpha(150));
    colors_ptr[ImGuiCol_ButtonActive] = ToImVec4(theme_ref.accent_primary);

    colors_ptr[ImGuiCol_Header] = ToImVec4(theme_ref.background_light);
    colors_ptr[ImGuiCol_HeaderHovered] = ToImVec4(theme_ref.accent_primary.withAlpha(120));
    colors_ptr[ImGuiCol_HeaderActive] = ToImVec4(theme_ref.accent_primary.withAlpha(180));

    colors_ptr[ImGuiCol_Separator] = ToImVec4(theme_ref.border_muted);
    colors_ptr[ImGuiCol_SeparatorHovered] = ToImVec4(theme_ref.accent_primary);
    colors_ptr[ImGuiCol_SeparatorActive] = ToImVec4(theme_ref.accent_secondary);

    colors_ptr[ImGuiCol_ResizeGrip] = ToImVec4(theme_ref.border_muted);
    colors_ptr[ImGuiCol_ResizeGripHovered] = ToImVec4(theme_ref.accent_primary);
    colors_ptr[ImGuiCol_ResizeGripActive] = ToImVec4(theme_ref.accent_secondary);

    colors_ptr[ImGuiCol_Tab] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_TabHovered] = ToImVec4(theme_ref.accent_primary.withAlpha(150));
    colors_ptr[ImGuiCol_TabSelected] = ToImVec4(theme_ref.background_base);
    colors_ptr[ImGuiCol_TabDimmed] = ToImVec4(theme_ref.background_dark);
    colors_ptr[ImGuiCol_TabDimmedSelected] = ToImVec4(theme_ref.background_base);

    colors_ptr[ImGuiCol_DockingPreview] = ToImVec4(theme_ref.accent_primary.withAlpha(140));
    colors_ptr[ImGuiCol_DockingEmptyBg] = ToImVec4(theme_ref.background_dark);
}

void Context::beginFrame()
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Context::renderFrame(const wgpu::RenderPassEncoder& pass_ref)
{
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass_ref.Get());
}

void Context::shutdownResources()
{
    if (wgpu_backend_ready_) {
        ImGui_ImplWGPU_Shutdown();
        wgpu_backend_ready_ = false;
    }
    if (sdl_backend_ready_) {
        ImGui_ImplSDL3_Shutdown();
        sdl_backend_ready_ = false;
    }
    if (initialized_) {
        ImGui::DestroyContext();
        initialized_ = false;
    }
}

} // namespace Ui
