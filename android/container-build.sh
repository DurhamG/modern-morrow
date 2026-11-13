#!/bin/bash
# Runs inside the modernmorrow-android-deps image, from android/build.sh. The repository is at /src and the build
# folder at /build. Builds libopenmw.so, then copies the native libraries and the game's resources into the app.
set -euo pipefail

JOBS="${JOBS:-8}"
APP=/src/android/app/src/main
LIBS="$APP/jniLibs/$ABI"
ASSETS="$APP/assets"

# The repository belongs to the host's user, and git refuses to read it as root without this.
git config --global --add safe.directory /src

cmake -S /src -B /build -G Ninja ${COMMON_CMAKE_ARGS} \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_FLAGS="-I${PREFIX}/include ${CXXFLAGS}" \
    -DBUILD_BSATOOL=OFF \
    -DBUILD_BULLETOBJECTTOOL=OFF \
    -DBUILD_ESMTOOL=OFF \
    -DBUILD_ESSIMPORTER=OFF \
    -DBUILD_LAUNCHER=OFF \
    -DBUILD_MWINIIMPORTER=OFF \
    -DBUILD_MYGUI_PLUGIN=OFF \
    -DBUILD_NAVMESHTOOL=OFF \
    -DBUILD_NIFTEST=OFF \
    -DBUILD_OPENCS=OFF \
    -DBUILD_WIZARD=OFF \
    -DOPENMW_USE_SYSTEM_ICU=ON \
    -DOPENMW_USE_SYSTEM_SQLITE3=OFF \
    -DOPENMW_USE_SYSTEM_YAML_CPP=OFF \
    -DOSG_STATIC=TRUE \
    -DOPENGL_gl_LIBRARY="${PREFIX}/lib/libng_gl4es.so" \
    -DOPENGL_glx_LIBRARY="${PREFIX}/lib/libng_gl4es.so" \
    -DOPENAL_INCLUDE_DIR="${PREFIX}/include/AL" \
    -DBullet_INCLUDE_DIR="${PREFIX}/include/bullet" \
    -DMyGUI_LIBRARY="${PREFIX}/lib/libMyGUIEngineStatic.a"

cmake --build /build --target openmw --parallel "$JOBS"

# The APK gets stripped copies. /build keeps libopenmw.so with its debug information, for reading crash stacks.
rm -rf "$LIBS"
mkdir -p "$LIBS"
llvm-strip -o "$LIBS/libopenmw.so" /build/libopenmw.so
# The shared libraries it needs from the dependencies, and those they need in turn. gl4es is loaded by name rather
# than linked.
copy_needed() {
    for lib in $(llvm-readelf -d "$1" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'); do
        if [ -f "${PREFIX}/lib/$lib" ] && [ ! -f "$LIBS/$lib" ]; then
            llvm-strip -o "$LIBS/$lib" "${PREFIX}/lib/$lib"
            copy_needed "${PREFIX}/lib/$lib"
        fi
    done
}
copy_needed /build/libopenmw.so
llvm-strip -o "$LIBS/libng_gl4es.so" "${PREFIX}/lib/libng_gl4es.so"
cp "${TOOLCHAIN}/sysroot/usr/lib/${NDK_TRIPLET}/libc++_shared.so" "$LIBS/"

# The app copies these out of the APK when it changes. See GameActivity.java.
rm -rf "$ASSETS"
mkdir -p "$ASSETS/config"
cp -r /build/resources "$ASSETS/resources"
# gamecontrollerdb.txt is left out. Android turns every controller's buttons into its standard key codes, and SDL maps
# those to the right buttons by itself. The community file's Android entries number the buttons as older Android
# drivers or SDL versions did, which scrambled a Switch Pro Controller's buttons. A player can still add the file to
# the user config folder.
cp /build/openmw.cfg /build/defaults.bin "$ASSETS/config/"
git -C /src rev-parse HEAD > "$ASSETS/version"
date +%s >> "$ASSETS/version"

ls -l "$LIBS"
