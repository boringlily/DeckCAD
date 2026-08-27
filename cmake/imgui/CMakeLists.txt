# Dear ImGui does not ship a CMakeLists, so the library and the two backends we
# use are compiled here.
#
# Split in two so deckcad_core (widget-drawing only) never pulls in SDL3 or
# Dawn: when it's built as a hot-reloadable shared library, statically linking
# the same SDL3 archive into both it and the main executable duplicates every
# Objective-C class SDL registers, which macOS reports as "implemented in
# both" and can crash.
set(IMGUI_DIR ${SUBMODULES_DIR}/imgui)

add_library(imgui_core STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
)

target_include_directories(imgui_core PUBLIC ${IMGUI_DIR})

set_target_properties(imgui_core PROPERTIES FOLDER "ThirdParty")

# The SDL3 and WebGPU backends: only linked into modules that own GPU/window
# state (Graphics, Platform), never into Core.
add_library(imgui STATIC
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
)

if(APPLE)
    # On macOS this file creates the CAMetalLayer the WebGPU surface binds to,
    # which is Objective-C; compiling it as plain C++ fails in the SDK headers.
    set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
        PROPERTIES LANGUAGE OBJCXX)
endif()

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Selects the Dawn flavour of the WebGPU backend; the header refuses to compile
# without exactly one backend being named.
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)

target_link_libraries(imgui PUBLIC
    imgui_core
    SDL3::SDL3
    dawn::webgpu_dawn
)

set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")
