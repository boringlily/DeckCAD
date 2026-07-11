cmake_minimum_required(VERSION 3.15)

# Single project for everything the app draws with: the window/frame lifecycle
# and asset loading (Graphics.cpp, Style.h) plus the Ui flexbox framework it
# hosts (Core/, Backend/, Components/) - one directory, one library, one export
# header. Previously Ui was its own separate library that Graphics linked; they
# are merged here since neither is independently hot-reloaded (only App.dll is -
# see Main/LibLoader), so the split bought no isolation, just an extra DLL
# boundary and a second export-macro/include-dir setup to keep in sync.
if(HOT_RELOAD)
set(LIB_BUILD_OPTION SHARED)
else()
set(LIB_BUILD_OPTION STATIC)
endif()

set(SRC_FILES
${GRAPHICS_DIR}/Graphics.cpp
${GRAPHICS_DIR}/Core/UiContext.cpp
${GRAPHICS_DIR}/Core/Layout.cpp
${GRAPHICS_DIR}/Core/Emit.cpp
${GRAPHICS_DIR}/Core/Input.cpp
${GRAPHICS_DIR}/Backend/Dispatch.cpp
${GRAPHICS_DIR}/Backend/Raylib/RaylibBackend.cpp
${GRAPHICS_DIR}/Backend/Raylib/IconLoader.cpp
)

set(HEADER_FILES
${COMMON_DIR}
${GRAPHICS_DIR}
${GRAPHICS_DIR}/Core
${GRAPHICS_DIR}/Backend
${GRAPHICS_DIR}/Backend/Raylib
${GRAPHICS_DIR}/Components
${RAYLIB_DIR}/src
)

set(LIBS_TO_LINK
raylib
)

if(WIN32)
    list(APPEND SRC_FILES
        # platform_win32.cpp
    )
    list(APPEND LIBS_TO_LINK
        winmm
        gdi32
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND SRC_FILES
        # platform_linux.cpp
    )
    list(APPEND LIBS_TO_LINK
        dl
    )
elseif(APPLE)
  list(APPEND SRC_FILES
        # platform_apple.cpp
    )
    list(APPEND LIBS_TO_LINK
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OPENGL"
    )
endif()

# SVG icon rasterization via the nanosvg submodule (opt-in; default ON when present).
option(UI_ENABLE_SVG "Enable SVG icon rasterization via nanosvg" ON)
set(NANOSVG_DIR ${CMAKE_SOURCE_DIR}/submodules/nanosvg/src)
if(UI_ENABLE_SVG AND EXISTS ${NANOSVG_DIR}/nanosvg.h)
    list(APPEND SRC_FILES ${GRAPHICS_DIR}/Backend/Raylib/nanosvg_impl.c)
else()
    set(UI_ENABLE_SVG OFF)
endif()

add_library(GRAPHICS ${LIB_BUILD_OPTION} ${SRC_FILES} ${HEADER_FILES})
target_include_directories(GRAPHICS PUBLIC ${HEADER_FILES})
target_link_libraries(GRAPHICS PUBLIC ${LIBS_TO_LINK})
# Tracy profiler client (inert unless TRACY_ENABLE=ON); PUBLIC so consumers of
# GRAPHICS inherit the <tracy/Tracy.hpp> include path.
target_link_libraries(GRAPHICS PUBLIC TracyClient)

if(UI_ENABLE_SVG)
    target_compile_definitions(GRAPHICS PRIVATE UI_ENABLE_SVG)
    target_include_directories(GRAPHICS PRIVATE ${NANOSVG_DIR})
    message(STATUS "GRAPHICS: SVG icon support enabled (nanosvg)")
else()
    message(STATUS "GRAPHICS: SVG icon support disabled (PNG only)")
endif()

include(GenerateExportHeader)
# Emit into this build's ${CMAKE_BINARY_DIR}/generated dir (globally on the include
# path), not the source tree, so SHARED and STATIC builds don't clobber each other's
# export macros.
generate_export_header(GRAPHICS
    EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/generated/Graphics.export.h"
    INCLUDE_GUARD_NAME "GRAPHICS_API_H"
    EXPORT_MACRO_NAME "GRAPHICS_API"
    NO_EXPORT_MACRO_NAME "GRAPHICS_NO_API"
    DEPRECATED_MACRO_NAME "GRAPHICS_API_DEPRECATED"
    NO_DEPRECATED_MACRO_NAME "GRAPHICS_API_NO_DEPRECATED"
)

# Standalone demo proving the Ui framework end-to-end, independent of the rest
# of the app.
option(BUILD_UI_DEMO "Build the standalone Ui framework demo" ON)
if(BUILD_UI_DEMO)
  set(UI_DEMO_LIBS GRAPHICS)
  if(WIN32)
    list(APPEND UI_DEMO_LIBS winmm gdi32)
  elseif(UNIX AND NOT APPLE)
    list(APPEND UI_DEMO_LIBS dl)
  elseif(APPLE)
    list(APPEND UI_DEMO_LIBS
            "-framework IOKit"
            "-framework Cocoa"
            "-framework OPENGL"
        )
  endif()

  add_executable(UiDemo ${GRAPHICS_DIR}/Demo/UiDemo.cpp)
  target_link_libraries(UiDemo PRIVATE ${UI_DEMO_LIBS})
  # --shot screenshots go to a dedicated dir under the build tree, not the CWD.
  target_compile_definitions(UiDemo PRIVATE UIDEMO_SHOT_DIR="${CMAKE_BINARY_DIR}/screenshots")
endif()
