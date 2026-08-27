# Builds third-party submodules as independent, installed external projects
# in a shared, commit-keyed cache directory outside this worktree, so a
# second worktree (or a fresh clone/CI runner) building the same submodule
# commit consumes the already-built, already-installed library directly via
# find_package() -- zero rebuilding, not just fast recompiles.
#
# Why not just add_subdirectory() at a shared *binary* directory: CMake's
# Ninja generator keeps its one incremental-build log (.ninja_log,
# .ninja_deps) only in the top-level build directory, so pointing
# add_subdirectory() at a shared binary dir does NOT get you cross-worktree
# build avoidance -- a fresh worktree's Ninja has no memory of having built
# those objects before, even if they're sitting on disk unused. Building each
# dependency as a genuinely separate `cmake -S/-B` invocation with its own
# install step sidesteps that: the installed .a/.so/.dylib and headers are
# just files on disk, and find_package() against them doesn't care which
# Ninja log (if any) produced them.
#
# Trade-off worth knowing: deckcad_build_external_dependency() runs each
# dependency's configure+build+install synchronously, one at a time, during
# the MAIN project's own configure step (via execute_process). That means a
# true cold start (first time ever on a machine, for a given submodule
# commit) does NOT parallelize across dependencies the way the old
# single-Ninja-graph add_subdirectory() approach did -- SDL, Dawn, FreeType
# and msdfgen now build one after another instead of concurrently. Every
# subsequent configure (any worktree, same commit) skips straight past
# already-installed dependencies, which is the case this is optimized for.

# Resolves DECKCAD_DEPS_DIR once: $ENV{DECKCAD_DEPS_DIR} if set, else a
# per-OS default cache location. Exposed as a cache variable so it shows up
# in `cmake -L` and IDE tooling.
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
        "Shared cache directory (external dependency installs, compiler cache), reused across worktrees. Override with the DECKCAD_DEPS_DIR environment variable.")
    file(MAKE_DIRECTORY "${DECKCAD_DEPS_DIR}")
    message(STATUS "DeckCAD shared cache: ${DECKCAD_DEPS_DIR}")
endfunction()

# Finds ccache/sccache once and reports back the program path and which
# environment variable controls its cache directory. Shared by
# deckcad_enable_compiler_cache() (this project's own compiles) and
# deckcad_build_external_dependency() (each nested dependency build), so a
# ccache hit in one benefits the other too.
function(deckcad_resolve_compiler_cache out_program out_env_var)
    if(NOT DECKCAD_USE_COMPILER_CACHE)
        set(${out_program} "" PARENT_SCOPE)
        return()
    endif()

    find_program(DECKCAD_CCACHE_PROGRAM NAMES sccache ccache)
    if(NOT DECKCAD_CCACHE_PROGRAM)
        set(${out_program} "" PARENT_SCOPE)
        return()
    endif()

    get_filename_component(cache_program_name "${DECKCAD_CCACHE_PROGRAM}" NAME_WE)
    if(cache_program_name STREQUAL "sccache")
        set(${out_env_var} "SCCACHE_DIR" PARENT_SCOPE)
    else()
        set(${out_env_var} "CCACHE_DIR" PARENT_SCOPE)
    endif()
    set(${out_program} "${DECKCAD_CCACHE_PROGRAM}" PARENT_SCOPE)
endfunction()

