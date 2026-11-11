#!/bin/bash
# Builds Modern Morrow's Linux release files: the portable package, ModernMorrow-<label>-Linux-x86_64.tar.xz, an
# AppImage made from it, and the debug information for its programs, ModernMorrow-<label>-Linux-x86_64-symbols.tar.xz,
# all in build/. The label is the first argument, such as the release tag, or the version when there is none.
#
# It runs as root on AlmaLinux 9, the same system OpenMW builds its portable Linux package on, so the result runs on
# any newer Linux system, SteamOS on the Steam Deck included. GitHub Actions runs it in the almalinux:9 container. To
# run it locally the same way, from a clone of the repository:
#
#   docker run --rm -v "$PWD:/src" -w /src almalinux:9 CI/linux/build_release.sh
#
# The build runs one compile per core, and the Lua binding files need over a gigabyte each. On a machine with many
# cores, that can use more memory than Docker's virtual machine has, and the build stalls. Limiting the container to
# some of the cores, with an option such as --cpuset-cpus=0-11, limits the compiles too.

set -euo pipefail

export CUSTOM_CPACK_LABEL="${1:-}"
export CC=gcc
export CXX=g++
# The build keeps debug information, which is saved apart from the package below. The package's programs are
# stripped, so its size stays the same.
export CMAKE_BUILD_TYPE=RelWithDebInfo
export OPENMW_LOCAL_INSTALL=1
export CI_OPENMW_USE_VCPKG=true

dnf install -y epel-release
dnf install -y git binutils gcc-toolset-13 cmake unzip pkg-config p7zip p7zip-plugins python3-pip xz file \
    mesa-libGL-devel libxkbcommon-devel mesa-libEGL-devel fontconfig openal-soft

# The newer compiler comes from a toolset that its own script turns on. The script reads unset variables.
set +u
source /opt/rh/gcc-toolset-13/enable
set -u

CI/linux/before_install_for_vcpkg.sh
CI/before_script.linux.sh

cd build
cmake --build . -- -j "$(nproc)"
cpack -G TXZ -D CPACK_STRIP_FILES=ON -D CPACK_THREADS=0 -D CPACK_INCLUDE_TOPLEVEL_DIRECTORY=0
package=$(ls ModernMorrow*-Linux-x86_64.tar.xz)

# A crash log made without a debugger lists each frame as a program and an offset in it. The debug information saved
# here, one <program>.debug file for each program the build made, names the function and line at each offset, as
# ui-rework/releasing.md explains. It only matches the programs from this same build.
rm -rf symbols
mkdir symbols
for file in *; do
    if [[ -f $file && -x $file ]] && readelf -S "$file" 2> /dev/null | grep '\.debug_info' > /dev/null; then
        objcopy --only-keep-debug --compress-debug-sections "$file" "symbols/$file.debug"
    fi
done
tar -cJf "${package%.tar.xz}-symbols.tar.xz" -C symbols .

# The AppImage is the package's files, plus a start script, a desktop entry and an icon at the top.
rm -rf AppDir
mkdir AppDir
tar -xf "$package" -C AppDir
cp ../CI/linux/appimage/AppRun AppDir/AppRun
chmod +x AppDir/AppRun
cp ../CI/linux/appimage/modern-morrow.desktop AppDir/
cp ../files/launcher/images/openmw.png AppDir/modern-morrow.png

# The package leaves OpenAL out, like OpenMW's own, so the game uses the system's copy and its sound setup. The
# AppImage also carries AlmaLinux's copy, which needs nothing beyond the C library and plays sound through ALSA. AppRun
# uses it only on a system that has no OpenAL.
mkdir AppDir/lib/openal-fallback
cp -P /usr/lib64/libopenal.so.1* AppDir/lib/openal-fallback/

# The AppImage runs the game on SDL3 through sdl2-compat, which replaces the package's SDL2. SDL3 recognizes newer
# controllers, such as the 2026 Steam Controller. The versions are in CI/sdl2_compat_versions.sh.
../CI/linux/build_sdl2_compat.sh sdl-install sdl-work
rm -f AppDir/lib/libSDL2-2.0.so*
cp -P sdl-install/lib/libSDL2-2.0.so.0* sdl-install/lib/libSDL3.so.0* AppDir/lib/

if [[ ! -x appimagetool ]]; then
    curl --fail --retry 3 -L -o appimagetool \
        https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool
fi
# Containers have no FUSE, so the tool runs from an unpacked copy of itself.
ARCH=x86_64 ./appimagetool --appimage-extract-and-run AppDir "${package%.tar.xz}.AppImage"
