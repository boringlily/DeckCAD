#include "DynamicLibrary.h"

#include <filesystem>
#include <system_error>

#if defined(PLATFORM_WIN32)
#include <Windows.h>
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace Platform
{
namespace DynamicLibraryInternal
{

    constexpr const char* SHADOW_SUFFIX = ".load";

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
        if(error)
        {
            return 0;
        }
        return write_time.time_since_epoch().count();
    }

    u32 GetCurrentProcessIdentifier()
    {
#if defined(PLATFORM_WIN32)
        return static_cast<u32>(::GetCurrentProcessId());
#else
        return static_cast<u32>(::getpid());
#endif
    }

    /// Deletes shadow copies this process is not using: leftovers from an
    /// earlier run that crashed before it could clean up after itself. Ones
    /// another live instance still has mapped simply refuse to be deleted,
    /// which is why every failure here is ignored rather than reported.
    void RemoveStaleShadowCopies(const std::string& library_path_ref, const std::string& keep_path_ref)
    {
        const std::filesystem::path library_path { library_path_ref };
        const std::string prefix = library_path.filename().string() + ".";

        std::error_code error;
        for(const auto& entry_ref : std::filesystem::directory_iterator(library_path.parent_path(), error))
        {
            const std::string name = entry_ref.path().filename().string();
            if(name.rfind(prefix, 0) != 0 || entry_ref.path().extension() != SHADOW_SUFFIX)
            {
                continue;
            }
            if(entry_ref.path().string() == keep_path_ref)
            {
                continue;
            }
            std::error_code remove_error;
            std::filesystem::remove(entry_ref.path(), remove_error);
        }
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
    shadow_path_.clear();
    shadow_index_ = 0;
    last_write_time_ = 0;
    RemoveStaleShadowCopies(library_path_, "");
    return loadShadowCopy();
}

void DynamicLibrary::closeLibrary()
{
    if(handle_ptr_)
    {
        PlatformFreeLibrary(handle_ptr_);
        handle_ptr_ = nullptr;
    }
    if(!shadow_path_.empty())
    {
        std::error_code error;
        std::filesystem::remove(shadow_path_, error);
        shadow_path_.clear();
    }
}

bool DynamicLibrary::reloadIfChanged()
{
    const s64 write_time = GetLastWriteTime(library_path_);
    if(write_time == 0 || write_time == last_write_time_)
    {
        return false;
    }
    return loadShadowCopy();
}

std::string DynamicLibrary::makeShadowPath(u32 index) const
{
    // The process id keeps two instances of the app running from the same
    // build directory off each other's shadow copies; the index keeps each
    // reload off the copy the previous one is still using.
    return library_path_ + "." + std::to_string(GetCurrentProcessIdentifier()) + "-" + std::to_string(index)
        + SHADOW_SUFFIX;
}

bool DynamicLibrary::loadShadowCopy()
{
    // Every load gets its OWN shadow file, and the library currently in use is
    // released only once its replacement is mapped. Reusing one fixed shadow
    // path cannot work: the copy would have to overwrite the very file this
    // process has mapped, which Windows refuses outright ("the process cannot
    // access the file because it is being used by another process"), so every
    // reload after the first failed silently and forever. Ordering it this way
    // also means a failed reload leaves the current library loaded and callable
    // rather than unloading it and leaving the caller's resolved function
    // pointers dangling into freed address space.
    const std::string next_shadow_path = makeShadowPath(shadow_index_);

    std::error_code error;
    std::filesystem::copy_file(
        library_path_, next_shadow_path, std::filesystem::copy_options::overwrite_existing, error);
    if(error)
    {
        // Likely a build is mid-write to the original file; try again next time.
        return false;
    }

    void* next_handle_ptr = PlatformLoadLibrary(next_shadow_path);
    if(!next_handle_ptr)
    {
        std::error_code remove_error;
        std::filesystem::remove(next_shadow_path, remove_error);
        return false;
    }

    if(handle_ptr_)
    {
        PlatformFreeLibrary(handle_ptr_);
    }
    if(!shadow_path_.empty())
    {
        std::error_code remove_error;
        std::filesystem::remove(shadow_path_, remove_error);
    }

    handle_ptr_ = next_handle_ptr;
    shadow_path_ = next_shadow_path;
    ++shadow_index_;
    last_write_time_ = GetLastWriteTime(library_path_);
    ++reload_count_;
    return true;
}

void* DynamicLibrary::getFunction(const char* function_name_ptr) const
{
    if(!handle_ptr_)
    {
        return nullptr;
    }
    return PlatformGetFunction(handle_ptr_, function_name_ptr);
}

} // namespace Platform
