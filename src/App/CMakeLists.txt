cmake_minimum_required(VERSION 3.15)

project("App")

set(APP_DIR ${CMAKE_CURRENT_LIST_DIR})

set(APP_SRC_FILES
${APP_DIR}/App.cpp
)

set(APP_INC_DIRS
"${COMMON_DIR}"
"${GRAPHICS_DIR}"
"${RAYLIB_DIR}/src"
"${GLAZE_INC}"
"${APP_DIR}"
"${APP_DIR}/Gui"
"${APP_DIR}/Gui/Toolbox"
"${APP_DIR}/Gui/Canvas"
"${APP_DIR}/Core"
"${APP_DIR}/Core/Kernel"
"${APP_DIR}/Core/Command"
"${APP_DIR}/Core/Command/Sketch"
"${APP_DIR}/Core/Context"
"${APP_DIR}/Core/Document"
"${APP_DIR}/Core/Geometry"
"${APP_DIR}/Core/Parameter"
"${APP_DIR}/Core/Serialize"
"${APP_DIR}/Core/Tool"
)

# Set the same variable in the parent scope so main can find the files
set(APP_INC_DIRS "${APP_INC_DIRS}" PARENT_SCOPE)

set(LIBS_TO_LINK
GRAPHICS
)

if(HOT_RELOAD)
    set(LIB_BUILD_OPTION SHARED)
else()
    set(LIB_BUILD_OPTION STATIC)
endif()

if(WIN32)
    list(APPEND APP_SRC_FILES
    
    )
    list(APPEND LIBS_TO_LINK
        winmm
        gdi32
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND APP_SRC_FILES
    
    )
    list(APPEND LIBS_TO_LINK
        dl
    )
elseif(APPLE)
  list(APPEND APP_SRC_FILES
    )
    list(APPEND LIBS_TO_LINK
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OPENGL"
    )
endif()

add_library(${PROJECT_NAME} ${LIB_BUILD_OPTION} ${APP_SRC_FILES} ${APP_INC_DIRS})


target_include_directories(${PROJECT_NAME} PUBLIC ${APP_INC_DIRS})
target_link_libraries(${PROJECT_NAME} ${LIBS_TO_LINK})
# glaze's file_ops.hpp calls std::fopen, which the MSVC UCRT marks deprecated. We never
# use glaze's file helpers (we do our own atomic write), but the header is pulled in
# transitively; silence just that warning for the App TU that includes glaze.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
# Tracy profiler client (inert unless TRACY_ENABLE=ON). Plain signature to match
# the call above (CMake forbids mixing plain and keyword forms on one target).
target_link_libraries(${PROJECT_NAME} TracyClient)

if(HOT_RELOAD)
    RANDOMIZE_PDB_NAME(${PROJECT_NAME})
endif()

include(GenerateExportHeader)
# Emit into this build's ${CMAKE_BINARY_DIR}/generated dir (globally on the include
# path), not the source tree, so SHARED and STATIC builds don't clobber each other's
# export macros. See the GRAPHICS CMakeLists for the full rationale.
generate_export_header("${PROJECT_NAME}"
    EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/generated/App.export.h"
    INCLUDE_GUARD_NAME "APP_API_H"
    EXPORT_MACRO_NAME "APP_API"
    NO_EXPORT_MACRO_NAME "APP_NO_API"
    DEPRECATED_MACRO_NAME "APP_API_DEPRECATED"
    NO_DEPRECATED_MACRO_NAME "APP_API_NO_DEPRECATED"
)