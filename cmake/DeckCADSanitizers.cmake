# Wires AddressSanitizer and UndefinedBehaviorSanitizer into DeckCAD's OWN
# targets (deckcad_common/platform/graphics/core/main, tests, and in-tree
# imgui) via DECKCAD_ENABLE_ASAN / DECKCAD_ENABLE_UBSAN. Deliberately does
# NOT extend to the externally-built dependencies (SDL, Dawn, FreeType,
# msdfgen, googletest, see cmake/DeckCADDeps.cmake): those are separate,
# already-installed builds, and instrumenting them too would mean a second
# multi-hundred-target Dawn build (its own cache entry, since instrumented
# and non-instrumented builds can't share one) just to sanitize code this
# project doesn't own or routinely change. ASan still catches memory bugs at
# the boundary with those libraries (it intercepts malloc/free globally,
# regardless of which translation unit allocated); UBSan issues occurring
# *inside* their own code specifically won't be caught, only in ours.

function(deckcad_enable_sanitizers)
    set(sanitizers "")
    if(DECKCAD_ENABLE_ASAN)
        list(APPEND sanitizers "address")
    endif()
    if(DECKCAD_ENABLE_UBSAN)
        list(APPEND sanitizers "undefined")
    endif()
    if(NOT sanitizers)
        return()
    endif()

    list(JOIN sanitizers "," sanitizer_flag_value)
    set(sanitizer_flags
        -fsanitize=${sanitizer_flag_value}
        -fno-omit-frame-pointer  # keeps sanitizer stack traces readable
        -fno-sanitize-recover=all  # abort on the first UB hit instead of limping on, so a bad run reliably fails CI
    )

    add_compile_options(${sanitizer_flags})
    add_link_options(-fsanitize=${sanitizer_flag_value})

    message(STATUS "Sanitizers enabled for DeckCAD's own targets: ${sanitizer_flag_value}")
endfunction()
