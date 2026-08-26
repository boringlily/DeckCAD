#include "DynamicLibrary.h"

#include <filesystem>
#include <system_error>

#if defined(PLATFORM_WIN32)
#include <Windows.h>
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)
#include <dlfcn.h>
#endif

namespace Platform {
namespace DynamicLibraryInternal {

    void* PlatformLoadLibrary(const std::string& path_ref)
    {
#if defined(PLATFORM_WIN32)
        return static_cast<void*>(::LoadLibraryA(path_ref.c_str()));
#else
        return ::dlopen(path_ref.c_str(), RTLD_NOW);
#endif
    }

    void PlatformFreeLibrary(void* handle_ptr)
    {
#if defined(PLATFORM_WIN32)
        ::FreeLibrary(static_cast<HMODULE>(handle_ptr));
#else
        ::dlclose(handle_ptr);
#endif
    }

    void* PlatformGetFunction(void* handle_ptr, const char* function_name_ptr)
    {
#if defined(PLATFORM_WIN32)
        return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle_ptr), function_name_ptr));
#else
        return ::dlsym(handle_ptr, function_name_ptr);
#endif
    }

    s64 GetLastWriteTime(const std::string& path_ref)
    {
        std::error_code error;
        auto write_time = std::filesystem::last_write_time(path_ref, error);
        if (error) {
            return 0;
        }
        return write_time.time_since_epoch().count();
    }

} // namespace DynamicLibraryInternal
using namespace DynamicLibraryInternal;

DynamicLibrary::~DynamicLibrary()
{
    closeLibrary();
}

bool DynamicLibrary::openLibrary(const std::string& library_path_ref)
{
    library_path_ = library_path_ref;
    shadow_path_ = library_path_ref + ".load";
    last_write_time_ = 0;
    return loadShadowCopy();
}

void DynamicLibrary::closeLibrary()
{
    if (handle_ptr_) {
        PlatformFreeLibrary(handle_ptr_);
        handle_ptr_ = nullptr;
    }
    if (!shadow_path_.empty()) {
        std::error_code error;
        std::filesystem::remove(shadow_path_, error);
    }
}

bool DynamicLibrary::reloadIfChanged()
{
    const s64 write_time = GetLastWriteTime(library_path_);
    if (write_time == 0 || write_time == last_write_time_) {
        return false;
    }
    return loadShadowCopy();
}

bool DynamicLibrary::loadShadowCopy()
{
    std::error_code error;
    std::filesystem::copy_file(library_path_, shadow_path_, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        // Likely a build is mid-write to the original file; try again next time.
        return false;
    }

    if (handle_ptr_) {
        PlatformFreeLibrary(handle_ptr_);
        handle_ptr_ = nullptr;
    }

    handle_ptr_ = PlatformLoadLibrary(shadow_path_);
    if (!handle_ptr_) {
        return false;
    }

    last_write_time_ = GetLastWriteTime(library_path_);
    ++reload_count_;
    return true;
}

void* DynamicLibrary::getFunction(const char* function_name_ptr) const
{
    if (!handle_ptr_) {
        return nullptr;
    }
    return PlatformGetFunction(handle_ptr_, function_name_ptr);
}

} // namespace Platform
