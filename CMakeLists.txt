cmake_minimum_required(VERSION 3.15)
project("DeckCAD")

# Custom UI Settings.
set(BUILD_UI_DEMO ON)

# Require C11 and Cpp23
set(CMAKE_C_STANDARD 11) 
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}/bin") 
# Global compile definitions.
add_compile_definitions(
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX=1>
    $<$<PLATFORM_ID:Windows>:PLATFORM_WIN32=1>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_DARWIN=1>
)


option(HOT_RELOAD "Enable a dynamic build with Runtime loading of the application Core Logic" OFF)

# ---- Tracy profiler (optional) -----------------------------------------------
# The TracyClient target and its headers are linked unconditionally so sources
# can #include <tracy/Tracy.hpp> and use ZoneScoped/FrameMark in every build; the
# macros stay completely inert unless TRACY_ENABLE is ON. Turn it on with the
# "Profile" preset (or -DTRACY_ENABLE=ON). TRACY_ON_DEMAND keeps the client idle
# until a Tracy server actually connects, so an instrumented build still runs at
# full speed when nobody is profiling.
option(TRACY_ENABLE "Enable Tracy profiler instrumentation" ON)
option(TRACY_ON_DEMAND "Tracy: only collect data while a server is connected" ON)
# CRITICAL on Windows: disable Tracy's ETW system tracing (CPU sampling / context
# switches / GPU VSync capture). It needs admin and, when the app is force-killed
# instead of exiting cleanly, LEAKS a kernel ETW session ("NT Kernel Logger" +
# "TracyVsync") that keeps tracing the GPU driver's present path (DxgKrnl VSync)
# with a dead consumer — wedging the GPU for EVERY app (even non-Tracy builds,
# raylib's example.exe, etc.) until the session is stopped or the machine reboots.
# We only need zones/FrameMark, so keep systrace off. FORCEd (not option()) so a
# stale cache value from Tracy's own set_option can't silently re-enable the wedge.
# See TracySysTrace.cpp: StartSingletonKernelLoggerSession + "TracyVsync".
set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "Disable Tracy ETW system tracing (avoids the force-kill GPU wedge)" FORCE)
# Builds the Tracy profiler GUI (the "server") from source as the `tracy-server`
# target. Heavy (pulls glfw/freetype/capstone/imgui/... via CPM) so it is OFF by
# default and EXCLUDE_FROM_ALL even when ON — build it explicitly with
# `cmake --build <dir> --target tracy-server`. Wired up at the bottom of this file.
option(TRACY_BUILD_SERVER "Provide a 'tracy-server' target that builds the Tracy profiler GUI" OFF)

SET(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CUSTOMIZE_BUILD ON)
set(BUILD_EXAMPLES OFF)
set(WITH_PIC OFF)
set(BUILD_SHARED_LIBS OFF)
set(SUPPORT_MODULE_RAUDIO OFF)
set(SUPPORT_FILEFORMAT_BMP ON)
# raylib's own CMakeCache entries for these are STICKY: once a build directory's
# cache is first populated (e.g. an old configure, or an old raylib release tag
# fetched via projects/CMake's FetchContent), later reconfigures never reset them
# even after CMakeLists.txt/the raylib pin changes — only a deleted build dir
# would. A build directory (`build/hotreload`) was found with SUPPORT_CUSTOM_
# FRAME_CONTROL stuck ON from stale cache: raylib's EndDrawing() then skips BOTH
# SwapScreenBuffer() AND PollInputEvents() entirely (see rcore.c), so the app
# renders (Tracy zones/FrameMark keep firing, GPU 3D engine stays busy) but never
# presents a frame or pumps its window messages — Windows shows a white "Not
# Responding" ghost window while everything looks fine in a profiler. Setting
# these as plain (non-CACHE) variables here, like the options above, shadows any
# stale cached value so raylib's own option() calls can't silently disagree.
set(SUPPORT_CUSTOM_FRAME_CONTROL OFF)
set(SUPPORT_BUSY_WAIT_LOOP OFF)


if(HOT_RELOAD)
add_compile_definitions(__HOT_RELOAD_ENABLED__)
set(WITH_PIC ON)
set(BUILD_SHARED_LIBS ON)
endif()


set(RAYLIB_DIR "${CMAKE_SOURCE_DIR}/submodules/raylib")
set(TRACY_DIR "${CMAKE_SOURCE_DIR}/submodules/tracy")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

