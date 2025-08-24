#pragma once

#ifndef LOADER_API
#define LOADER_API
#endif

#include <cstdint>
static constexpr size_t LOADER_API_PATH_LENGTH { 512 };

struct DynamicLibrary {
    char lib_path[512] = {};
    char copy_path[512] = {};
    uint32_t reload_count { 0 };
    uint64_t last_write_time { 0 };
    void* handle { nullptr };
};

/// @brief
/// @param library
/// @param library_name
/// @param file_directory_path
/// @return
LOADER_API bool InitDynamicLibrary(DynamicLibrary* library, const char* library_name, const char* file_directory_path);

/// @brief Init library without constructing the full path. Either pass the path into library objcet or
/// @param library
/// @param full_file_path
/// @return True if initialization is successful
LOADER_API bool InitDynamicLibraryFullPath(DynamicLibrary* library, const char* full_file_path = nullptr);

/// @brief Deinits the loaded library handle
/// @param library the object to deinit
LOADER_API void CloseDynamicLibrary(DynamicLibrary* library);

/// @brief Loads a the specified library handle. If already loaded, will reload the library if the file timestamp is newer than what is already loaded.
/// @param library Address of library struct.
/// @return true if the library was loaded.
LOADER_API bool LoadDynamicLibrary(DynamicLibrary* library);

/// @brief Get an address for a named function from the library.
/// @param library Loaded library
/// @param function_name Function you want
/// @return Function pointer. (Must be cast to function type.)
LOADER_API void* GetDynamicLibraryFunction(DynamicLibrary* library, const char* function_name);