# deckcad_enable_compiler_cache()
#
# Routes this project's OWN compiles (DeckCAD's sources, plus in-tree imgui)
# through ccache/sccache, cache directory pinned under DECKCAD_DEPS_DIR via
# `cmake -E env` rather than the ambient environment, so it applies
# consistently however `ninja`/`cmake --build` ends up invoked later (fresh
# shell, IDE build button, CI).
function(deckcad_enable_compiler_cache)
    deckcad_resolve_deps_dir()
    deckcad_resolve_compiler_cache(cache_program cache_dir_env_var)
    if(NOT cache_program)
        message(STATUS "Compiler cache: ccache/sccache not found on PATH. Run scripts/setup_dev_env.py to install one.")
        return()
    endif()

    set(compiler_cache_dir "${DECKCAD_DEPS_DIR}/compiler-cache")
    file(MAKE_DIRECTORY "${compiler_cache_dir}")

    set(launcher_command
        "${CMAKE_COMMAND}" -E env "${cache_dir_env_var}=${compiler_cache_dir}"
        "${cache_program}"
    )
    set(CMAKE_C_COMPILER_LAUNCHER "${launcher_command}" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${launcher_command}" PARENT_SCOPE)
    if(APPLE)
        set(CMAKE_OBJCXX_COMPILER_LAUNCHER "${launcher_command}" PARENT_SCOPE)
    endif()

    message(STATUS "Compiler cache: ${cache_program} -> ${compiler_cache_dir}")
endfunction()

# deckcad_build_external_dependency(
#     NAME <name>
#     SOURCE_DIR <path>
#     OUT_INSTALL_DIR <var>
#     [CMAKE_ARGS <arg>...]
# )
#
# Configures, builds and installs the CMake project at SOURCE_DIR into a
# directory under DECKCAD_DEPS_DIR keyed by NAME and the commit currently
# checked out in SOURCE_DIR (e.g. ".../deps/sdl-402fc52af4e7/install"), then
# sets OUT_INSTALL_DIR to that install directory. If a previous run (this
# worktree or another one) already installed that exact commit, this returns
# immediately without reconfiguring or rebuilding anything.
#
# Runs synchronously as part of THIS project's configure step (via
# execute_process, not CMake's ExternalProject_Add build-time custom
# commands), so a plain `cmake --preset Debug` followed by `cmake --build`
# still works as a single, ordinary two-step build -- callers don't need to
# know a nested build happened, except that the first configure for a new
# commit takes as long as that dependency's own build does.
function(deckcad_build_external_dependency)
    cmake_parse_arguments(arg "" "NAME;SOURCE_DIR;OUT_INSTALL_DIR" "CMAKE_ARGS" ${ARGN})
    if(NOT arg_NAME OR NOT arg_SOURCE_DIR OR NOT arg_OUT_INSTALL_DIR)
        message(FATAL_ERROR "deckcad_build_external_dependency: NAME, SOURCE_DIR and OUT_INSTALL_DIR are required")
    endif()

    deckcad_resolve_deps_dir()

    execute_process(
        COMMAND git -C "${arg_SOURCE_DIR}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE submodule_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE git_result
    )
    if(NOT git_result EQUAL 0 OR submodule_commit STREQUAL "")
        message(FATAL_ERROR "Could not determine the checked-out commit for '${arg_SOURCE_DIR}' (git rev-parse failed). Is the submodule initialized?")
    endif()

    # CMAKE_BUILD_TYPE is part of the cache key, not just a CMAKE_ARGS value:
    # a Release DeckCAD linking against a Debug-built static dependency (or
    # vice versa) is exactly the kind of silent mismatch this is supposed to
    # prevent, not cause.
    string(TOLOWER "${CMAKE_BUILD_TYPE}" deckcad_build_type_lower)
    if(NOT deckcad_build_type_lower)
        set(deckcad_build_type_lower "default")
    endif()
    set(dep_root "${DECKCAD_DEPS_DIR}/${arg_NAME}-${deckcad_build_type_lower}-${submodule_commit}")
    set(install_dir "${dep_root}/install")
    set(stamp_file "${dep_root}/.deckcad-installed")

    if(EXISTS "${stamp_file}")
        message(STATUS "External dependency '${arg_NAME}': already installed (commit ${submodule_commit}) at ${install_dir}")
        set(${arg_OUT_INSTALL_DIR} "${install_dir}" PARENT_SCOPE)
        return()
    endif()

    message(STATUS "External dependency '${arg_NAME}': building + installing (commit ${submodule_commit}, first time on this machine)...")

    set(build_dir "${dep_root}/build")
    deckcad_resolve_compiler_cache(cache_program cache_dir_env_var)

    set(configure_command "${CMAKE_COMMAND}")
    if(cache_program)
        set(compiler_cache_dir "${DECKCAD_DEPS_DIR}/compiler-cache")
        file(MAKE_DIRECTORY "${compiler_cache_dir}")
        set(configure_command "${CMAKE_COMMAND}" -E env "${cache_dir_env_var}=${compiler_cache_dir}" "${CMAKE_COMMAND}")
    endif()

    execute_process(
        COMMAND ${configure_command}
            -S "${arg_SOURCE_DIR}"
            -B "${build_dir}"
            -G "${CMAKE_GENERATOR}"
            "-DCMAKE_INSTALL_PREFIX=${install_dir}"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
            ${arg_CMAKE_ARGS}
        RESULT_VARIABLE configure_result
    )
    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR "External dependency '${arg_NAME}': configure failed (see output above). "
            "Delete ${dep_root} to force a clean retry.")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --parallel
        RESULT_VARIABLE build_result
    )
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "External dependency '${arg_NAME}': build failed (see output above). "
            "Delete ${dep_root} to force a clean retry.")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${build_dir}"
        RESULT_VARIABLE install_result
    )
    if(NOT install_result EQUAL 0)
        message(FATAL_ERROR "External dependency '${arg_NAME}': install failed (see output above). "
            "Delete ${dep_root} to force a clean retry.")
    endif()

    file(WRITE "${stamp_file}" "${submodule_commit}\n")
    message(STATUS "External dependency '${arg_NAME}': installed to ${install_dir}")
    set(${arg_OUT_INSTALL_DIR} "${install_dir}" PARENT_SCOPE)
endfunction()