set(COMMON_DIR "${SRC_DIR}/Common")
set(GRAPHICS_DIR "${SRC_DIR}/Graphics")
set(UI_DIR "${SRC_DIR}/Ui")
set(APP_DIR "${SRC_DIR}/App")

set(APP_INC_DIRS "")

set(MAIN_DIR ${SRC_DIR}/Main)

# This custom target's job is to configure cmake each time we compile, this is only useful for hot-reloading feature.
if(HOT_RELOAD)
add_custom_target(hot_reload ALL
        # COMMAND cd .. / cmake -B build
    )
endif()

# Macro used to append a random string to the target's pdb name.
macro(RANDOMIZE_PDB_NAME target)
    string(RANDOM random_string)
    set(pdbname "${target}_")
    string(APPEND pdbname ${random_string})
    set_target_properties(${target}
        PROPERTIES
        COMPILE_PDB_NAME ${pdbname}
        PDB_NAME ${pdbname}
    )
endmacro()

add_subdirectory(${RAYLIB_DIR}/projects/CMake)

# Tracy's client must be exactly ONE instance shared by everything that emits
# zones. In the static build that's the single Main.exe (static lib is fine). In a
# HOT_RELOAD build the app is split across Main.exe + GRAPHICS.dll + App.dll, so
# TracyClient has to be a shared DLL they all import: TRACY_STATIC=OFF makes Tracy
# emit dllexport/dllimport (TRACY_EXPORTS/TRACY_IMPORTS) and build TracyClient.dll.
# If it were static there, each module would link its own Tracy instance and they
# would fight over the profiling socket — which breaks/crashes a capture. Force it
# to track BUILD_SHARED_LIBS so a stale cache can't silently reintroduce that bug.
if(BUILD_SHARED_LIBS)
    set(TRACY_STATIC OFF CACHE BOOL "Tracy client links as a shared library" FORCE)
else()
    set(TRACY_STATIC ON CACHE BOOL "Tracy client links as a static library" FORCE)
endif()
add_subdirectory(${TRACY_DIR})
add_subdirectory(${COMMON_DIR})

# generate_export_header (UI/GRAPHICS/App) emits *.export.h into this per-build dir.
# Made visible to every target so any source can #include "<Name>.export.h" even when
# it uses another module's header without linking it (e.g. Main -> App.h under hot
# reload, App.cpp -> Graphics.h in the static build). Keeping the generated headers
# per-build (not in the source tree) stops build/static and build/hotreload from
# clobbering each other's export macros. Must precede the module subdirectories.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
include_directories(${CMAKE_BINARY_DIR}/generated)

# UI before GRAPHICS/APP: both link against it (the sole UI layer since the Clay teardown).
add_subdirectory(${UI_DIR})
add_subdirectory(${GRAPHICS_DIR})
add_subdirectory(${APP_DIR})
add_subdirectory(${MAIN_DIR})

