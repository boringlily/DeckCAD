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
# this subtree only, leaving UI/GRAPHICS/App shared. Otherwise gtest would build as
# DLLs that don't sit next to ui_tests.exe, and gtest_discover_tests would fail to
# launch it (STATUS_DLL_NOT_FOUND). Linking gtest statically makes the test exe need
# only UI.dll + raylib.dll, which resolve from the exe's own directory.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${GTEST_DIR} ${CMAKE_BINARY_DIR}/_deps/googletest EXCLUDE_FROM_ALL)

include(GoogleTest)

add_executable(ui_tests
    Ui/HoverTest.cpp
)
# Linking UI propagates its PUBLIC include dirs, so #include "Core/..." /
# "Backend/..." resolve without restating them here. GTest::gtest_main supplies main().
target_link_libraries(ui_tests PRIVATE UI GTest::gtest_main)

# ui_tests lands in the shared runtime output dir (bin/$<CONFIG>) next to UI.dll and
# raylib.dll, so a hot-reload (shared UI) build resolves those at discovery/run time;
# a static build links UI in and needs no DLLs.
gtest_discover_tests(ui_tests)
