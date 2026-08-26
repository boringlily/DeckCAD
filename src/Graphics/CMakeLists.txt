# GPU context, MSDF text, ImGui integration and the 3D viewport renderers.
add_library(deckcad_graphics STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Gpu/GpuContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Text/MsdfAtlas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Text/TextRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Ui/UiContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Ui/Icons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Ui/Widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Viewport/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Viewport/GridRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Viewport/SolidRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Viewport/Viewport.cpp
)

target_include_directories(deckcad_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Gpu
    ${CMAKE_CURRENT_SOURCE_DIR}/Text
    ${CMAKE_CURRENT_SOURCE_DIR}/Ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Viewport
)

target_link_libraries(deckcad_graphics
    PUBLIC
        deckcad_common
        deckcad_platform   # asset resolution for shaders, fonts and icons
        imgui
        webgpu_dawn
    PRIVATE
        msdfgen::msdfgen-core
        msdfgen::msdfgen-ext
        nanosvg_headers
)

set_target_properties(deckcad_graphics PROPERTIES FOLDER "DeckCAD")
