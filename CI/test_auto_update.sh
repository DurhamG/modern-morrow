#!/bin/bash
# Tests CI/linux/appimage/ModernMorrow-AutoUpdate.sh, the script that keeps the Steam Deck's AppImage up to date.
#
# Each test copies the script into a folder of its own, because the script keeps its AppImage and its log beside
# itself. A stand-in curl, first on PATH, plays GitHub: it answers the request for the latest release with a status
# and a description of the release, and answers the download with a file. The environment variables below say what it
# does. The "AppImages" are small shell scripts that write their name and arguments to a file, so a test can see
# which one the update script started.
#
# Run it with bash CI/test_auto_update.sh. It prints ok or FAILED for each test, and exits with 1 when any failed.
set -u

SCRIPT="$(cd "$(dirname "$0")" && pwd)/linux/appimage/ModernMorrow-AutoUpdate.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
failures=0

# The stand-in curl. FAKE_API_EXIT fails the request for the latest release, as when GitHub can't be reached.
# FAKE_STATUS and FAKE_RELEASE are the answer's status and description. FAKE_DOWNLOAD is the file a download gets, and
# FAKE_DOWNLOAD_EXIT fails the download. Every call is written to FAKE_CALLS.
mkdir -p "$WORK/bin"
cat > "$WORK/bin/curl" << 'EOF'
#!/bin/bash
echo "$*" >> "$FAKE_CALLS"
output=""
while (($#)); do
    case "$1" in
        -o) output="$2"; shift ;;
        */releases/latest)
            ((${FAKE_API_EXIT:-0})) && exit "$FAKE_API_EXIT"
            cat "$FAKE_RELEASE"
            printf '\n%s' "$FAKE_STATUS"
            exit 0
            ;;
    esac
    shift
done
if [[ -n "$output" ]]; then
    ((${FAKE_DOWNLOAD_EXIT:-0})) && exit "$FAKE_DOWNLOAD_EXIT"
    cp "$FAKE_DOWNLOAD" "$output"
    exit 0
fi
exit 2
EOF
chmod +x "$WORK/bin/curl"

# Writes an AppImage stand-in that records its name and arguments in the test folder's runs file.
make_appimage() {
    local path="$1" name="$2"
    printf '#!/bin/bash\necho "%s $*" >> "$(dirname "$0")/runs"\n' "$name" > "$path"
    chmod +x "$path"
}

# Writes GitHub's description of a release whose AppImage is the given file, in the form the script reads: JSON, one
# field per line, with a Windows zip listed first.
make_release() {
    local path="$1" appimage="$2" name="${3:-ModernMorrow-v9.9.9-Linux-x86_64.AppImage}"
    local digest size
    digest=$(sha256sum "$appimage" | cut -d ' ' -f 1)
    size=$(wc -c < "$appimage" | tr -d ' ')
    cat > "$path" << EOF
{
  "url": "https://api.github.com/repos/DurhamG/modern-morrow/releases/1",
  "tag_name": "v9.9.9",
  "assets": [
    {
      "url": "https://api.github.com/repos/DurhamG/modern-morrow/releases/assets/10",
      "name": "ModernMorrow-v9.9.9-Windows-x64.zip",
      "size": 1234,
      "digest": "sha256:0000000000000000000000000000000000000000000000000000000000000000",
      "browser_download_url": "https://github.com/DurhamG/modern-morrow/releases/download/v9.9.9/ModernMorrow-v9.9.9-Windows-x64.zip"
    },
    {
      "url": "https://api.github.com/repos/DurhamG/modern-morrow/releases/assets/11",
      "name": "$name",
      "size": $size,
      "digest": "sha256:$digest",
      "browser_download_url": "https://github.com/DurhamG/modern-morrow/releases/download/v9.9.9/$name"
    }
  ]
}
EOF
}

# Sets up a test folder with the update script, an old AppImage that is already there, a new AppImage for the release,
# and a release that offers the new one. A test changes what it needs before calling run_script.
setup() {
    T="$WORK/$1"
    mkdir -p "$T"
    cp "$SCRIPT" "$T/ModernMorrow-AutoUpdate.sh"
    make_appimage "$T/ModernMorrow.AppImage" old
    make_appimage "$WORK/$1-new.AppImage" new
    make_release "$WORK/$1-release.json" "$WORK/$1-new.AppImage"
    export FAKE_CALLS="$T/calls" FAKE_RELEASE="$WORK/$1-release.json" FAKE_DOWNLOAD="$WORK/$1-new.AppImage"
    export FAKE_STATUS=200 FAKE_API_EXIT=0 FAKE_DOWNLOAD_EXIT=0
    : > "$T/calls"
}

