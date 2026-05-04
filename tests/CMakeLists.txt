set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${SUBMODULES_DIR}/googletest ${CMAKE_BINARY_DIR}/submodules/googletest EXCLUDE_FROM_ALL)

add_executable(deckcad_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/MathTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CameraTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AppStateTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TextAtlasTests.cpp
)

# Camera.cpp and AppState.cpp are compiled directly rather than linked through
# deckcad_core, so the tests do not drag in a GPU device or a window.
target_sources(deckcad_tests PRIVATE
    ${SRC_DIR}/Graphics/Viewport/Camera.cpp
    ${SRC_DIR}/Core/AppState.cpp
    ${SRC_DIR}/Graphics/Text/MsdfAtlas.cpp
)

target_include_directories(deckcad_tests PRIVATE
    ${SRC_DIR}/Common
    ${SRC_DIR}/Core
    ${SRC_DIR}/Graphics/Viewport
    ${SRC_DIR}/Graphics/Text
)

# The atlas tests bake a real font from the repo's assets.
target_compile_definitions(deckcad_tests PRIVATE
    DECKCAD_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(deckcad_tests PRIVATE
    gtest_main
    msdfgen::msdfgen-core
    msdfgen::msdfgen-ext
)

include(GoogleTest)
gtest_discover_tests(deckcad_tests)

set_target_properties(deckcad_tests PROPERTIES FOLDER "DeckCAD")
