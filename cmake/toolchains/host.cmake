# Toolchain file for native (non-cross) builds.
#
# Reads the compiler paths from DECKCAD_CC_PATH / DECKCAD_CXX_PATH, real
# environment variables that scripts/setup_dev_env.py exports globally (shell
# rc file on macOS/Linux, `setx` on Windows) -- not a git-ignored preset file
# each clone has to author by hand. A fresh shell picks these up once the
# setup script has run once on the machine.
#
# Toolchain files run before project(), which is the correct point to set
# CMAKE_C_COMPILER/CMAKE_CXX_COMPILER; CMakePresets.json's "base" preset
# points at this file via "toolchainFile".

if(DEFINED ENV{DECKCAD_CC_PATH})
    set(CMAKE_C_COMPILER "$ENV{DECKCAD_CC_PATH}" CACHE FILEPATH "C compiler")
endif()

if(DEFINED ENV{DECKCAD_CXX_PATH})
    set(CMAKE_CXX_COMPILER "$ENV{DECKCAD_CXX_PATH}" CACHE FILEPATH "C++ compiler")
endif()

if(NOT DEFINED ENV{DECKCAD_CC_PATH} OR NOT DEFINED ENV{DECKCAD_CXX_PATH})
    message(WARNING
        "DECKCAD_CC_PATH / DECKCAD_CXX_PATH are not set in the environment -- "
        "falling back to CMake's default compiler detection, which may not be "
        "the pinned Clang version this project expects. Run "
        "scripts/setup_dev_env.py, then open a new shell (or `source` your "
        "shell rc file) before configuring.")
endif()
