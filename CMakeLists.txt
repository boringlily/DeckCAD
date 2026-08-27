cmake_minimum_required(VERSION 3.20)
if(APPLE)
    # imgui_impl_wgpu.cpp builds the Metal surface with Objective-C, so the
    # Objective-C++ toolchain has to be enabled on this platform.
    project(DeckCAD LANGUAGES C CXX OBJCXX)
else()
    project(DeckCAD LANGUAGES C CXX)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(DECKCAD_BUILD_TESTS "Build the DeckCAD unit tests" ON)
option(DECKCAD_HOT_RELOAD "Build Core as a hot-reloadable shared library" OFF)
option(DECKCAD_USE_COMPILER_CACHE "Route compiles through ccache/sccache if found, cached under DECKCAD_DEPS_DIR" ON)

add_compile_definitions(
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX=1>
    $<$<PLATFORM_ID:Windows>:PLATFORM_WIN32=1>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_DARWIN=1>
)

set(SUBMODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Third-party submodules (SDL, Dawn, FreeType, msdfgen, googletest) build as
# independent, installed external projects in a shared, commit-keyed cache
# directory outside this worktree -- see cmake/DeckCADDeps.cmake for why and
# how. This project's OWN compiles (and in-tree imgui) additionally go
# through a compiler cache when one is available.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeckCADDeps.cmake)
deckcad_enable_compiler_cache()

# Fail early and clearly when the submodules have not been checked out. Each
# entry names a file that only exists once the submodule is populated.
set(DECKCAD_SUBMODULE_MARKERS
    "sdl/CMakeLists.txt"
    "dawn/CMakeLists.txt"
    "imgui/imgui.cpp"
    "freetype/CMakeLists.txt"
    "msdfgen/CMakeLists.txt"
    "nanosvg/src/nanosvg.h"
)
if(DECKCAD_BUILD_TESTS)
    list(APPEND DECKCAD_SUBMODULE_MARKERS "googletest/CMakeLists.txt")
endif()

foreach(marker IN LISTS DECKCAD_SUBMODULE_MARKERS)
    if(NOT EXISTS "${SUBMODULES_DIR}/${marker}")
        get_filename_component(dep "${marker}" DIRECTORY)
        message(FATAL_ERROR
            "Missing submodule for '${marker}'. Run: git submodule update --init --recursive")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------

# --- SDL3: window + input --------------------------------------------------
# Hot reload needs SDL3 as a shared library: deckcad_core.dylib and DeckCAD
# both call into it, and statically linking the same SDL3 archive into two
# loaded images duplicates every Objective-C class SDL registers, which macOS
# reports as "implemented in both" and can crash. SDL3::SDL3 aliases to
# whichever of these is enabled, so consumers don't need to know which.
# The install is keyed by NAME + submodule commit, not by which CMAKE_ARGS
# were passed -- so the linkage choice has to be part of NAME too, or
# toggling DECKCAD_HOT_RELOAD on a machine that already built the other
# linkage would silently reuse a stale, wrongly-linked install.
if(DECKCAD_HOT_RELOAD)
    set(deckcad_sdl_variant "sdl-shared")
    set(deckcad_sdl_shared ON)
    set(deckcad_sdl_static OFF)
else()
    set(deckcad_sdl_variant "sdl-static")
    set(deckcad_sdl_shared OFF)
    set(deckcad_sdl_static ON)
endif()

deckcad_build_external_dependency(
    NAME ${deckcad_sdl_variant}
    SOURCE_DIR ${SUBMODULES_DIR}/sdl
    OUT_INSTALL_DIR sdl_install_dir
    CMAKE_ARGS
        "-DSDL_SHARED=${deckcad_sdl_shared}"
        "-DSDL_STATIC=${deckcad_sdl_static}"
        -DSDL_TEST_LIBRARY=OFF
        -DSDL_EXAMPLES=OFF
        -DSDL_INSTALL=ON
)
find_package(SDL3 CONFIG REQUIRED PATHS ${sdl_install_dir} NO_DEFAULT_PATH)

# --- Dawn: native WebGPU ---------------------------------------------------
# DAWN_FETCH_DEPENDENCIES lets Dawn pull its own third-party tree at configure
# time, which avoids requiring depot_tools on a contributor's machine. That
# fetch writes into submodules/dawn/third_party/ (shared across worktrees
# already, since it's inside the submodule checkout) and, like the rest of
# this build, only runs once per commit thanks to the install stamp below.
deckcad_build_external_dependency(
    NAME dawn
    SOURCE_DIR ${SUBMODULES_DIR}/dawn
    OUT_INSTALL_DIR dawn_install_dir
    CMAKE_ARGS
        -DDAWN_FETCH_DEPENDENCIES=ON
        -DDAWN_BUILD_SAMPLES=OFF
        -DDAWN_BUILD_TESTS=OFF
        -DDAWN_ENABLE_INSTALL=ON
        -DDAWN_BUILD_MONOLITHIC_LIBRARY=SHARED
        -DTINT_BUILD_TESTS=OFF
        -DTINT_BUILD_CMD_TOOLS=OFF
)
find_package(Dawn CONFIG REQUIRED PATHS ${dawn_install_dir} NO_DEFAULT_PATH)

# --- FreeType: font outlines ----------------------------------------------
deckcad_build_external_dependency(
    NAME freetype
    SOURCE_DIR ${SUBMODULES_DIR}/freetype
    OUT_INSTALL_DIR freetype_install_dir
    CMAKE_ARGS
        -DFT_DISABLE_ZLIB=ON
        -DFT_DISABLE_BZIP2=ON
        -DFT_DISABLE_PNG=ON
        -DFT_DISABLE_HARFBUZZ=ON
        -DFT_DISABLE_BROTLI=ON
)
# Use FreeType's own exported config (lowercase "freetype", matching its
# installed lib/cmake/freetype/freetype-config.cmake), not CMake's generic
# FindFreetype MODULE: on Apple platforms FreeType privately links Apple's
# HVF vector-font renderer, and only the project's own exported config
# records that dependency (confirmed by testing both -- the generic module
# leaves it out entirely and every consumer fails to link with undefined HVF
# symbols). The exported config already defines Freetype::Freetype itself.
find_package(freetype CONFIG REQUIRED PATHS ${freetype_install_dir} NO_DEFAULT_PATH)

# --- msdfgen: distance field generation ------------------------------------
# msdfgen looks for an existing Freetype::Freetype target before falling back
# to find_package() itself; passing CMAKE_PREFIX_PATH through to its own
# nested configure makes that fallback find our install the same way.
deckcad_build_external_dependency(
    NAME msdfgen
    SOURCE_DIR ${SUBMODULES_DIR}/msdfgen
    OUT_INSTALL_DIR msdfgen_install_dir
    CMAKE_ARGS
        -DMSDFGEN_CORE_ONLY=OFF
        -DMSDFGEN_BUILD_STANDALONE=OFF
        -DMSDFGEN_USE_VCPKG=OFF
        -DMSDFGEN_USE_SKIA=OFF
        -DMSDFGEN_INSTALL=ON
        # Both pull in extra third-party deps we have no use for here.
        -DMSDFGEN_DISABLE_SVG=ON
        -DMSDFGEN_DISABLE_PNG=ON
        "-DCMAKE_PREFIX_PATH=${freetype_install_dir}"
)
find_package(msdfgen CONFIG REQUIRED PATHS ${msdfgen_install_dir} NO_DEFAULT_PATH)

# --- nanosvg: SVG icon rasterization ---------------------------------------
# Consumed header-only; Ui/Icons.cpp owns the single implementation definition,
# so linking nanosvg's own library would duplicate every symbol.
add_library(nanosvg_headers INTERFACE)
target_include_directories(nanosvg_headers INTERFACE ${SUBMODULES_DIR}/nanosvg/src)

# --- Dear ImGui + its SDL3 and WebGPU backends -----------------------------
add_subdirectory(cmake/imgui)

# ---------------------------------------------------------------------------
# DeckCAD modules
# ---------------------------------------------------------------------------
add_subdirectory(${SRC_DIR}/Common)
add_subdirectory(${SRC_DIR}/Platform)
add_subdirectory(${SRC_DIR}/Graphics)
add_subdirectory(${SRC_DIR}/Core)
add_subdirectory(${SRC_DIR}/Main)

if(DECKCAD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
