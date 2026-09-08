# Provides Dawn (native WebGPU) as a downloaded prebuilt binary release
# instead of a source submodule that this project configures, compiles and
# installs itself.
#
# Why: building Dawn from source cost ~0.9 GB of submodule working tree
# (124k files), a further ~570 MB that DAWN_FETCH_DEPENDENCIES pulled into
# submodules/dawn/third_party at configure time, a ~1.4 GB build tree, a
# Python 3 dependency for Dawn's fetcher and code generators, and 10-20
# minutes of wall clock on every machine that had not built that exact
# submodule commit before. None of it bought us anything: DeckCAD touches
# only the standard WebGPU surface (src/Graphics/Gpu/GpuContext.h is the one
# file that includes a Dawn header), so a stock build of Dawn is all we need.
#
# What makes this a drop-in: Dawn's own release workflow packages each
# artifact by running `cmake --install` and tarring the resulting prefix (see
# .github/workflows/ci.yml in the Dawn tree). A release tarball is therefore
# the same kind of thing deckcad_build_external_dependency() used to produce
# -- headers, lib/, and a lib/cmake/Dawn/DawnConfig.cmake exporting
# dawn::webgpu_dawn -- so the consuming side stays an ordinary
# find_package(Dawn CONFIG REQUIRED) and no DeckCAD source changes.
#
# Caveats worth knowing, all of them deliberate trade-offs:
#
# - These builds are, in the Dawn team's own words, "a best-effort service...
#   not signed or guaranteed by Google"; only the googlesource Git tree is
#   official. That is why the release below is PINNED and every artifact is
#   checked against a recorded SHA-256 rather than tracking "latest".
# - The prebuilts are STATIC (Dawn's DAWN_BUILD_MONOLITHIC_LIBRARY defaults to
#   STATIC and the release workflow does not override it), where this project
#   previously asked for SHARED. That is fine here because Graphics is the
#   only module that links Dawn and it only ever lives in the main executable
#   -- Core, the hot-reloadable one, never links Dawn at all.
# - Dawn publishes Release and Debug binaries, and which one we take is
#   decided per platform rather than uniformly, because only Windows forces
#   the issue. MSVC stamps every object with its `_ITERATOR_DEBUG_LEVEL` and
#   `RuntimeLibrary` and has the linker reject mismatches outright, so a Debug
#   DeckCAD (IDL 2, msvcrtd) genuinely cannot link a Release Dawn (IDL 0,
#   msvcrt) -- lld-link fails the build rather than producing something
#   subtly broken. Windows therefore matches the configuration. Nothing on
#   macOS or Linux works that way: a Release static library links into a Debug
#   executable there without complaint, so those stay on Release, which also
#   keeps CI (Linux, Debug) on a 34 MB download instead of a 794 MB one.
#   The cost avoided is real -- Dawn's Windows Debug webgpu_dawn.lib alone is
#   1.47 GB unpacked, against 590 MB for Release.
# - Only the four desktop targets Dawn's CI publishes are supported:
#   windows-latest (MSVC), macos-latest (arm64), macos-15-intel and
#   ubuntu-latest (gcc, built in a manylinux_2_28 container, so glibc 2.28 is
#   the floor). Anything else fails loudly below rather than silently.

# The pinned release. These two values move together with the hashes in
# deckcad_resolve_dawn_artifact() -- a new tag means new artifact hashes -- so
# they are deliberately plain variables rather than cache entries a user could
# half-override into a hash mismatch.
#
# To roll forward: pick a tag from https://github.com/google/dawn/releases,
# take the commit SHA out of the artifact names, download the four
# Dawn-<commit>-<platform>-Release.tar.gz assets and record their SHA-256.
set(DECKCAD_DAWN_RELEASE_TAG "v20260831.205016")
set(DECKCAD_DAWN_RELEASE_COMMIT "8f25b9c7064ae89802c8db4e7daab9d1fd3e77ca")

