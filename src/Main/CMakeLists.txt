cmake_minimum_required(VERSION 3.15)

project("Main")

set(MAIN_SRC_FILES
${SRC_DIR}/Main/Main.cpp
)

set(MAIN_INC_DIRS
"${APP_INC_DIRS}"
"${GRAPHICS_DIR}"
)

set(LIBS_TO_LINK
GRAPHICS
)

if(NOT HOT_RELOAD)
    list(APPEND LIBS_TO_LINK
        App
    )
else()
    list(APPEND MAIN_SRC_FILES
      LibLoader.cpp
    )
endif()

if(WIN32)
    if(HOT_RELOAD)
        list(APPEND MAIN_SRC_FILES
        )
    endif()
    list(APPEND LIBS_TO_LINK
        winmm
        gdi32
    )
elseif(UNIX AND NOT APPLE)
    if(HOT_RELOAD)
        list(APPEND MAIN_SRC_FILES
        )
    endif()
    list(APPEND LIBS_TO_LINK
        dl
    )
elseif(APPLE)
    if(HOT_RELOAD)
        list(APPEND MAIN_SRC_FILES
        )
    endif()
    list(APPEND LIBS_TO_LINK
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OPENGL"
    )
endif()

add_executable(${PROJECT_NAME} ${MAIN_SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${MAIN_INC_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS_TO_LINK})
