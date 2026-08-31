#pragma once
#include <string>
#include <vector>

namespace Platform
{

/**
 * @brief Locates files shipped alongside the executable.
 * @note The old build resolved assets through a "../assets/" literal, which
 * only worked when the binary was launched from one specific directory.
 * Paths are now anchored to the executable itself; the app runs from
 * anywhere.
 */
namespace Assets
{

    /// Absolute path of the assets directory. Cached after the first call.
    const std::string& Root();

    /// Resolves a path relative to the assets root, e.g. "shaders/grid.wgsl".
    std::string Resolve(const std::string& relative_path_ref);

    /// Reads a whole text file. Returns false and fills @p out_error_ref on failure.
    bool ReadTextFile(const std::string& absolute_path_ref, std::string& out_contents_ref, std::string& out_error_ref);

    /// Reads a whole binary file. Returns false and fills @p out_error_ref on failure.
    bool ReadBinaryFile(const std::string& absolute_path_ref, std::vector<unsigned char>& out_bytes_ref, std::string& out_error_ref);

} // namespace Assets

} // namespace Platform