# deckcad_resolve_dawn_artifact(<out_platform> <out_config> <out_sha256>)
#
# Maps the target platform (and, on Windows, the build configuration) onto the
# name of the Dawn CI artifact built for it, plus that artifact's recorded hash.
function(deckcad_resolve_dawn_artifact out_platform out_config out_sha256)
    # Release everywhere except a Windows Debug build; see the header comment
    # for why Windows is the exception.
    set(config "Release")

    if(WIN32)
        if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(FATAL_ERROR
                "Dawn's Windows prebuilt is built with MSVC and cannot be linked by a "
                "MinGW/GNU toolchain (different C runtime and object conventions). "
                "cmake/toolchains/windows-x86_64.cmake (llvm-mingw) is the affected "
                "path. Build with clang or cl targeting the MSVC ABI instead.")
        endif()
        if(CMAKE_CONFIGURATION_TYPES)
            message(FATAL_ERROR
                "On Windows the Dawn artifact has to match the build configuration "
                "exactly (MSVC's _ITERATOR_DEBUG_LEVEL/RuntimeLibrary checks make a "
                "mismatch a link error), which a multi-config generator cannot express "
                "with one imported library. Configure with a single-config generator "
                "-- Ninja, as CMakePresets.json does -- and set CMAKE_BUILD_TYPE.")
        endif()
        set(platform "windows-latest")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(config "Debug")
            set(sha256 "6e7c71640b1df329003a9ae1080a39a1bd150aebf378ed32dda564f458ef9241")
        else()
            set(sha256 "fc0917e5963a411f0a3820a09601074444559930dc2d0a7bc3e1f0ab17bb558c")
        endif()
    elseif(APPLE)
        # CMAKE_OSX_ARCHITECTURES wins when it names exactly one architecture;
        # a universal build has no single matching artifact.
        list(LENGTH CMAKE_OSX_ARCHITECTURES osx_arch_count)
        if(osx_arch_count GREATER 1)
            message(FATAL_ERROR
                "Dawn's prebuilts are single-architecture, so CMAKE_OSX_ARCHITECTURES "
                "cannot name more than one ('${CMAKE_OSX_ARCHITECTURES}'). Configure "
                "arm64 and x86_64 separately, or use Dawn's xcframework artifact.")
        elseif(osx_arch_count EQUAL 1)
            set(target_arch "${CMAKE_OSX_ARCHITECTURES}")
        else()
            set(target_arch "${CMAKE_SYSTEM_PROCESSOR}")
        endif()

        if(target_arch MATCHES "^(arm64|aarch64)$")
            set(platform "macos-latest")
            set(sha256 "a8421b7784ef1b03d29f9b0c2ca688c276d30ef47264cbc8281dc0fb223c7886")
        elseif(target_arch MATCHES "^(x86_64|AMD64)$")
            set(platform "macos-15-intel")
            set(sha256 "9e7da73c07f96c44a5a3ff3ac0eaa86b8962dfa6ad17e80f0e3a5e5d6c961ef5")
        else()
            message(FATAL_ERROR "No Dawn prebuilt for macOS architecture '${target_arch}'.")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            message(FATAL_ERROR
                "Dawn publishes a Linux prebuilt for x86_64 only, not "
                "'${CMAKE_SYSTEM_PROCESSOR}'.")
        endif()
        set(platform "ubuntu-latest")
        set(sha256 "2ee5414628bf6425bf5d5d206bd42bdbf92fc16c877da3e703dcf1da5d0f7907")
    else()
        message(FATAL_ERROR "No Dawn prebuilt for platform '${CMAKE_SYSTEM_NAME}'.")
    endif()

    set(${out_platform} "${platform}" PARENT_SCOPE)
    set(${out_config} "${config}" PARENT_SCOPE)
    set(${out_sha256} "${sha256}" PARENT_SCOPE)
endfunction()

