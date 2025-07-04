cmake_minimum_required(VERSION 3.15)
project(DeckCAD)

# Require C11 and Cpp23
set(CMAKE_C_STANDARD 11) 
set(CMAKE_CXX_STANDARD 23)

set(RAYLIB_DIR submodules/raylib/src)
set(CLAY_DIR submodules/clay)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(UI_DIR ${SRC_DIR}/UI)
set(LAYOUT_DIR ${UI_DIR}/LAYOUT)
set(STYLE_DIR ${UI_DIR}/Style)
set(WORKBENCH_DIR ${CMAKE_SOURCE_DIR}/src/Workbench)
set(EXPLORER_DIR ${WORKBENCH_DIR}/Explorer)
set(CANVAS_DIR ${WORKBENCH_DIR}/Canvas)
set(TOOLBOX_DIR ${WORKBENCH_DIR}/Toolbox)
set(SCENE_DIR ${WORKBENCH_DIR}/Scene)


set(SRC 
${SCENE_DIR}/SceneCamera.cpp
${EXPLORER_DIR}/Explorer.cpp
${CANVAS_DIR}/Canvas.cpp
${CANVAS_DIR}/RenderHandler.cpp
${TOOLBOX_DIR}/Toolbox.cpp
${LAYOUT_DIR}/ClayRender.cpp
${SRC_DIR}/main.cpp
)

set(INC 
${SRC_DIR}
${UI_DIR}
${LAYOUT_DIR}
${STYLE_DIR}
${WORKBENCH_DIR}
${EXPLORER_DIR}
${SCENE_DIR}
${CANVAS_DIR}
${TOOLBOX_DIR}
)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}") 

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