#pragma once
#include "DTL.h"

#include <string>
#include <string_view>

// Path construction for `.dcad` files — pure string logic, no filesystem, no raylib, so
// it is unit-testable in core_tests. The actual I/O and the exe-directory lookup live in
// the app-layer Storage wrapper.
namespace Serialize {

inline constexpr std::string_view kDcadExt = ".dcad";
inline constexpr std::string_view kCacheExt = ".cache.dcad";
inline constexpr std::string_view kCacheDirName = "cache";

// Make a scene name safe to use as a filename stem: anything that isn't alphanumeric,
// space, dash, or underscore becomes '_', leading/trailing spaces are dropped, and an
// empty result falls back to "untitled". Keeps a user-typed scene name ("Deck #1") from
// producing an invalid or surprising path.
inline std::string SanitizeStem(std::string_view name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == ' ' || c == '-' || c == '_';
        out.push_back(ok ? c : '_');
    }
    // Trim surrounding spaces.
    u32 begin = 0;
    while (begin < out.size() && out[begin] == ' ') {
        ++begin;
    }
    u32 end = static_cast<u32>(out.size());
    while (end > begin && out[end - 1] == ' ') {
        --end;
    }
    std::string trimmed = out.substr(begin, end - begin);
    return trimmed.empty() ? std::string { "untitled" } : trimmed;
}

// Join two path fragments with a forward slash, avoiding a doubled or missing separator.
// Forward slashes work on every platform the app targets (Win32 accepts them).
inline std::string JoinPath(std::string_view dir, std::string_view leaf)
{
    if (dir.empty()) {
        return std::string { leaf };
    }
    std::string out { dir };
    if (out.back() != '/' && out.back() != '\\') {
        out.push_back('/');
    }
    out.append(leaf);
    return out;
}

// "<stem>.dcad" — the user-facing document filename.
inline std::string DcadFileName(std::string_view sceneName)
{
    return SanitizeStem(sceneName) + std::string { kDcadExt };
}

// "<stem>.cache.dcad" — the auto-save filename.
inline std::string CacheFileName(std::string_view sceneName)
{
    return SanitizeStem(sceneName) + std::string { kCacheExt };
}

// The directory auto-saves live in: "<exeDir>/cache".
inline std::string CacheDir(std::string_view exeDir)
{
    return JoinPath(exeDir, kCacheDirName);
}

// The full auto-save path: "<exeDir>/cache/<stem>.cache.dcad".
inline std::string CachePath(std::string_view exeDir, std::string_view sceneName)
{
    return JoinPath(CacheDir(exeDir), CacheFileName(sceneName));
}

} // namespace Serialize
