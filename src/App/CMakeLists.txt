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
"${CLAY_DIR}"
"${APP_DIR}"
"${APP_DIR}/Gui/Toolbox"
"${APP_DIR}/Gui/Canvas"
"${APP_DIR}/Gui/Explorer"
"${APP_DIR}/Core"
"${APP_DIR}/Core/Kernel"
"${APP_DIR}/Core/Command"
"${APP_DIR}/Core/Geometry"
)

# Set the same variable in the parent scope so main can find the files
set(APP_INC_DIRS "${APP_INC_DIRS}" PARENT_SCOPE)

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

if(HOT_RELOAD)
    RANDOMIZE_PDB_NAME(${PROJECT_NAME})
endif()

include(GenerateExportHeader)
generate_export_header("${PROJECT_NAME}"
    EXPORT_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}/App.export.h"
    INCLUDE_GUARD_NAME "APP_API_H"
    EXPORT_MACRO_NAME "APP_API"
    NO_EXPORT_MACRO_NAME "APP_NO_API"
    DEPRECATED_MACRO_NAME "APP_API_DEPRECATED"
    NO_DEPRECATED_MACRO_NAME "APP_API_NO_DEPRECATED"
)