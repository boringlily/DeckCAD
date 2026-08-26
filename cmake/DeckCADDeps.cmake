# Shares compiled objects for identical (source, flags) across worktrees and
# build directories via a compiler cache (ccache/sccache), so a fresh
# worktree building the same submodule commit skips recompiling Dawn/SDL/etc.
# instead of the usual 10-20 minute cold start.
#
# This is not the same problem as CMake's own build-directory caching: CMake's
# Ninja generator keeps its single incremental-build log (.ninja_log,
# .ninja_deps) only in the top-level build directory, so pointing
# add_subdirectory() at a shared binary directory does NOT get you
# cross-worktree build avoidance -- a fresh worktree's Ninja has no memory of
# having built those objects before, even if they're sitting on disk unused.
# A compiler cache sidesteps that: it keys on a hash of the preprocessed
# source and flags, independent of which build directory or Ninja log
# invoked it.

# Resolves DECKCAD_DEPS_DIR once: $ENV{DECKCAD_DEPS_DIR} if set, else a
# per-OS default cache location. Exposed as a cache variable so it shows up
# in `cmake -L` and IDE tooling. Currently used to host the compiler cache;
# named generically since it's the one shared-cache root for the project.
function(deckcad_resolve_deps_dir)
    if(DEFINED CACHE{DECKCAD_DEPS_DIR})
        return()
    endif()

    if(DEFINED ENV{DECKCAD_DEPS_DIR})
        set(default_deps_dir "$ENV{DECKCAD_DEPS_DIR}")
    elseif(WIN32)
        set(default_deps_dir "$ENV{LOCALAPPDATA}/DeckCAD/deps")
    elseif(APPLE)
        set(default_deps_dir "$ENV{HOME}/Library/Caches/DeckCAD/deps")
    else()
        if(DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
            set(default_deps_dir "$ENV{XDG_CACHE_HOME}/deckcad/deps")
        else()
            set(default_deps_dir "$ENV{HOME}/.cache/deckcad/deps")
        endif()
    endif()

    set(DECKCAD_DEPS_DIR "${default_deps_dir}" CACHE PATH
        "Shared cache directory (compiler cache, etc.), reused across worktrees. Override with the DECKCAD_DEPS_DIR environment variable.")
    file(MAKE_DIRECTORY "${DECKCAD_DEPS_DIR}")
    message(STATUS "DeckCAD shared cache: ${DECKCAD_DEPS_DIR}")
endfunction()

# deckcad_enable_compiler_cache()
#
# Finds ccache or sccache and, if present, routes every compile through it
# with its cache directory pinned under DECKCAD_DEPS_DIR. The cache directory
# is passed via `cmake -E env`, not the ambient environment, so it applies
# consistently however `ninja`/`cmake --build` ends up invoked later (fresh
# shell, IDE build button, CI) rather than only when the environment variable
# happens to be set.
function(deckcad_enable_compiler_cache)
    if(NOT DECKCAD_USE_COMPILER_CACHE)
        return()
    endif()

    deckcad_resolve_deps_dir()

    find_program(DECKCAD_CCACHE_PROGRAM NAMES sccache ccache)
    if(NOT DECKCAD_CCACHE_PROGRAM)
        message(STATUS "Compiler cache: ccache/sccache not found on PATH; submodule rebuilds won't be shared across worktrees. Run scripts/setup_dev_env.py to install one.")
        return()
    endif()

    get_filename_component(cache_program_name "${DECKCAD_CCACHE_PROGRAM}" NAME_WE)
    if(cache_program_name STREQUAL "sccache")
        set(cache_dir_env_var "SCCACHE_DIR")
    else()
        set(cache_dir_env_var "CCACHE_DIR")
    endif()

    set(compiler_cache_dir "${DECKCAD_DEPS_DIR}/compiler-cache")
    file(MAKE_DIRECTORY "${compiler_cache_dir}")

    set(launcher_command
        "${CMAKE_COMMAND}" -E env "${cache_dir_env_var}=${compiler_cache_dir}"
        "${DECKCAD_CCACHE_PROGRAM}"
    )
    set(CMAKE_C_COMPILER_LAUNCHER "${launcher_command}" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${launcher_command}" PARENT_SCOPE)
    if(APPLE)
        set(CMAKE_OBJCXX_COMPILER_LAUNCHER "${launcher_command}" PARENT_SCOPE)
    endif()

    message(STATUS "Compiler cache: ${DECKCAD_CCACHE_PROGRAM} -> ${compiler_cache_dir}")
endfunction()
