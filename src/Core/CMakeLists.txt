# Application logic and panels.
#
# Holds no GPU handles of its own: every renderer arrives through FrameContext.
# That constraint is what keeps the hot-reload boundary tractable: Main can
# swap this library out at runtime without any GPU handle going stale.
set(DECKCAD_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AppState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CoreApi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Panels/HeaderBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Panels/ExplorerPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Panels/ViewportPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Panels/ToolboxPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Panels/StatusBar.cpp
)

if(DECKCAD_HOT_RELOAD)
    add_library(deckcad_core SHARED ${DECKCAD_CORE_SOURCES})
    target_compile_definitions(deckcad_core PRIVATE DECKCAD_CORE_SHARED)
else()
    add_library(deckcad_core STATIC ${DECKCAD_CORE_SOURCES})
endif()

target_include_directories(deckcad_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(deckcad_core PUBLIC
    deckcad_common
    deckcad_graphics
    imgui_core   # widget drawing only; never the SDL3/Dawn-linked backends
)

if(DECKCAD_HOT_RELOAD AND APPLE)
    # As a shared library, deckcad_core must resolve every symbol itself,
    # including the ones deckcad_graphics only needs because it drives the
    # Metal-backed ImGui WebGPU backend.
    target_link_libraries(deckcad_core PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework IOKit"
    )
endif()

set_target_properties(deckcad_core PROPERTIES FOLDER "DeckCAD")
