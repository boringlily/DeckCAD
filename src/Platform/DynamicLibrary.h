#pragma once
#include "Types.h"
#include <string>

namespace Platform
{

/**
 * @brief Loads a shared library from disk and can reload it in place when
 * the file on disk changes, letting application logic be swapped without
 * restarting the process.
 * @note The library is loaded from a shadow copy rather than the original
 * path, so a build can overwrite the original while this process keeps the
 * copy mapped. Each load takes a *fresh* shadow path
 * (`<path>.<pid>-<n>.load`) and the previous one is released only once the
 * replacement is mapped -- reusing a single shadow path would mean copying
 * over a file this process still has mapped, which Windows refuses.
 */
class DynamicLibrary
{
public:
    DynamicLibrary() = default;
    ~DynamicLibrary();

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    /// Points the loader at a library on disk and performs the first load.
    /// Returns false on failure.
    bool openLibrary(const std::string& library_path_ref);
    void closeLibrary();

    /// Reloads the library if its last-write time has changed since the last
    /// successful load. Returns true when a reload happened.
    bool reloadIfChanged();

    /// Looks up a function by name in the currently loaded library. Returns
    /// nullptr if the library isn't loaded or the symbol isn't found.
    void* getFunction(const char* function_name_ptr) const;

    u32 getReloadCount() const { return reload_count_; }

private:
    bool loadShadowCopy();
    std::string makeShadowPath(u32 index) const;

    std::string library_path_;
    std::string shadow_path_; ///< The shadow copy currently mapped, if any.
    u32 shadow_index_ { 0 };
    s64 last_write_time_ { 0 };
    void* handle_ptr_ { nullptr };
    u32 reload_count_ { 0 };
};

} // namespace Platform
