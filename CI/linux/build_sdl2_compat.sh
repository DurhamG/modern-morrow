#!/bin/bash
# Builds SDL3 and sdl2-compat from their source releases, and installs them into a folder. The versions are in
# CI/sdl2_compat_versions.sh. The AppImage ships both in place of the SDL2 that the dependencies came with, so the
# game runs on SDL3. Run it on AlmaLinux 9 as root, as build_release.sh does.
#
#   CI/linux/build_sdl2_compat.sh <install folder> <work folder>

set -euo pipefail

prefix="$(realpath -m "$1")"
work="$(realpath -m "$2")"
. "$(dirname "$0")/../sdl2_compat_versions.sh"

# SDL3 loads the display, input and sound libraries when the game starts, so it only needs their headers here. The
# udev headers let it find controllers through hidraw, which is how it talks to the Steam Controller.
dnf install -y libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXi-devel libXfixes-devel \
    libXScrnSaver-devel libXtst-devel wayland-devel libxkbcommon-devel mesa-libEGL-devel mesa-libGL-devel \
    systemd-devel dbus-devel pulseaudio-libs-devel alsa-lib-devel

mkdir -p "$work"
cd "$work"

fetch() {
    local url="$1" file="$2" sha256="$3"
    if [[ ! -f "$file" ]]; then
        curl --fail --retry 3 -sSL -o "$file" "$url/$file"
    fi
    echo "$sha256  $file" | sha256sum -c -
    tar -xzf "$file"
}

fetch "$SDL3_URL" "SDL3-${SDL3_VERSION}.tar.gz" "$SDL3_SOURCE_SHA256"
fetch "$SDL2_COMPAT_URL" "sdl2-compat-${SDL2_COMPAT_VERSION}.tar.gz" "$SDL2_COMPAT_SOURCE_SHA256"

cmake -S "SDL3-${SDL3_VERSION}" -B sdl3-build -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX="$prefix" \
    -D CMAKE_INSTALL_LIBDIR=lib -D SDL_STATIC=OFF -D SDL_TESTS=OFF -D SDL_TEST_LIBRARY=OFF -D SDL_X11=ON \
    -D SDL_WAYLAND=ON -D SDL_HIDAPI=ON
cmake --build sdl3-build -j "$(nproc)"
cmake --install sdl3-build

# sdl2-compat loads libSDL3.so.0 by name when the game starts. Its RUNPATH of $ORIGIN makes it look in its own
# folder first, where the AppImage puts SDL3.
cmake -S "sdl2-compat-${SDL2_COMPAT_VERSION}" -B sdl2-compat-build -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_INSTALL_PREFIX="$prefix" -D CMAKE_INSTALL_LIBDIR=lib -D CMAKE_PREFIX_PATH="$prefix" \
    -D SDL2COMPAT_TESTS=OFF -D CMAKE_INSTALL_RPATH='$ORIGIN'
cmake --build sdl2-compat-build -j "$(nproc)"
cmake --install sdl2-compat-build

ls -la "$prefix/lib"
