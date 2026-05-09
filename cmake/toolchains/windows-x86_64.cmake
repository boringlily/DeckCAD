# Cross-compile for Windows x86_64 with llvm-mingw
# (https://github.com/mstorsjo/llvm-mingw), as installed by the
# `windows-cross` stage of Docker/Dockerfile.
#
# Best-effort: llvm-mingw's DirectX 12 headers lag the real Windows SDK, so
# Dawn's D3D12 backend is the part of a full DeckCAD build most likely to
# need extra work under this toolchain. Verified only with a trivial C smoke
# test (see the Dockerfile) -- not a full DeckCAD build.
#
# Usage: cmake -S . -B build-windows -G Ninja
#            -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86_64.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(DECKCAD_MINGW_TRIPLE x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER ${DECKCAD_MINGW_TRIPLE}-clang REQUIRED)
find_program(CMAKE_CXX_COMPILER ${DECKCAD_MINGW_TRIPLE}-clang++ REQUIRED)
find_program(CMAKE_RC_COMPILER ${DECKCAD_MINGW_TRIPLE}-windres REQUIRED)

get_filename_component(DECKCAD_MINGW_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
set(CMAKE_FIND_ROOT_PATH "${DECKCAD_MINGW_BIN_DIR}/../${DECKCAD_MINGW_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
