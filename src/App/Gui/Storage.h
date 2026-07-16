#pragma once
#include "Convert.h"
#include "DcadFile.h"
#include "Scene.h"
#include "StoragePath.h"

#include "raylib.h" // GetApplicationDirectory

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

// The app-layer storage wrapper: the only serialization code that touches the filesystem
// and raylib. Kept thin so the serialization core (DTO/convert/glaze/paths) stays
// hermetic and testable in core_tests.
//
// EXCEPTION-FREE end to end. std::filesystem is used through its std::error_code
// overloads (the throwing ones would std::terminate under -fno-exceptions), and the
// streams never have exceptions enabled, so every failure is a returned bool.
namespace AppStorage {

// GetApplicationDirectory is the EXECUTABLE's directory — distinct from the working
// directory the app loads assets from ("../assets"). The cache belongs beside the exe as
// the user asked, so this is the right anchor.
inline std::string ExeDir() { return std::string { GetApplicationDirectory() }; }

// A path directly beside the executable (for a durable, non-cache .dcad).
inline std::string JoinExe(std::string_view leaf) { return Serialize::JoinPath(ExeDir(), leaf); }

inline bool FileExists(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

// Read a whole file into `out`. Returns false if it can't be opened (missing / locked).
inline bool ReadFile(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    in.read(out.data(), size);
    return static_cast<bool>(in) || in.eof();
}

// Write `content` to `path` atomically: to a sibling ".tmp" first, then rename over the
// target. A crash mid-write can then only ever damage the throwaway temp, never an
// existing good file. Creates the parent directory if needed.
inline bool WriteFileAtomic(const std::string& path, std::string_view content)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path target { path };
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec); // ec overload: no throw
    }

    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) {
            return false;
        }
    } // stream closed here, before the rename

    fs::rename(tmp, target, ec);
    if (ec) {
        // Some platforms won't rename onto an existing file; fall back to replace.
        fs::remove(target, ec);
        fs::rename(tmp, target, ec);
    }
    return !ec;
}

// ── save / load a Scene ─────────────────────────────────────────────────────────

inline bool SaveScene(const Scene& scene, const std::string& path)
{
    Serialize::SceneMeta meta { scene.display_unit, scene.filename };
    Serialize::DocumentDto dto = Serialize::ToDto(scene.workbench.Doc(), scene.workbench.Params(), meta);
    return WriteFileAtomic(path, Serialize::WriteDcad(dto));
}

inline bool LoadScene(Scene& scene, const std::string& path, Serialize::SerError& err)
{
    std::string json;
    if (!ReadFile(path, json)) {
        err.ok = false;
        err.message = "could not open " + path;
        return false;
    }

    Serialize::DocumentDto dto;
    if (!Serialize::ReadDcad(json, dto, err)) {
        return false;
    }

    // Drop any in-progress gesture / nested context before swapping the document out.
    scene.workbench.PrepareForLoad();

    Serialize::SceneMeta meta;
    if (!Serialize::FromDto(dto, scene.workbench.Doc(), scene.workbench.Params(), meta)) {
        err.ok = false;
        err.message = "unsupported .dcad version";
        return false;
    }

    scene.display_unit = meta.displayUnit;
    if (!meta.name.empty()) {
        scene.filename = meta.name;
    }
    return true;
}

// ── auto-save cache ─────────────────────────────────────────────────────────────

inline std::string SceneCachePath(const Scene& scene)
{
    return Serialize::CachePath(ExeDir(), scene.filename);
}

// Write the scene's auto-save cache under <exeDir>/cache/<name>.cache.dcad.
inline bool AutoSave(const Scene& scene)
{
    return SaveScene(scene, SceneCachePath(scene));
}

} // namespace AppStorage