# deckcad_provide_dawn(<out_install_dir> <out_config>)
#
# Downloads and unpacks the pinned Dawn release for this platform into the
# shared cache directory, then sets <out_install_dir> to a prefix that
# find_package(Dawn CONFIG) understands and <out_config> to the configuration
# the artifact was built in (which the caller needs, to map DeckCAD's own
# configurations onto the one the imported target actually carries).
#
# Keyed by release tag, platform and artifact configuration, so -- exactly
# like deckcad_build_external_dependency() -- a second worktree, a wiped
# build/ directory or a fresh clone on the same machine reuses what is already
# unpacked and does no work at all. Off Windows every configuration resolves
# to the same Release artifact, so Debug and Release DeckCAD builds there
# share one copy rather than fetching two.
function(deckcad_provide_dawn out_install_dir out_config)
    deckcad_resolve_deps_dir()
    deckcad_resolve_dawn_artifact(platform config sha256)

    set(dep_root "${DECKCAD_DEPS_DIR}/dawn-prebuilt-${DECKCAD_DAWN_RELEASE_TAG}-${platform}-${config}")
    set(install_dir "${dep_root}/install")
    set(stamp_file "${dep_root}/.deckcad-installed")

    if(EXISTS "${stamp_file}")
        message(STATUS "Dawn prebuilt ${DECKCAD_DAWN_RELEASE_TAG} (${platform}, ${config}): already unpacked at ${install_dir}")
        set(${out_install_dir} "${install_dir}" PARENT_SCOPE)
        set(${out_config} "${config}" PARENT_SCOPE)
        return()
    endif()

    set(archive_name "Dawn-${DECKCAD_DAWN_RELEASE_COMMIT}-${platform}-${config}.tar.gz")
    set(archive_url "https://github.com/google/dawn/releases/download/${DECKCAD_DAWN_RELEASE_TAG}/${archive_name}")
    set(archive_path "${dep_root}/${archive_name}")

    message(STATUS "Dawn prebuilt ${DECKCAD_DAWN_RELEASE_TAG} (${platform}, ${config}): downloading (first time on this machine)...")
    file(DOWNLOAD "${archive_url}" "${archive_path}"
        EXPECTED_HASH "SHA256=${sha256}"
        SHOW_PROGRESS
        STATUS download_status
    )
    list(GET download_status 0 download_code)
    if(NOT download_code EQUAL 0)
        list(GET download_status 1 download_message)
        file(REMOVE "${archive_path}")
        message(FATAL_ERROR
            "Failed to download ${archive_url}: ${download_message}. "
            "A hash mismatch here means the pinned release was re-published or the "
            "download was corrupted -- do not just update the hash without checking why.")
    endif()

    # Unpack into a scratch directory first: the tarball has a single
    # <name>-<config>/ top-level directory that we want to become install/, and
    # renaming into place last keeps a half-extracted tree from ever looking
    # finished. The stamp file written at the end is what marks it done.
    set(extract_dir "${dep_root}/extract")
    file(REMOVE_RECURSE "${extract_dir}" "${install_dir}")
    file(MAKE_DIRECTORY "${extract_dir}")
    file(ARCHIVE_EXTRACT INPUT "${archive_path}" DESTINATION "${extract_dir}")

    file(GLOB extracted_roots LIST_DIRECTORIES true "${extract_dir}/*")
    list(LENGTH extracted_roots extracted_root_count)
    if(NOT extracted_root_count EQUAL 1)
        message(FATAL_ERROR
            "Expected exactly one top-level directory in ${archive_name}, found "
            "${extracted_root_count}. Delete ${dep_root} and retry.")
    endif()
    list(GET extracted_roots 0 extracted_root)
    file(RENAME "${extracted_root}" "${install_dir}")
    file(REMOVE_RECURSE "${extract_dir}")
    file(REMOVE "${archive_path}")

    # DawnConfig.cmake exports exactly three targets -- dawn::webgpu_dawn,
    # dawn::dawn_public_config and dawn::webgpu_upstream_cpp_headers -- and
    # webgpu_dawn is monolithic, meaning Tint is already inside it. The
    # separately installed tint_* archives (~261 MB on Windows) and the Tint
    # command line tools in bin/ (~26 MB) are consequently dead weight here,
    # and nothing can reference them because they are not in the export set.
    # Dropping them is worth about a third of the unpacked size.
    # lib/ on Windows and macOS, lib64/ on the manylinux-built Linux artifact;
    # and Unix static libraries carry a "lib" prefix the Windows ones don't.
    file(GLOB tint_archives "${install_dir}/lib*/tint_*" "${install_dir}/lib*/libtint_*")
    if(tint_archives)
        file(REMOVE ${tint_archives})
    endif()
    file(REMOVE_RECURSE "${install_dir}/bin")

    file(WRITE "${stamp_file}" "${DECKCAD_DAWN_RELEASE_TAG} ${platform} ${config}\n")
    message(STATUS "Dawn prebuilt ${DECKCAD_DAWN_RELEASE_TAG} (${platform}, ${config}): unpacked to ${install_dir}")
    set(${out_install_dir} "${install_dir}" PARENT_SCOPE)
    set(${out_config} "${config}" PARENT_SCOPE)
