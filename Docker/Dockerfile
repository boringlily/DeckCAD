# Multi-stage build image for DeckCAD.
#
# Build from the REPO ROOT (not this directory), so the COPY instructions
# below can reach cmake/toolchains/ and Docker/macos-sdk/:
#
#   docker build -f Docker/Dockerfile -t deckcad-build .
#
# `docker build` (no --target) builds the `linux` stage, unchanged from
# before: a native Linux build environment with a C++23 toolchain, Python
# (Dawn's dependency fetcher and code generators), Ninja, and the
# X11/Wayland/Vulkan development headers that SDL3 and Dawn's Vulkan backend
# compile against.
#
# `--target windows-cross` and `--target macos-cross` add a
# cross-compiling toolchain on top of that base. Both are best-effort: they
# give you a working cross-compiler, sysroot, and CMake toolchain file
# (verified in this Dockerfile with a trivial smoke-test compile), but a full
# DeckCAD+Dawn cross-build has not been exercised end-to-end. Dawn's own code
# generators (Tint) must run as host-native tools even when cross-compiling
# the final binary -- a known sharp edge in cross-compiling CMake projects
# with codegen steps -- and Dawn's upstream cross-compile support isn't
# something this repo has tested. Treat these two stages as a starting point,
# not a guarantee.

FROM rockylinux:9 AS linux

RUN dnf install -y epel-release && \
    dnf install -y --allowerasing \
        cmake \
        ninja-build \
        git \
        clang \
        clang-tools-extra \
        lld \
        ccache \
        python3 \
        python3-pip \
        pkgconfig \
        curl \
        tar \
        xz \
        file \
        # SDL3 video backends
        libX11-devel \
        libXext-devel \
        libXrandr-devel \
        libXinerama-devel \
        libXcursor-devel \
        libXi-devel \
        libXfixes-devel \
        libXScrnSaver-devel \
        libxkbcommon-devel \
        wayland-devel \
        wayland-protocols-devel \
        mesa-libEGL-devel \
        mesa-libGL-devel \
        # Dawn's Vulkan backend
        vulkan-loader-devel \
        vulkan-headers \
        && dnf clean all

ENV CC=clang
ENV CXX=clang++

# ---------------------------------------------------------------------------
# windows-cross: cross-compile for Windows with llvm-mingw.
#
# llvm-mingw (clang targeting *-w64-mingw32, https://github.com/mstorsjo/llvm-mingw)
# instead of MSVC/clang-cl + the Windows SDK: it's fully open-source and
# redistributable, so this stage needs nothing supplied by the user, unlike
# the macOS stage below. The tradeoff: mingw-w64's DirectX 12 headers lag
# the real Windows SDK, so Dawn's D3D12 backend specifically is the least
# likely part of a full build to compile as-is.
# ---------------------------------------------------------------------------
FROM linux AS windows-cross

ARG LLVM_MINGW_VERSION=20240619
ARG LLVM_MINGW_ARCHIVE=llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-ubuntu-20.04-x86_64.tar.xz

RUN curl -fsSL -o /tmp/llvm-mingw.tar.xz \
        "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSION}/${LLVM_MINGW_ARCHIVE}" && \
    mkdir -p /opt/llvm-mingw && \
    tar -xJf /tmp/llvm-mingw.tar.xz -C /opt/llvm-mingw --strip-components=1 && \
    rm /tmp/llvm-mingw.tar.xz

ENV PATH="/opt/llvm-mingw/bin:${PATH}"

# Smoke test: confirms the cross-compiler and sysroot actually produce a
# Windows PE binary. Does not exercise CMake, SDL, or Dawn.
RUN echo 'int main(void) { return 0; }' > /tmp/smoke.c && \
    x86_64-w64-mingw32-clang /tmp/smoke.c -o /tmp/smoke.exe && \
    file /tmp/smoke.exe | grep -qi "PE32+" && \
    rm /tmp/smoke.c /tmp/smoke.exe

COPY cmake/toolchains/windows-x86_64.cmake /opt/toolchains/windows-x86_64.cmake

# ---------------------------------------------------------------------------
# macos-cross: cross-compile for macOS with osxcross.
#
# Apple's license does not allow redistributing the macOS SDK, so this stage
# cannot fetch or bundle it itself. Before building this target, extract the
# SDK from your own Xcode install (see osxcross's own docs -- typically
# `xcodebuild -exportPlatformSDK` or the older `gen_sdk_package.sh` /
# tosxcross's `tools/gen_sdk_package.sh` against Xcode.app) into
# Docker/macos-sdk/MacOSX*.sdk.tar.xz. That file is git-ignored; docker build
# fails at the COPY below with a clear "not found" if it's missing.
# ---------------------------------------------------------------------------
FROM linux AS macos-cross

RUN dnf install -y --allowerasing \
        clang \
        cmake \
        patch \
        libxml2-devel \
        openssl-devel \
        bzip2 \
        gzip \
        cpio \
        && dnf clean all

RUN git clone --depth 1 https://github.com/tpoechtrager/osxcross /opt/osxcross

COPY Docker/macos-sdk/*.sdk.tar.xz /opt/osxcross/tarballs/

RUN cd /opt/osxcross && UNATTENDED=1 ./build.sh

ENV PATH="/opt/osxcross/target/bin:${PATH}"

# Smoke test: confirms the cross-compiler and SDK actually produce a Mach-O
# binary. Does not exercise CMake, SDL, or Dawn.
RUN echo 'int main(void) { return 0; }' > /tmp/smoke.c && \
    (oa64-clang /tmp/smoke.c -o /tmp/smoke || o64-clang /tmp/smoke.c -o /tmp/smoke) && \
    file /tmp/smoke | grep -qi "Mach-O" && \
    rm -f /tmp/smoke.c /tmp/smoke

COPY cmake/toolchains/macos-arm64.cmake /opt/toolchains/macos-arm64.cmake
COPY cmake/toolchains/macos-x86_64.cmake /opt/toolchains/macos-x86_64.cmake

# ---------------------------------------------------------------------------
# Docker builds the LAST stage in the file when no --target is given. Without
# this, a plain `docker build .` would build macos-cross above and fail
# immediately on the missing SDK. This stage is just `linux` again so the
# no-flags case stays the native Linux build, matching every build before
# this file grew cross-compile stages.
# ---------------------------------------------------------------------------
FROM linux
