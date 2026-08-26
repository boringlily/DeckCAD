add_executable(DeckCAD ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# Needed for Core/App.h and Core/AppState.h even when deckcad_core isn't
# linked below (its PUBLIC include dir only propagates through linking).
target_include_directories(DeckCAD PRIVATE ${CMAKE_SOURCE_DIR}/src/Core)

# SDL3 and Dawn arrive transitively through the module targets; naming them
# again here only produces duplicate-library warnings at link time.
if(DECKCAD_HOT_RELOAD)
    # Core is loaded at runtime via Platform::DynamicLibrary instead of being
    # linked in, so editing a panel only requires rebuilding deckcad_core.
    target_sources(DeckCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/HotReloadCore.cpp)
    target_compile_definitions(DeckCAD PRIVATE DECKCAD_HOT_RELOAD_ENABLED)
    target_link_libraries(DeckCAD PRIVATE
        deckcad_graphics
        deckcad_platform
    )
    add_dependencies(DeckCAD deckcad_core)

    # deckcad_core is a shared library, so it has to sit next to the
    # executable for Platform::DynamicLibrary to find it at runtime.
    add_custom_command(TARGET DeckCAD POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:deckcad_core> $<TARGET_FILE_DIR:DeckCAD>
        COMMENT "Staging the hot-reloadable Core library"
    )

    # SDL3 is also built shared in this configuration (see the root
    # CMakeLists.txt), so it needs staging too.
    add_custom_command(TARGET DeckCAD POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:DeckCAD>
        COMMENT "Staging the SDL3 runtime library"
    )
else()
    target_link_libraries(DeckCAD PRIVATE
        deckcad_core
        deckcad_graphics
        deckcad_platform
    )
endif()

if(APPLE)
    target_link_libraries(DeckCAD PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework IOKit"
    )
endif()

# Dawn is built as a shared library, so it has to sit next to the executable.
add_custom_command(TARGET DeckCAD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:webgpu_dawn> $<TARGET_FILE_DIR:DeckCAD>
    COMMENT "Staging the Dawn runtime library"
)

# Assets are resolved relative to the executable at runtime, so stage them.
add_custom_command(TARGET DeckCAD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:DeckCAD>/assets
    COMMENT "Staging assets"
)

set_target_properties(DeckCAD PROPERTIES FOLDER "DeckCAD")