endfunction()

# deckcad_find_windows_sdk_dll(<out_var> <dll_name>)
#
# Locates one DLL in the x64 tools directory of the newest installed Windows
# 10/11 SDK. Prefers whatever a Visual Studio developer environment already
# points at, and falls back to the default install location. Sets <out_var> to
# an empty string when nothing matches.
function(deckcad_find_windows_sdk_dll out_var dll_name)
    set(candidate_globs "")
    if(DEFINED ENV{WindowsSdkVerBinPath})
        list(APPEND candidate_globs "$ENV{WindowsSdkVerBinPath}/x64/${dll_name}")
    endif()
    if(DEFINED ENV{WindowsSdkDir})
        list(APPEND candidate_globs "$ENV{WindowsSdkDir}/bin/*/x64/${dll_name}")
    endif()
    # CMake cannot parse $ENV{ProgramFiles(x86)} directly -- '(' is not legal in
    # a variable name -- so the name goes through a variable of its own.
    set(program_files_x86_name "ProgramFiles(x86)")
    foreach(program_files_root "$ENV{${program_files_x86_name}}" "$ENV{ProgramFiles}")
        if(program_files_root)
            list(APPEND candidate_globs "${program_files_root}/Windows Kits/10/bin/*/x64/${dll_name}")
        endif()
    endforeach()

    foreach(candidate_glob IN LISTS candidate_globs)
        file(GLOB matches "${candidate_glob}")
        if(matches)
            # Globs come back sorted ascending, so the newest SDK is last.
            list(GET matches -1 newest)
            set(${out_var} "${newest}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${out_var} "" PARENT_SCOPE)
endfunction()

# deckcad_stage_dawn_runtime(<target>)
#
# Windows only. Dawn's prebuilt is compiled with DAWN_USE_BUILT_DXC=ON, so its
# D3D12 backend loads the DirectX shader compiler at runtime -- but the release
# tarball ships no DLLs at all, only the static library and headers. Without
# these three beside the executable, adapter enumeration succeeds and then
# device creation fails outright:
#
#   [WebGPU device lost] Failed to create device:
#   DynamicLib.Open: dxil.dll Windows Error: 87
#
# Dawn looks for them on a search path that starts at the executable's own
# directory (see EnsureDXCLibraries in Dawn's PlatformFunctionsD3D12.cpp), so
# staging them next to it is what makes the prebuilt usable. They come from the
# Windows SDK, which a clang-targeting-MSVC toolchain necessarily already has:
#
# - dxil.dll        signs compiled shaders; D3D12 rejects unsigned ones, and
#                   Dawn deliberately loads it before dxcompiler.
# - dxcompiler.dll  the DXC compiler itself.
# - d3dcompiler_47.dll  the older FXC compiler. Not needed by D3D12, but Dawn's
#                   D3D11 backend probes for it during adapter discovery and
#                   prints an error to stderr on every run without it.
#
# Note for later: these are Microsoft redistributables, fine to copy into a
# local build tree. Shipping them in an installer is a licensing question this
# does not answer.
function(deckcad_stage_dawn_runtime target)
    if(NOT WIN32)
        return()
    endif()

    foreach(runtime_dll dxil.dll dxcompiler.dll d3dcompiler_47.dll)
        deckcad_find_windows_sdk_dll(runtime_dll_path "${runtime_dll}")
        if(NOT runtime_dll_path)
            message(FATAL_ERROR
                "Could not find ${runtime_dll} in any installed Windows SDK. Dawn's "
                "prebuilt needs it beside the executable at runtime (it ships no DLLs "
                "of its own). Install the Windows SDK, or configure from a Visual "
                "Studio developer prompt so WindowsSdkDir is set.")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${runtime_dll_path}" $<TARGET_FILE_DIR:${target}>
            COMMENT "Staging ${runtime_dll} for Dawn's D3D12 backend"
        )
    endforeach()
endfunction()
