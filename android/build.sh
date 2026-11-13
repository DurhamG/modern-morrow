#!/bin/bash
# Builds the Android version of Modern Morrow. See ui-rework/android.md.
#
#   android/build.sh deps     builds the image of Android dependencies, modernmorrow-android-deps
#   android/build.sh openmw   builds libopenmw.so in that image, and puts the app's libraries and assets in place
#   android/build.sh apk      builds the debug APK on this machine with Gradle, into android/out/
#   android/build.sh release  builds the release APK, signed with the key that ANDROID_KEYSTORE,
#                             ANDROID_KEYSTORE_PASSWORD and ANDROID_KEY_ALIAS name
#   android/build.sh          does deps, openmw and apk
#
# libopenmw.so builds in the Docker volume modernmorrow-android-build, and ccache keeps its cache in
# modernmorrow-android-ccache, so later builds only compile what changed. JOBS sets how many compiles run at once.
# CI sets IMAGE to the image it pulled, CCACHE_HOST_DIR to a folder it caches between runs, and KEEP_SYMBOLS=1 to
# keep libopenmw.so with its debug information in android/out/symbols. MM_VERSION_CODE and MM_VERSION_NAME set the
# APK's version.
set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$ANDROID_DIR")"
IMAGE="${IMAGE:-modernmorrow-android-deps}"
JOBS="${JOBS:-8}"

PATH="$HOME/.docker/bin:$PATH"

build_deps() {
    docker build --platform linux/amd64 -t "$IMAGE" "$ANDROID_DIR"
}

build_openmw() {
    docker run --rm --platform linux/amd64 \
        -v "$REPO_ROOT:/src" \
        -v modernmorrow-android-build:/build \
        -v "${CCACHE_HOST_DIR:-modernmorrow-android-ccache}:/root/.ccache" \
        -e JOBS="$JOBS" \
        -e KEEP_SYMBOLS="${KEEP_SYMBOLS:-0}" \
        "$IMAGE" /src/android/container-build.sh
}

# Runs a Gradle task, then copies the APK it made to android/out/ under the given name.
run_gradle() {
    local task="$1" built="$2" name="$3"
    if [ ! -f "$ANDROID_DIR/app/src/main/assets/version" ]; then
        echo "No libraries or assets in the app yet. Run $0 openmw first." >&2
        exit 1
    fi

    export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
    export JAVA_HOME="${JAVA_HOME:-/Applications/Android Studio.app/Contents/jbr/Contents/Home}"
    (cd "$ANDROID_DIR" && ./gradlew --quiet "$task")
    mkdir -p "$ANDROID_DIR/out"
    cp "$ANDROID_DIR/app/build/outputs/apk/$built" "$ANDROID_DIR/out/$name"
    echo "APK: $ANDROID_DIR/out/$name"
}

build_apk() {
    run_gradle assembleDebug debug/app-debug.apk modern-morrow-debug.apk
}

build_release() {
    local var
    for var in ANDROID_KEYSTORE ANDROID_KEYSTORE_PASSWORD ANDROID_KEY_ALIAS; do
        if [ -z "${!var:-}" ]; then
            echo "$var isn't set. A release APK needs the signing key: see ui-rework/releasing.md." >&2
            exit 1
        fi
    done
    run_gradle assembleRelease release/app-release.apk modern-morrow-release.apk
}

case "${1:-all}" in
    deps) build_deps ;;
    openmw) build_openmw ;;
    apk) build_apk ;;
    release) build_release ;;
    all) build_deps && build_openmw && build_apk ;;
    *) echo "Usage: $0 [deps|openmw|apk|release]" >&2; exit 2 ;;
esac
