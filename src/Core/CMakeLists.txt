cmake_minimum_required(VERSION 3.15)


set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(SRC_FILES
${SRC_DIR}/Core/Core.cpp
)

set(INC_DIRS
${COMMON_DIR}
${GRAPHICS_DIR}
${RAYLIB_DIR}/src
${CLAY_DIR}
${CORE_DIR}
${CORE_DIR}
)

set(LIBS_TO_LINK
)

if(HOT_RELOAD)
    set(LIB_BUILD_OPTION SHARED)
    
    list(APPEND LIBS_TO_LINK 
        GRAPHICS
    )

else()

    set(LIB_BUILD_OPTION STATIC)

endif()

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

add_library(CORE ${LIB_BUILD_OPTION} ${SRC_FILES} ${INC_DIRS})
target_include_directories(CORE PUBLIC ${INC_DIRS})
target_link_libraries(CORE ${LIBS_TO_LINK})
if(HOT_RELOAD)
    RANDOMIZE_PDB_NAME(CORE)
endif()

include(GenerateExportHeader)
generate_export_header(CORE
    EXPORT_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}/Core.export.h"
    INCLUDE_GUARD_NAME "CORE_API_H"
    EXPORT_MACRO_NAME "CORE_API"
    NO_EXPORT_MACRO_NAME "CORE_NO_API"
    DEPRECATED_MACRO_NAME "CORE_API_DEPRECATED"
    NO_DEPRECATED_MACRO_NAME "CORE_API_NO_DEPRECATED"
)