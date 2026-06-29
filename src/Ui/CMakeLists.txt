cmake_minimum_required(VERSION 3.15)

project("Ui")

set(UI_DIR ${CMAKE_CURRENT_LIST_DIR})

set(UI_SRC
    ${UI_DIR}/Core/UiContext.cpp
    ${UI_DIR}/Core/Layout.cpp
    ${UI_DIR}/Core/Emit.cpp
    ${UI_DIR}/Core/Input.cpp
    ${UI_DIR}/Backend/Dispatch.cpp
    ${UI_DIR}/Backend/Raylib/RaylibBackend.cpp
    ${UI_DIR}/Backend/Raylib/IconLoader.cpp
)

set(UI_INC
    ${COMMON_DIR}
    ${UI_DIR}
    ${UI_DIR}/Core
    ${UI_DIR}/Backend
    ${UI_DIR}/Backend/Raylib
    ${UI_DIR}/Components
    ${RAYLIB_DIR}/src
)

# SVG icon rasterization via the nanosvg submodule (opt-in; default ON when present).
option(UI_ENABLE_SVG "Enable SVG icon rasterization via nanosvg" ON)
set(NANOSVG_DIR ${CMAKE_SOURCE_DIR}/submodules/nanosvg/src)
if(UI_ENABLE_SVG AND EXISTS ${NANOSVG_DIR}/nanosvg.h)
    list(APPEND UI_SRC ${UI_DIR}/Backend/Raylib/nanosvg_impl.c)
else()
    set(UI_ENABLE_SVG OFF)
endif()

add_library(UI STATIC ${UI_SRC})
target_include_directories(UI PUBLIC ${UI_INC})
target_link_libraries(UI PUBLIC raylib)

if(UI_ENABLE_SVG)
    target_compile_definitions(UI PRIVATE UI_ENABLE_SVG)
    target_include_directories(UI PRIVATE ${NANOSVG_DIR})
    message(STATUS "UI: SVG icon support enabled (nanosvg)")
else()
    message(STATUS "UI: SVG icon support disabled (PNG only)")
endif()


# Standalone demo proving the framework end-to-end without Clay.
option(BUILD_UI_DEMO "Build the standalone Ui framework demo" ON)
if(BUILD_UI_DEMO)
  set(UI_DEMO_LIBS UI raylib)
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

  add_executable(UiDemo ${UI_DIR}/Demo/UiDemo.cpp)
  target_link_libraries(UiDemo PRIVATE ${UI_DEMO_LIBS})
endif()
