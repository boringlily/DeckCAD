cmake_minimum_required(VERSION 3.15)
project(DeckCAD)

# Require C11 and Cpp23
set(CMAKE_C_STANDARD 11) 
set(CMAKE_CXX_STANDARD 23)

set(RAYLIB_DIR submodules/raylib/src)
set(CLAY_DIR submodules/clay)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(WORKBENCH_DIR ${CMAKE_SOURCE_DIR}/src/Workbench)
set(EXPLORER_DIR ${WORKBENCH_DIR}/Explorer)
set(CANVAS_DIR ${WORKBENCH_DIR}/Canvas)
set(TOOLBOX_DIR ${WORKBENCH_DIR}/Toolbox)
set(SCENE_DIR ${WORKBENCH_DIR}/Scene)


set(SRC 
${SRC_DIR}/main.cpp
${EXPLORER_DIR}/Explorer.cpp
${CANVAS_DIR}/Canvas.cpp
${CANVAS_DIR}/RenderHandler.cpp
${TOOLBOX_DIR}/Toolbox.cpp
)

set(INC 
${SRC_DIR}
${WORKBENCH_DIR}
${EXPLORER_DIR}
${SCENE_DIR}
${CANVAS_DIR}
${TOOLBOX_DIR}
)

add_executable(${PROJECT_NAME} ${SRC} ${INC})
target_include_directories(${PROJECT_NAME} PUBLIC ${INC})

target_include_directories(${PROJECT_NAME} PRIVATE ${CLAY_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_DIR})

add_subdirectory(submodules/raylib/projects/CMake)

target_link_libraries(${PROJECT_NAME} raylib)

# Checks if OSX and links appropriate frameworks (only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()