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
option(DECKCAD_ENABLE_ASAN "Build DeckCAD's own targets with AddressSanitizer" OFF)
option(DECKCAD_ENABLE_UBSAN "Build DeckCAD's own targets with UndefinedBehaviorSanitizer" OFF)

add_compile_definitions(
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX=1>
    $<$<PLATFORM_ID:Windows>:PLATFORM_WIN32=1>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_DARWIN=1>
)

set(SUBMODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Third-party submodules (SDL, FreeType, msdfgen, googletest) build as
# independent, installed external projects in a shared, commit-keyed cache
# directory outside this worktree -- see cmake/DeckCADDeps.cmake for why and
# how. Dawn is the exception: it arrives prebuilt (cmake/DeckCADDawn.cmake).
# This project's OWN compiles (and in-tree imgui) additionally go through a
# compiler cache when one is available.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeckCADDeps.cmake)
deckcad_enable_compiler_cache()

# Fail early when the submodules have not been checked out. Each
# entry names a file that only exists once the submodule is populated.
set(DECKCAD_SUBMODULE_MARKERS
    "sdl/CMakeLists.txt"
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
# Keying the install directory by variant (above) stops a stale, wrongly-linked
# SDL being *built*, but does nothing about find_package() itself: it caches
# where it found the package in SDL3_DIR, and PATHS does not override a cache
# entry that already has a value. Toggling DECKCAD_HOT_RELOAD in an existing
# build directory therefore built the shared SDL and then silently linked the
# static one anyway -- the exact mix this whole block exists to prevent. Pin it
# every configure, the same way Dawn_DIR is pinned below.
set(SDL3_DIR "${sdl_install_dir}/cmake" CACHE PATH
    "Directory holding the selected SDL3 build's SDL3Config.cmake" FORCE)
find_package(SDL3 CONFIG REQUIRED PATHS ${sdl_install_dir} NO_DEFAULT_PATH)

# --- Dawn: native WebGPU ---------------------------------------------------
# Unlike the dependencies around it, Dawn is not built from source here: it is
# downloaded as a pinned, hash-checked prebuilt from Dawn's own nightly
# releases, which are themselves just `cmake --install` prefixes in a tarball.
# cmake/DeckCADDawn.cmake explains why and what the trade-offs are.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeckCADDawn.cmake)
deckcad_provide_dawn(dawn_install_dir dawn_artifact_config)

# DawnTargets.cmake names Threads::Threads inside a $<LINK_ONLY:...>, which
# still has to resolve to a real target at generate time. Dawn's own build
# defined it as a side effect of building; consuming an install tree doesn't.
find_package(Threads REQUIRED)

# find_package() remembers where it found a package in the Dawn_DIR cache
# entry, and PATHS does not override an entry that already has a value. An
# existing build directory would therefore keep resolving whichever artifact it
# saw first even after the pinned release or the artifact configuration
# changed -- silently, and with a confusing failure much later
# ("IMPORTED_LOCATION not set for configuration Debug"). Pin it every
# configure instead of hoping the cache is current.
# Find the package config inside the unpacked prefix rather than assuming where
# it sits. Dawn's Linux artifact is built in a manylinux container, where CMake
# installs to lib64/, while the Windows and macOS artifacts use lib/ -- so a
# hardcoded lib/ path configures happily on Windows and then fails CI on Linux
# with "Could not find a package configuration file provided by Dawn".
file(GLOB deckcad_dawn_config_dirs "${dawn_install_dir}/lib*/cmake/Dawn")
if(NOT deckcad_dawn_config_dirs)
    message(FATAL_ERROR
        "No DawnConfig.cmake found under ${dawn_install_dir}. The unpacked prebuilt "
        "looks incomplete -- delete that directory under DECKCAD_DEPS_DIR and reconfigure.")
endif()
list(GET deckcad_dawn_config_dirs 0 deckcad_dawn_config_dir)
set(Dawn_DIR "${deckcad_dawn_config_dir}" CACHE PATH
    "Directory holding the prebuilt Dawn's DawnConfig.cmake" FORCE)
find_package(Dawn CONFIG REQUIRED PATHS ${dawn_install_dir} NO_DEFAULT_PATH)

# One Dawn artifact is fetched per configure, so the imported target carries
# exactly one IMPORTED_CONFIGURATION -- which is not necessarily the one being
# built (off Windows, a Debug DeckCAD deliberately uses the Release artifact;
# see cmake/DeckCADDawn.cmake). CMake would fall back to the only available
# configuration on its own, but only by guessing. Say it outright, so this
# keeps working if a configuration is added later.
foreach(deckcad_imported_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_property(TARGET dawn::webgpu_dawn
        PROPERTY MAP_IMPORTED_CONFIG_${deckcad_imported_config} ${dawn_artifact_config})
endforeach()

# The Windows artifact is built by MSVC, so DawnTargets.cmake exports a raw
# MSVC *linker* flag in its link interface: `-ignore:4221`, silencing LNK4221
# ("archive member has no public symbols"). Our link driver is clang++ with
# the GNU-style frontend, which rejects it outright --
# "clang++: error: unknown argument: '-ignore:4221'" -- because it never gets
# far enough to hand it to lld-link. The warning it suppresses is cosmetic, so
# drop the flag rather than translating it into -Xlinker form. A no-op on the
# macOS and Linux artifacts, whose interfaces carry no such flag.
get_target_property(deckcad_dawn_link_interface dawn::webgpu_dawn INTERFACE_LINK_LIBRARIES)
if(deckcad_dawn_link_interface)
    list(FILTER deckcad_dawn_link_interface EXCLUDE REGEX "-ignore:")
    set_property(TARGET dawn::webgpu_dawn
        PROPERTY INTERFACE_LINK_LIBRARIES "${deckcad_dawn_link_interface}")
endif()

if(WIN32)
    # The tarball ships webgpu_dawn.lib but not the .pdb files its objects
    # reference (they point at paths on the GitHub runner that built them), so
    # lld-link emits LNK4099 "cannot use debug info" once per object -- 825 of
    # them in a Debug link, enough to bury anything real. Nothing is lost:
    # stepping through Dawn's internals would need its sources too, which a
    # prebuilt does not carry. Silence that one warning, via CMake's LINKER:
    # prefix so it reaches the linker rather than clang's driver -- which is
    # exactly what the -ignore:4221 dropped above got wrong.
    set_property(TARGET dawn::webgpu_dawn APPEND PROPERTY
        INTERFACE_LINK_OPTIONS "LINKER:/ignore:4099")
endif()

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
        # msdfgen defaults to the *static* MSVC runtime while everything else
        # here (including CMake's own default) uses the dynamic one. Mixing the
        # two in one image is a hard link error on Windows -- lld-link's
        # /failifmismatch rejects "MTd_StaticDebug" against "MDd_DynamicDebug"
        # -- so make msdfgen agree with the rest of the build. No effect off
        # Windows, where MSVC_RUNTIME_LIBRARY means nothing.
        -DMSDFGEN_DYNAMIC_RUNTIME=ON
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

# Applies -fsanitize=... to every target added from here on: in-tree imgui
# and DeckCAD's own modules below, not the externally-built dependencies
# above (already built, and deliberately not sanitized -- see
# cmake/DeckCADSanitizers.cmake for why).
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeckCADSanitizers.cmake)
deckcad_enable_sanitizers()

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
