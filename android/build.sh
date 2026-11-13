#!/bin/bash
# Builds the Android version of Modern Morrow. See ui-rework/android.md.
#
#   android/build.sh deps     builds the image of Android dependencies, modernmorrow-android-deps
#   android/build.sh openmw   builds libopenmw.so in that image, and puts the app's libraries and assets in place
#   android/build.sh apk      builds the APK on this machine with Gradle, into android/out/
#   android/build.sh          does all three
#
# libopenmw.so builds in the Docker volume modernmorrow-android-build, and ccache keeps its cache in
# modernmorrow-android-ccache, so later builds only compile what changed. JOBS sets how many compiles run at once.
set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$ANDROID_DIR")"
IMAGE=modernmorrow-android-deps
JOBS="${JOBS:-8}"

PATH="$HOME/.docker/bin:$PATH"

build_deps() {
    docker build --platform linux/amd64 -t "$IMAGE" "$ANDROID_DIR"
}

build_openmw() {
    docker run --rm --platform linux/amd64 \
        -v "$REPO_ROOT:/src" \
        -v modernmorrow-android-build:/build \
        -v modernmorrow-android-ccache:/root/.ccache \
        -e JOBS="$JOBS" \
        "$IMAGE" /src/android/container-build.sh
}

build_apk() {
    local app="$ANDROID_DIR/app/src/main"
    if [ ! -f "$app/assets/version" ]; then
        echo "No libraries or assets in the app yet. Run $0 openmw first." >&2
        exit 1
    fi

    export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
    export JAVA_HOME="${JAVA_HOME:-/Applications/Android Studio.app/Contents/jbr/Contents/Home}"
    (cd "$ANDROID_DIR" && ./gradlew --quiet assembleDebug)
    mkdir -p "$ANDROID_DIR/out"
    cp "$ANDROID_DIR/app/build/outputs/apk/debug/app-debug.apk" "$ANDROID_DIR/out/modern-morrow-debug.apk"
    echo "APK: $ANDROID_DIR/out/modern-morrow-debug.apk"
}

case "${1:-all}" in
    deps) build_deps ;;
    openmw) build_openmw ;;
    apk) build_apk ;;
    all) build_deps && build_openmw && build_apk ;;
    *) echo "Usage: $0 [deps|openmw|apk]" >&2; exit 2 ;;
esac
