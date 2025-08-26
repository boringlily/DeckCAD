cmake_minimum_required(VERSION 3.15)
project(DeckCAD)

# Require C11 and Cpp23
set(CMAKE_C_STANDARD 11) 
set(CMAKE_CXX_STANDARD 23)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}/bin") 
# Global compile definitions.
add_compile_definitions(
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX=1>
    $<$<PLATFORM_ID:Windows>:PLATFORM_WIN32=1>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_DARWIN=1>
)


option(HOT_RELOAD "Enable a dynamic build with Runtime loading of the application Core Logic" OFF)

SET(CMAKE_POLICY_DEFAULT_CMP0077 NEW) 

set(CUSTOMIZE_BUILD ON)
set(BUILD_EXAMPLES OFF)
set(WITH_PIC OFF)
set(BUILD_SHARED_LIBS OFF)
set(SUPPORT_MODULE_RAUDIO OFF)
set(SUPPORT_FILEFORMAT_BMP ON)


if(HOT_RELOAD)
add_compile_definitions(__HOT_RELOAD_ENABLED__)
if(WIN32)
    add_compile_definitions(CLAY_DLL)
endif()
set(WITH_PIC ON)
set(BUILD_SHARED_LIBS ON)
endif()


set(RAYLIB_DIR ${CMAKE_SOURCE_DIR}/submodules/raylib)
set(CLAY_DIR ${CMAKE_SOURCE_DIR}/submodules/clay)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(COMMON_DIR ${SRC_DIR}/Common)
set(GRAPHICS_DIR ${SRC_DIR}/Graphics)
set(CORE_DIR ${SRC_DIR}/Core)
set(MAIN_DIR ${SRC_DIR}/Main)

# This custom target's job is to configure cmake each time we compile, this is only useful for hot-reloading feature.
if(HOT_RELOAD)
add_custom_target(hot_reload ALL
        # COMMAND cd .. / cmake -B build
    )
endif()

# Macro used to append a random string to the target's pdb name.
macro(RANDOMIZE_PDB_NAME target)
    string(RANDOM random_string)
    set(pdbname "${target}_")
    string(APPEND pdbname ${random_string})
    set_target_properties(${target}
        PROPERTIES
        COMPILE_PDB_NAME ${pdbname}
        PDB_NAME ${pdbname}
    )
endmacro()

add_subdirectory(${RAYLIB_DIR}/projects/CMake)
add_subdirectory(${COMMON_DIR})
add_subdirectory(${GRAPHICS_DIR})
add_subdirectory(${CORE_DIR})
add_subdirectory(${MAIN_DIR})
