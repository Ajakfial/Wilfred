FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

# Match the Linux CI job in .github/workflows/tests.yml: Ninja, pkg-config,
# and X11 headers for the overlay. WebKitGTK and gtk-layer-shell are optional
# at configure time, so CMake falls back to the X11 overlay when they are
# absent. clang-format/clang-tidy match the lint workflow's tooling.
RUN apt-get update \
    && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends \
        ninja-build \
        pkg-config \
        libx11-dev \
        clang-format \
        clang-tidy \
    && rm -rf /var/lib/apt/lists/*
