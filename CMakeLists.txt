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

add_compile_definitions(
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX=1>
    $<$<PLATFORM_ID:Windows>:PLATFORM_WIN32=1>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_DARWIN=1>
)

set(SUBMODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
if(DECKCAD_HOT_RELOAD)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${SUBMODULES_DIR}/sdl EXCLUDE_FROM_ALL)

# --- Dawn: native WebGPU ---------------------------------------------------
# DAWN_FETCH_DEPENDENCIES lets Dawn pull its own third-party tree at configure
# time, which avoids requiring depot_tools on a contributor's machine.
set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY SHARED CACHE STRING "" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(${SUBMODULES_DIR}/dawn EXCLUDE_FROM_ALL)

# --- FreeType: font outlines ----------------------------------------------
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory(${SUBMODULES_DIR}/freetype EXCLUDE_FROM_ALL)

# msdfgen looks for an existing Freetype::Freetype target before falling back to
# find_package(). FreeType's in-tree build does not define that alias itself, so
# without this it would silently link a system FreeType instead of ours.
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

# --- msdfgen: distance field generation ------------------------------------
set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)
# Both pull in extra third-party deps we have no use for here.
set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
add_subdirectory(${SUBMODULES_DIR}/msdfgen EXCLUDE_FROM_ALL)

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
