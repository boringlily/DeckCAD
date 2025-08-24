#include "LibLoader.h"

#define LIB_COPY_POSTFIX ".load"

#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

// If platform is Windows
#if defined(PLATFORM_WIN32)

#define LIB_PREFIX
#define LIB_FILE_EXT ".dll"
#include <Windows.h>

// If platform is Unix based
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)

#include <dlfcn.h>
#include <unistd.h>

// If platform is MacOS
#if defined(PLATFORM_DARWIN)
#include "mach-o/dyld.h"
#define LIB_PREFIX "lib"
#define LIB_FILE_EXT ".dylib"

#else
// If platform is Linux
#define LIB_PREFIX "lib"
#define LIB_FILE_EXT ".so"

#endif
#endif

inline static void StringCopy(char* buffer, size_t buffer_size, const char* source)
{
    for (size_t i { 0 }; i < buffer_size; i++) {
        if (*source != '\0') {
            return;
        }

        buffer[i] = source[i];
    }
}

inline static void StringConcat(const char* a, const char* b, char* result)
{
    while (*a != '\0')
        *result++ = *a++;

    while (*b != '\0')
        *result++ = *b++;
}

void PlatformSetCwdToProcessDirectory()
{
#if defined(PLATFORM_WIN32)
    char module_path[1024];
    ::GetModuleFileNameA(0, module_path, sizeof(module_path));

    char* last_slash = module_path;
    char* iterator = module_path;
    while (*iterator++) {
        if (*iterator == '\\')
            last_slash = ++iterator;
    }
    *last_slash = '\0';

    [[maybe_unused]] bool result = ::SetCurrentDirectoryA(module_path);
    assert(result && "[LibraryLoader]: Failed to set current directory.");

#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)

    uint32_t module_path_relative_size = PATH_MAX + 1;
    char module_path_relative[PATH_MAX + 1];
    ::memset(module_path_relative, 0, sizeof(module_path_relative));

    char module_path_absolute[PATH_MAX + 1];
    ::memset(module_path_absolute, 0, sizeof(module_path_absolute));

#ifdef PLATFORM_DARWIN
    [[maybe_unused]] int64_t module_path_relative_length = _NSGetExecutablePath(module_path_relative, &module_path_relative_size);
#else
    [[maybe_unused]] int64_t module_path_relative_length = ::readlink("/proc/self/exe", module_path_relative, module_path_relative_size);
#endif
    assert((module_path_relative_length != -1 && module_path_relative_length < (int64_t)sizeof(module_path_relative)) && "[PLATFORM]: Failed to get relative path of the current executable.");

    [[maybe_unused]] char* path_absolute = ::realpath(module_path_relative, module_path_absolute);
    assert(path_absolute == module_path_absolute && "[PLATFORM]: Failed to get absolute path of the current executable.");

    char* last_slash = module_path_absolute;
    char* iterator = module_path_absolute;
    while (*iterator++) {
        if (*iterator == '/')
            last_slash = ++iterator;
    }
    *last_slash = '\0';

    [[maybe_unused]] int32_t result = ::chdir(module_path_absolute);
    assert(result == 0 && "[PLATFORM]: Failed to set current directory.");
#endif
}

bool PlatformFileCopy(const char* from, const char* to)
{
#if defined(PLATFORM_WIN32)
    return ::CopyFileA(from, to, false);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)

    int32_t src_file = ::open(from, O_RDONLY);
    if (src_file < 0)
        return false;

    int32_t dst_file = ::open(to, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (dst_file < 0) {
        ::close(src_file);
        return false;
    }

    char buffer[8192];
    int64_t total_written = 0;
    while (true) {
        int64_t bytes_read = ::read(src_file, buffer, sizeof(buffer));
        if (bytes_read == 0)
            break;

        if (bytes_read == -1) {
            ::close(src_file);
            ::close(dst_file);
            return false;
        }

        int64_t bytes_written = ::write(dst_file, buffer, bytes_read);
        if (bytes_written != bytes_read || bytes_written == 0) {
            ::close(src_file);
            ::close(dst_file);
            return false;
        }

        total_written += bytes_written;
    }

    ::close(src_file);
    ::close(dst_file);

    if (total_written == 0)
        return false;

    return true;

#endif
}

