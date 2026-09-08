# Shadows CMake's builtin FindFreetype module, purely to satisfy msdfgen's
# installed config, which calls find_dependency(Freetype REQUIRED).
#
# find_dependency() always performs its search: it skips only when an internal
# per-call-signature hash says that exact call already ran, not when
# <pkg>_FOUND is set. So the builtin module runs and goes hunting for a
# *system* FreeType, which is wrong here twice over:
#
#   - ours is an install under DECKCAD_DEPS_DIR that the module cannot see, and
#   - a Debug FreeType installs as `freetyped`, which the module's NAMES list
#     (freetype, libfreetype, freetype219) does not match anyway, so pointing
#     the module at our prefix would not have rescued it either.
#
# Left alone it fails outright on a CI runner with no libfreetype development
# package -- "Could NOT find Freetype (missing: FREETYPE_LIBRARY
# FREETYPE_INCLUDE_DIRS)" -- and, more insidiously, quietly succeeds on a
# developer machine that happens to have an unrelated FreeType lying around.
# On one machine here it bound to MSYS2's C:/msys64/mingw64/lib/libfreetype.dll.a,
# which is exactly why this stayed hidden until CI ran.
#
# The root CMakeLists runs find_package(freetype CONFIG REQUIRED) against our
# own install well before anything asks for `Freetype`, so all this module has
# to do is report that target as the answer.

if(NOT TARGET Freetype::Freetype)
    set(Freetype_FOUND FALSE)
    set(FREETYPE_FOUND FALSE)
    if(Freetype_FIND_REQUIRED)
        message(FATAL_ERROR
            "Freetype::Freetype is not defined. DeckCAD expects the root CMakeLists to "
            "find FreeType through its own exported config (find_package(freetype CONFIG)) "
            "before anything requests the Freetype package.")
    endif()
    return()
endif()

set(Freetype_FOUND TRUE)
set(FREETYPE_FOUND TRUE)
set(FREETYPE_LIBRARIES Freetype::Freetype)

# Consumers that read the variables rather than linking the target still expect
# an include path; take it from the target so there is one source of truth.
get_target_property(FREETYPE_INCLUDE_DIRS Freetype::Freetype INTERFACE_INCLUDE_DIRECTORIES)
if(NOT FREETYPE_INCLUDE_DIRS)
    set(FREETYPE_INCLUDE_DIRS "")
endif()
