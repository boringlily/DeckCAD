#include "Assets.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <filesystem>

namespace Platform::Assets {
namespace AssetsInternal {

    std::string ComputeRoot()
    {
        // SDL_GetBasePath() is the directory the executable lives in.
        const char* base_ptr = SDL_GetBasePath();
        std::filesystem::path start = base_ptr ? std::filesystem::path(base_ptr) : std::filesystem::current_path();

        // walk up looking for "assets": covers the installed layout
        // (assets beside the binary) and the dev layout (binary in
        // build/bin/<Config>, assets at the repo root)
        std::filesystem::path directory = start;
        for (int depth = 0; depth < 6; ++depth) {
            std::filesystem::path candidate = directory / "assets";
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec)) {
                return candidate.string();
            }
            if (!directory.has_parent_path() || directory.parent_path() == directory) {
                break;
            }
            directory = directory.parent_path();
        }

        // not found: fall back to the expected location, errors then name
        // a concrete path instead of an empty string
        return (start / "assets").string();
    }

} // namespace AssetsInternal
using namespace AssetsInternal;

const std::string& Root()
{
    static const std::string root = ComputeRoot();
    return root;
}

std::string Resolve(const std::string& relative_path_ref)
{
    return (std::filesystem::path(Root()) / relative_path_ref).string();
}

bool ReadTextFile(const std::string& absolute_path_ref, std::string& out_contents_ref, std::string& out_error_ref)
{
    std::FILE* file_ptr = std::fopen(absolute_path_ref.c_str(), "rb");
    if (!file_ptr) {
        out_error_ref = "Could not open '" + absolute_path_ref + "'";
        return false;
    }

    std::fseek(file_ptr, 0, SEEK_END);
    long size = std::ftell(file_ptr);
    std::fseek(file_ptr, 0, SEEK_SET);
    if (size < 0) {
        std::fclose(file_ptr);
        out_error_ref = "Could not determine the size of '" + absolute_path_ref + "'";
        return false;
    }

    out_contents_ref.resize(static_cast<size_t>(size));
    size_t read = size > 0 ? std::fread(out_contents_ref.data(), 1, static_cast<size_t>(size), file_ptr) : 0;
    std::fclose(file_ptr);

    if (read != static_cast<size_t>(size)) {
        out_error_ref = "Short read on '" + absolute_path_ref + "'";
        return false;
    }
    return true;
}

bool ReadBinaryFile(const std::string& absolute_path_ref, std::vector<unsigned char>& out_bytes_ref, std::string& out_error_ref)
{
    std::FILE* file_ptr = std::fopen(absolute_path_ref.c_str(), "rb");
    if (!file_ptr) {
        out_error_ref = "Could not open '" + absolute_path_ref + "'";
        return false;
    }

    std::fseek(file_ptr, 0, SEEK_END);
    long size = std::ftell(file_ptr);
    std::fseek(file_ptr, 0, SEEK_SET);
    if (size < 0) {
        std::fclose(file_ptr);
        out_error_ref = "Could not determine the size of '" + absolute_path_ref + "'";
        return false;
    }

    out_bytes_ref.resize(static_cast<size_t>(size));
    size_t read = size > 0 ? std::fread(out_bytes_ref.data(), 1, static_cast<size_t>(size), file_ptr) : 0;
    std::fclose(file_ptr);

    if (read != static_cast<size_t>(size)) {
        out_error_ref = "Short read on '" + absolute_path_ref + "'";
        return false;
    }
    return true;
}

} // namespace Platform::Assets