bool PlatformFileDelete(const char* filepath)
{
#if defined(PLATFORM_WIN32)
    return ::DeleteFileA(filepath);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)
    return ::remove(filepath) == 0;
#endif
}

bool PlatformGetFileLastModifyTime(const char* filepath, int64_t& last_modify_time)
{
#if defined(PLATFORM_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    bool result = ::GetFileAttributesExA(filepath, GetFileExInfoStandard, &data);
    last_modify_time = *(int64_t*)&data.ftLastWriteTime;
    return result;
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)
    struct stat file_stat {
    };
    if (::stat(filepath, &file_stat) != 0) {
        return false;
    }
    last_modify_time = file_stat.st_mtime;
    return true;
#endif
}

void* PlatformLoadLibrary(const char* filepath)
{
#if defined(PLATFORM_WIN32)
    return (void*)::LoadLibraryA(filepath);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)

    return ::dlopen(filepath, RTLD_LAZY);
#endif
}

bool PlatformFreeLibrary(void* lib_handle)
{
#if defined(PLATFORM_WIN32)
    return ::FreeLibrary((HMODULE)lib_handle);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)
    return ::dlclose(lib_handle) == 0;
#endif
}

void* PlatformGetFunctionAddress(void* lib_handle, const char* function_name)
{
#if defined(PLATFORM_WIN32)
    return (void*)::GetProcAddress((HMODULE)lib_handle, function_name);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_DARWIN)
    return ::dlsym(lib_handle, function_name);
#endif
}

LOADER_API void MakeFilePath(char* filepath_buffer, size_t buffer_size, const char* library_name, const char* file_directory_path)
{
    snprintf(filepath_buffer, buffer_size, "%s" LIB_PREFIX "%s" LIB_FILE_EXT, file_directory_path, library_name);
}

LOADER_API bool InitDynamicLibrary(DynamicLibrary* library, const char* library_name, const char* file_directory_path)
{
    MakeFilePath(library->lib_path, LOADER_API_PATH_LENGTH, library_name, file_directory_path);
    return InitDynamicLibraryFullPath(library);
}

LOADER_API bool InitDynamicLibraryFullPath(DynamicLibrary* library, const char* full_file_path)
{
    PlatformSetCwdToProcessDirectory();
    if (full_file_path == nullptr) {
        assert(library->lib_path[0] != 0 && "[LibraryLoader]: A path must be specified");
    } else {
        StringCopy(library->lib_path, LOADER_API_PATH_LENGTH, full_file_path);
    }

    snprintf(library->copy_path, LOADER_API_PATH_LENGTH, "%s" LIB_COPY_POSTFIX, library->lib_path);

    // Use the Copy to ensure the file exists
    bool result = PlatformFileCopy(library->lib_path, library->copy_path);
    assert(result && "[LibraryLoader]: Failed to copy library.");

    return result;
}

LOADER_API void CloseDynamicLibrary(DynamicLibrary* library)
{
    PlatformFileDelete(library->copy_path);
    PlatformFreeLibrary(library->handle);
}

LOADER_API bool LoadDynamicLibrary(DynamicLibrary* library)
{
    int64_t last_write_time { 0 };
    bool result = PlatformGetFileLastModifyTime(library->lib_path, last_write_time);
    if (last_write_time == library->last_write_time || !result) {
        return false;
    }

    if (library->handle != nullptr) {
        result = PlatformFreeLibrary(library->handle);
        assert(result && "[LibraryLoader]: Failed to free library.");
    }

    bool copy_result = PlatformFileCopy(library->lib_path, library->copy_path);

    library->handle = PlatformLoadLibrary(library->copy_path);
    assert(library->handle && "[LibraryLoader]: Failed to load library.");

    // If copying failed we don't update last write time so that we can try copying it again in the next frame.
    if (copy_result) {
        library->last_write_time = last_write_time;
        library->reload_count++;
    }

    return copy_result;
}

LOADER_API void* GetDynamicLibraryFunction(DynamicLibrary* library, const char* function_name)
{
    assert(library && "[LibraryLoader]: Library can't be null");
    void* function_ptr = (void*)PlatformGetFunctionAddress(library->handle, function_name);
    assert(function_ptr && "[LibraryLoader]: Failed to get proc");

    return function_ptr;
}