# Runs the update script with the given arguments, with no window to show progress in.
run_script() {
    env -u DISPLAY -u WAYLAND_DISPLAY PATH="$WORK/bin:$PATH" bash "$T/ModernMorrow-AutoUpdate.sh" "$@" \
        > "$T/stdout" 2> "$T/stderr"
    STATUS=$?
}

ran() { cat "$T/runs" 2> /dev/null; }
logged() { grep -qF "$1" "$T/ModernMorrow-AutoUpdate.log"; }
downloads() { grep -c -- ' -o ' "$T/calls"; }

check() {
    local name="$1"
    shift
    if "$@"; then
        echo "ok $name"
    else
        echo "FAILED $name"
        echo "  log: $(cat "$T/ModernMorrow-AutoUpdate.log" 2> /dev/null)"
        echo "  runs: $(ran)"
        echo "  stderr: $(cat "$T/stderr")"
        failures=$((failures + 1))
    fi
}

no_latest_release_starts_existing_copy() {
    setup no-latest
    FAKE_STATUS=404 run_script
    [[ "$(ran)" == "old " ]] && logged "No release is marked as the latest release."
}

server_error_starts_existing_copy() {
    setup server-error
    FAKE_STATUS=500 run_script
    [[ "$(ran)" == "old " ]] && logged "HTTP status 500"
}

unreachable_starts_existing_copy() {
    setup unreachable
    FAKE_API_EXIT=6 run_script
    [[ "$(ran)" == "old " ]] && logged "Couldn't reach GitHub."
}

up_to_date_downloads_nothing() {
    setup up-to-date
    cp "$FAKE_DOWNLOAD" "$T/ModernMorrow.AppImage"
    run_script
    [[ "$(ran)" == "new " && "$(downloads)" == 0 ]] && logged "Already up to date"
}

newer_release_is_downloaded_and_started() {
    setup newer
    run_script
    [[ "$(ran)" == "new " && "$(downloads)" == 1 && -x "$T/ModernMorrow.AppImage" ]] && logged "Updated." &&
        cmp -s "$T/ModernMorrow.AppImage" "$FAKE_DOWNLOAD"
}

failed_download_keeps_existing_copy() {
    setup failed-download
    FAKE_DOWNLOAD_EXIT=28 run_script
    [[ "$(ran)" == "old " && ! -e "$T/ModernMorrow.AppImage.download" ]] && logged "The download failed."
}

wrong_checksum_keeps_existing_copy() {
    setup wrong-checksum
    make_appimage "$WORK/wrong-checksum-other.AppImage" other
    FAKE_DOWNLOAD="$WORK/wrong-checksum-other.AppImage" run_script
    [[ "$(ran)" == "old " && ! -e "$T/ModernMorrow.AppImage.download" ]] &&
        logged "The download doesn't match its checksum."
}

first_run_without_a_release_fails_with_a_message() {
    setup first-run
    rm "$T/ModernMorrow.AppImage"
    FAKE_STATUS=404 run_script
    [[ "$STATUS" == 1 && -z "$(ran)" ]] && grep -qF "ModernMorrow-AutoUpdate.log" "$T/stderr"
}

first_run_downloads_the_game() {
    setup first-download
    rm "$T/ModernMorrow.AppImage"
    run_script
    [[ "$(ran)" == "new " ]]
}

release_without_appimage_is_ignored() {
    setup no-appimage
    make_release "$FAKE_RELEASE" "$FAKE_DOWNLOAD" "ModernMorrow-v9.9.9-Linux-x86_64.tar.gz"
    run_script
    [[ "$(ran)" == "old " && "$(downloads)" == 0 ]] && logged "The latest release has no AppImage with a checksum."
}

arguments_reach_the_appimage() {
    setup arguments
    FAKE_STATUS=404 run_script --launcher --skip-menu
    [[ "$(ran)" == "old --launcher --skip-menu" ]]
}

asks_github_for_the_latest_release() {
    setup asks-latest
    run_script
    grep -qF "https://api.github.com/repos/DurhamG/modern-morrow/releases/latest" "$T/calls"
}

for test in no_latest_release_starts_existing_copy server_error_starts_existing_copy \
    unreachable_starts_existing_copy up_to_date_downloads_nothing newer_release_is_downloaded_and_started \
    failed_download_keeps_existing_copy wrong_checksum_keeps_existing_copy \
    first_run_without_a_release_fails_with_a_message first_run_downloads_the_game \
    release_without_appimage_is_ignored arguments_reach_the_appimage asks_github_for_the_latest_release; do
    check "$test" "$test"
done

if ((failures)); then
    echo "$failures test(s) failed."
    exit 1
fi
echo "All update script tests passed."
