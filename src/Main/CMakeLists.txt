cmake_minimum_required(VERSION 3.15)

set(SRC_FILES
${SRC_DIR}/Main/Main.cpp
)

set(HEADER_FILES
${CORE_DIR}
${GRAPHICS_DIR}
)

set(LIBS_TO_LINK
GRAPHICS
)

if(NOT HOT_RELOAD)
    list(APPEND LIBS_TO_LINK
        CORE
    )
else()
    list(APPEND SRC_FILES
      LibLoader.cpp
    )
endif()

if(WIN32)
    if(HOT_RELOAD)
        list(APPEND SRC_FILES
        )
    endif()
    list(APPEND LIBS_TO_LINK
        winmm
        gdi32
    )
elseif(UNIX AND NOT APPLE)
    if(HOT_RELOAD)
        list(APPEND SRC_FILES
        )
    endif()
    list(APPEND LIBS_TO_LINK
        dl
    )
elseif(APPLE)
    if(HOT_RELOAD)
        list(APPEND SRC_FILES
        )
    endif()
    list(APPEND LIBS_TO_LINK
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OPENGL"
    )
endif()

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${HEADER_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS_TO_LINK})