# ---- Unit tests (Google Test) ------------------------------------------------
# Built by default; runs via CTest (the "Test" test preset, or `ctest` in the
# build dir). Turn off with -DBUILD_TESTS=OFF to skip building googletest.
option(BUILD_TESTS "Build the unit tests (Google Test)" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# ---- Tracy profiler GUI / server ---------------------------------------------
# The server is a standalone CMake project that uses C++ exceptions and pulls a
# large vendored dependency set (glfw, freetype, capstone, imgui, libcurl, ...)
# via CPM. We build it as an isolated ExternalProject rather than add_subdirectory
# so it gets its own fresh configure and is NOT contaminated by this project's
# global -fno-exceptions / C++23 flags. Its deps are cached under .cpm-cache/ so a
# rebuild doesn't re-download them. Build on demand:
#     cmake --build build-profile --target tracy-server
# which leaves the GUI at  build-profile/bin/tracy-profiler(.exe).
if(TRACY_BUILD_SERVER)
    include(ExternalProject)

    set(TRACY_SERVER_EXE "tracy-profiler${CMAKE_EXECUTABLE_SUFFIX}")
    set(TRACY_CPM_CACHE "${CMAKE_SOURCE_DIR}/.cpm-cache")

    set(TRACY_SERVER_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCPM_SOURCE_CACHE=${TRACY_CPM_CACHE}
        # NO_ISA_EXTENSIONS drops -march=native / /arch:AVX2 (avoids requiring AVX2 at
        # runtime); NO_LTO skips ThinLTO — slow across the whole vendored dep set and
        # pointless for a dev-tool GUI.
        -DNO_ISA_EXTENSIONS=ON
        -DNO_LTO=ON
        # Force every pkg-config-detectable dep to build from source. Otherwise Tracy
        # picks up msys64's mingw system libs (e.g. freetype), whose headers are
        # incompatible with our MSVC-targeting clang-cl and break the build.
        -DDOWNLOAD_CAPSTONE=ON
        -DDOWNLOAD_GLFW=ON
        -DDOWNLOAD_FREETYPE=ON
        -DDOWNLOAD_PUGIXML=ON
        -DDOWNLOAD_LIBCURL=ON
        # freetype otherwise auto-detects msys64's system zlib/png/brotli and drags
        # C:/msys64/mingw64/include onto every compile line (as -imsvc); those mingw
        # headers don't parse under clang-cl. Use freetype's bundled copies instead.
        -DFT_DISABLE_ZLIB=ON
        -DFT_DISABLE_BZIP2=ON
        -DFT_DISABLE_PNG=ON
        -DFT_DISABLE_HARFBUZZ=ON
        -DFT_DISABLE_BROTLI=ON
        # Same problem for libcurl: it auto-detects msys64's zlib, which pulls mingw's
        # winsock2.h/windows.h (and a broken x86intrin chain) ahead of the MSVC SDK.
        # Tracy only needs plain HTTP, so drop zlib.
        -DCURL_ZLIB=OFF
        # Belt-and-suspenders: keep find_package away from the msys64 prefix so no
        # other vendored dep picks up its mingw headers/libs either.
        -DCMAKE_IGNORE_PREFIX_PATH=C:/msys64/mingw64
        # base64 builds its own SIMD codepaths with -Werror, which clang 20 trips on
        # (implicit conversions, switch-default, ...). Not our code — turn it off.
        -DBASE64_WERROR=OFF
    )
    # Tracy's Windows build (and vendored deps like capstone) assume an MSVC-style
    # compiler — their non-MSVC branches hardcode -Werror and GNU-only flags that our
    # GNU-driver clang++ trips over. Build the server with clang-cl (same LLVM 20,
    # MSVC driver) so CMake reports MSVC=TRUE and those branches are skipped; pair it
    # with llvm-rc for the Win32 .rc icon/manifest. Elsewhere, reuse the project
    # compilers as-is.
    if(WIN32)
        get_filename_component(_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        # clang-cl links MSVC-style, so CMake wants the manifest tool (mt.exe) and a
        # resource compiler. Outside a VS developer shell those aren't on PATH, so
        # point at LLVM's drop-in equivalents (llvm-mt / llvm-rc) explicitly.
        list(APPEND TRACY_SERVER_CMAKE_ARGS
            -DCMAKE_C_COMPILER=${_llvm_bin}/clang-cl.exe
            -DCMAKE_CXX_COMPILER=${_llvm_bin}/clang-cl.exe
            -DCMAKE_RC_COMPILER=${_llvm_bin}/llvm-rc.exe
            -DCMAKE_MT=${_llvm_bin}/llvm-mt.exe
            # Tracy adds the MSVC flag /MP globally; clang-cl ignores it and warns
            # (-Wunused-command-line-argument), which some deps (e.g. base64) escalate
            # to an error via -Werror. Disable just that warning so the ignored /MP is
            # harmless. *_FLAGS_INIT appends to the platform defaults (keeps /EHsc etc).
            -DCMAKE_C_FLAGS_INIT=-Wno-unused-command-line-argument
            -DCMAKE_CXX_FLAGS_INIT=-Wno-unused-command-line-argument
        )
    else()
        list(APPEND TRACY_SERVER_CMAKE_ARGS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        )
    endif()

    ExternalProject_Add(tracy-server
        SOURCE_DIR "${TRACY_DIR}/profiler"
        PREFIX "${CMAKE_BINARY_DIR}/tracy-server"
        CMAKE_GENERATOR "Ninja"
        CMAKE_ARGS ${TRACY_SERVER_CMAKE_ARGS}
        BUILD_BYPRODUCTS "<BINARY_DIR>/${TRACY_SERVER_EXE}"
        # Drop the finished GUI next to our other binaries instead of installing.
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "<BINARY_DIR>/${TRACY_SERVER_EXE}"
            "${CMAKE_BINARY_DIR}/bin/${TRACY_SERVER_EXE}"
        EXCLUDE_FROM_ALL TRUE
    )
endif()
