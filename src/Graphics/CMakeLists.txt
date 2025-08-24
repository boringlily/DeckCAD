cmake_minimum_required(VERSION 3.15)

if(HOT_RELOAD)
set(LIB_BUILD_OPTION SHARED)
else()
set(LIB_BUILD_OPTION STATIC)
endif()



set(SRC_FILES
${GRAPHICS_DIR}/Graphics.cpp
)

set(HEADER_FILES
${COMMON_DIR}
${GRAPHICS_DIR}
${CLAY_DIR}
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

add_library(GRAPHICS ${LIB_BUILD_OPTION} ${SRC_FILES} ${HEADER_FILES})
target_include_directories(GRAPHICS PUBLIC ${HEADER_FILES})
target_link_libraries(GRAPHICS PUBLIC ${LIBS_TO_LINK})

include(GenerateExportHeader)
generate_export_header(GRAPHICS
    EXPORT_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}/Graphics.export.h"
    INCLUDE_GUARD_NAME "GRAPHICS_API_H"
    EXPORT_MACRO_NAME "GRAPHICS_API"
    NO_EXPORT_MACRO_NAME "GRAPHICS_NO_API"
    DEPRECATED_MACRO_NAME "GRAPHICS_API_DEPRECATED"
    NO_DEPRECATED_MACRO_NAME "GRAPHICS_API_NO_DEPRECATED"
)
