# Unit tests (Google Test). GoogleTest is vendored as a submodule under
# submodules/googletest to match the other dependencies (raylib, clay, nanosvg,
# tracy) rather than fetched at configure time. Built only when BUILD_TESTS is ON.
set(GTEST_DIR ${CMAKE_SOURCE_DIR}/submodules/googletest)
if(NOT EXISTS ${GTEST_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "googletest submodule is missing. Fetch it with:\n"
        "  git submodule update --init submodules/googletest")
endif()

# We only need gtest (no gmock) and never want it installed. gtest_force_shared_crt
# matches the project's dynamic CRT on MSVC-like toolchains (a no-op under the
# GNU-driver clang used here, where CMake's MSVC is false — harmless either way).
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Force googletest to build STATIC even in the hot-reload build (where the project
# sets BUILD_SHARED_LIBS ON). A plain directory-scoped set() shadows the cache for
# this subtree only, leaving GRAPHICS/App shared. Otherwise gtest would build as
# DLLs that don't sit next to ui_tests.exe, and gtest_discover_tests would fail to
# launch it (STATUS_DLL_NOT_FOUND). Linking gtest statically makes the test exe need
# only GRAPHICS.dll + raylib.dll, which resolve from the exe's own directory.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${GTEST_DIR} ${CMAKE_BINARY_DIR}/_deps/googletest EXCLUDE_FROM_ALL)

include(GoogleTest)

add_executable(ui_tests
    Ui/HoverTest.cpp
    Ui/ExplorerTest.cpp
    Ui/StorageTest.cpp
)
# Linking GRAPHICS propagates its PUBLIC include dirs, so #include "Core/..." /
# "Backend/..." resolve without restating them here. GTest::gtest_main supplies main().
# ExplorerTest drives the real AppUi::UiExplorer and StorageTest the raylib-linked
# storage wrapper, so this also needs App's headers (header-only; the App library itself
# is not linked, which would drag in App.cpp's whole GUI + a raylib window).
target_link_libraries(ui_tests PRIVATE GRAPHICS GTest::gtest_main)
target_include_directories(ui_tests PRIVATE ${APP_INC_DIRS})
# StorageTest includes glaze (via the serialization headers); silence the UCRT
# fopen-deprecation from its file_ops.hpp, as the App and core_tests targets do.
if(WIN32)
    target_compile_definitions(ui_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---- core_tests: the UI-free domain layer ------------------------------------
# The command / parameter / geometry code is header-only over Common's DTL.h, so this
# target deliberately links NEITHER GRAPHICS NOR raylib: it needs no window, no GL, and
# no assets, which keeps it fast and hermetic. Include the source dirs directly rather
# than linking App -- App compiles App.cpp (the whole GUI) and would drag raylib back in.
#
# Do NOT add anything here that reaches Scene.h: it pulls Toolbox.h / CanvasCamera.h ->
# raylib.h, which is exactly the dependency this target exists to avoid.
add_executable(core_tests
    Core/UnitsTest.cpp
    Core/ExpressionParserTest.cpp
    Core/ParameterEngineTest.cpp
    Core/CommandSystemTest.cpp
    Core/DimensionTest.cpp
    Core/DimensionVisualTest.cpp
    Core/SerializationTest.cpp
    Core/ConstraintSolverTest.cpp
)
target_include_directories(core_tests PRIVATE
    ${COMMON_DIR}
    ${GLAZE_INC}
    ${SRC_DIR}/App/Gui
    ${SRC_DIR}/App/Core
    ${SRC_DIR}/App/Core/Command
    ${SRC_DIR}/App/Core/Command/Sketch
    ${SRC_DIR}/App/Core/Context
    ${SRC_DIR}/App/Core/Document
    ${SRC_DIR}/App/Core/Geometry
    ${SRC_DIR}/App/Core/Kernel
    ${SRC_DIR}/App/Core/Parameter
    ${SRC_DIR}/App/Core/Serialize
    ${SRC_DIR}/App/Core/Tool
)
target_link_libraries(core_tests PRIVATE GTest::gtest_main)
# See the App target: silence the UCRT fopen-deprecation from glaze's file_ops.hpp.
if(WIN32)
    target_compile_definitions(core_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ui_tests lands in the shared runtime output dir (bin/$<CONFIG>) next to GRAPHICS.dll
# and raylib.dll, so a hot-reload (shared) build resolves those at discovery/run time;
# a static build links GRAPHICS in and needs no DLLs.
gtest_discover_tests(ui_tests)
gtest_discover_tests(core_tests)
