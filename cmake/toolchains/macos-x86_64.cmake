# Cross-compile for macOS x86_64 (Intel) with osxcross
# (https://github.com/tpoechtrager/osxcross), as built by the `macos-cross`
# stage of Docker/Dockerfile from a user-supplied SDK.
#
# Best-effort: verified only with a trivial C smoke test (see the
# Dockerfile) -- not a full DeckCAD build. Dawn's own code generators must
# still run as host (Linux) binaries even though the final artifacts target
# macOS; this toolchain file only sets up the C/C++ cross-compiler, it does
# not address that.
#
# Usage: cmake -S . -B build-macos-x86_64 -G Ninja
#            -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-x86_64.cmake

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_OSX_ARCHITECTURES x86_64)

find_program(CMAKE_C_COMPILER NAMES o64-clang REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES o64-clang++ REQUIRED)

get_filename_component(DECKCAD_OSXCROSS_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
set(CMAKE_FIND_ROOT_PATH "${DECKCAD_OSXCROSS_BIN_DIR}/..")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
