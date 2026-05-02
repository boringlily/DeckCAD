#include "HotReloadCore.h"

#include <cstdio>
#include <filesystem>

namespace HotReloadCoreInternal {

#if defined(PLATFORM_WIN32)
constexpr const char* CORE_LIBRARY_FILE_NAME = "deckcad_core.dll";
#elif defined(PLATFORM_DARWIN)
constexpr const char* CORE_LIBRARY_FILE_NAME = "libdeckcad_core.dylib";
#else
constexpr const char* CORE_LIBRARY_FILE_NAME = "libdeckcad_core.so";
#endif

} // namespace HotReloadCoreInternal
using namespace HotReloadCoreInternal;

bool HotReloadCore::openLibrary(const std::string& library_directory_ref)
{
    const std::string library_path = (std::filesystem::path(library_directory_ref) / CORE_LIBRARY_FILE_NAME).string();
    if (!library_.openLibrary(library_path)) {
        std::fprintf(stderr, "[DeckCAD] hot reload: failed to load '%s'\n", library_path.c_str());
        return false;
    }

    resolveFunctions();
    if (!app_init_func_ptr_ || !build_frame_func_ptr_) {
        std::fprintf(stderr, "[DeckCAD] hot reload: '%s' is missing CoreAppInit or CoreBuildFrame\n", library_path.c_str());
        return false;
    }
    return true;
}

void HotReloadCore::resolveFunctions()
{
    app_init_func_ptr_ = reinterpret_cast<AppInitFunc>(library_.getFunction("CoreAppInit"));
    build_frame_func_ptr_ = reinterpret_cast<BuildFrameFunc>(library_.getFunction("CoreBuildFrame"));
}

void HotReloadCore::reloadIfChanged(Core::AppState& app_ref)
{
    if (library_.reloadIfChanged()) {
        std::printf("[DeckCAD] Hot reload: Core (%u)\n", library_.getReloadCount());
        resolveFunctions();
        appInit(app_ref);
    }
}

void HotReloadCore::appInit(Core::AppState& app_ref)
{
    if (app_init_func_ptr_) {
        app_init_func_ptr_(&app_ref, ImGui::GetCurrentContext());
    }
}

void HotReloadCore::buildFrame(Core::FrameContext& frame_ref)
{
    if (build_frame_func_ptr_) {
        build_frame_func_ptr_(&frame_ref, ImGui::GetCurrentContext());
    }
}
