#!/bin/bash
# The SDL3 and sdl2-compat releases that Modern Morrow's release builds ship. sdl2-compat is SDL's own library that
# runs programs written for SDL2 on SDL3, so the game keeps its SDL2 code but gets SDL3's controller support. That
# includes the 2026 Steam Controller, which SDL2 doesn't recognize.
#
# Every file comes from the libsdl-org releases on GitHub. The SHA-256 digests are the ones GitHub lists for each file.

SDL3_VERSION=3.4.20
SDL3_URL=https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}
SDL3_WIN64_SHA256=368bed8b24e44ad9ea372990814f2343fcfb9e468abb7f8f4a3bc146c7304752
SDL3_SOURCE_SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f

SDL2_COMPAT_VERSION=2.32.74
SDL2_COMPAT_URL=https://github.com/libsdl-org/sdl2-compat/releases/download/release-${SDL2_COMPAT_VERSION}
SDL2_COMPAT_WIN64_SHA256=94a0184bc2d0670151a023656fb58e8f722fc001bda3a6a424234c88c5501cc0
SDL2_COMPAT_SOURCE_SHA256=ec68abde77e2e459c8abc4f9587b8976b5d2a657b2bfda2fb3a079b0f8924588
