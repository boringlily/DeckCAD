# Linux build image for DeckCAD.
#
# The stack needs a C++23 toolchain, Python (Dawn's dependency fetcher and code
# generators), Ninja, and the X11/Wayland/Vulkan development headers that SDL3
# and Dawn's Vulkan backend compile against.
FROM rockylinux:9

RUN dnf install -y epel-release && \
    dnf install -y --allowerasing \
        cmake \
        ninja-build \
        git \
        clang \
        clang-tools-extra \
        lld \
        python3 \
        python3-pip \
        pkgconfig \
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
