#!/bin/bash
# Puts sdl2-compat's SDL2.dll and SDL3's SDL3.dll into a Windows build folder, in place of the SDL2.dll that the
# dependencies came with. The versions are in sdl2_compat_versions.sh. Packaging then ships both, so the game runs on
# SDL3. Run it after the build, because a build copies the old SDL2.dll back when it is newer than the one there.
#
#   CI/fetch_sdl2_compat_windows.sh <build folder> [download folder]

set -euo pipefail

build_dir="$1"
download_dir="${2:-$(mktemp -d)}"
. "$(dirname "$0")/sdl2_compat_versions.sh"

mkdir -p "$download_dir"
cd "$download_dir"

fetch() {
    local url="$1" file="$2" sha256="$3"
    if [[ ! -f "$file" ]]; then
        curl --fail --retry 3 -sSL -o "$file" "$url/$file"
    fi
    echo "$sha256  $file" | sha256sum -c -
}

fetch "$SDL3_URL" "SDL3-${SDL3_VERSION}-win32-x64.zip" "$SDL3_WIN64_SHA256"
fetch "$SDL2_COMPAT_URL" "sdl2-compat-${SDL2_COMPAT_VERSION}-win32-x64.zip" "$SDL2_COMPAT_WIN64_SHA256"

rm -rf sdl3 sdl2-compat
7z x -y -osdl3 "SDL3-${SDL3_VERSION}-win32-x64.zip" SDL3.dll > /dev/null
7z x -y -osdl2-compat "sdl2-compat-${SDL2_COMPAT_VERSION}-win32-x64.zip" SDL2.dll > /dev/null

# The sdl2-compat zip also has an SDL3.dll, but the SDL3 release is newer.
cp sdl2-compat/SDL2.dll "$build_dir/SDL2.dll"
cp sdl3/SDL3.dll "$build_dir/SDL3.dll"
echo "Copied sdl2-compat ${SDL2_COMPAT_VERSION} and SDL3 ${SDL3_VERSION} into $build_dir"
