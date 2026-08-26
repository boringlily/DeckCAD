# The only module that talks to SDL.
add_library(deckcad_platform STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Assets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DynamicLibrary.cpp
)

target_include_directories(deckcad_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(deckcad_platform
    PUBLIC
        deckcad_common
    PRIVATE
        SDL3::SDL3
        imgui   # Window.cpp forwards events to the ImGui SDL3 backend
)

if(UNIX AND NOT APPLE)
    # DynamicLibrary.cpp calls dlopen/dlsym/dlclose, which live in libdl here.
    target_link_libraries(deckcad_platform PRIVATE dl)
endif()

set_target_properties(deckcad_platform PROPERTIES FOLDER "DeckCAD")
