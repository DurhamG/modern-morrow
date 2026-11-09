#!/bin/bash
# Starts Modern Morrow's Linux AppImage, after updating it to the newest release on GitHub. Point a Steam shortcut at
# this script instead of at an AppImage, and the game stays up to date without a trip to Desktop Mode.
#
# The script keeps the AppImage in its own folder, as ModernMorrow.AppImage. GitHub lists each release file with its
# SHA-256 checksum. The script downloads the newest release's AppImage when that checksum differs from its own copy's,
# so a release rebuilt under the same version number still updates. When GitHub can't be reached, or the download
# fails or doesn't match its checksum, the script starts the AppImage it already has.
#
# While it downloads, the script shows a window with a progress bar, using zenity, which SteamOS has. Steam's Game Mode
# shows it like a game's window. On a system without zenity, the download shows nothing.
#
# Any arguments, such as "--launcher" or "--wizard", go to the AppImage. What the last update did is in
# ModernMorrow-AutoUpdate.log, next to the script.
set -u

REPO=DurhamG/modern-morrow

DIR="$(dirname "$(readlink -f "$0")")"
APPIMAGE="$DIR/ModernMorrow.AppImage"
LOG="$DIR/ModernMorrow-AutoUpdate.log"

# Prints the checksum, the size in bytes and the download link of the AppImage in the newest release that has one,
# from GitHub's list of releases. The list is JSON, one field per line. Each release file starts with a "url" field
# that ends in /releases/assets/<id>, and its "size", "digest" and "browser_download_url" fields follow before the next
# file starts.
newest_appimage() {
    awk '
        function check() {
            if (digest != "" && link ~ /\/ModernMorrow-[A-Za-z0-9._-]+-Linux-x86_64\.AppImage$/) {
                print digest, (size == "" ? 0 : size), link
                found = 1
                exit
            }
        }
        /"url": *"[^"]*\/releases\/assets\/[0-9]+"/ { check(); digest = ""; size = ""; link = "" }
        /"size": *[0-9]+/ { size = $0; gsub(/[^0-9]/, "", size) }
        /"digest": *"sha256:/ { digest = $0; sub(/.*"sha256:/, "", digest); sub(/".*/, "", digest) }
        /"browser_download_url":/ { link = $0; sub(/.*"browser_download_url": *"/, "", link); sub(/".*/, "", link) }
        END { if (!found) check() }
    '
}

# Succeeds when a window can be shown.
can_show_window() {
    command -v zenity > /dev/null && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]
}

# Prints the progress of a download for zenity, once a second until the download's process ends. Each update is a
# percentage, then a line that starts with "#" for the text above the bar.
print_progress() {
    local pid="$1" file="$2" size="$3" have
    while kill -0 "$pid" 2> /dev/null; do
        have=$(stat -c %s "$file" 2> /dev/null || echo 0)
        if ((size > 0)); then
            echo $((have * 99 / size))
            echo "# Downloading the Modern Morrow update: $((have / 1048576)) of $((size / 1048576)) MB"
        fi
        sleep 1
    done
    echo 100
}

update() {
    # The newest releases come first. Pre-releases count, and drafts aren't visible without signing in.
    local releases sha256 size url
    releases=$(curl -fsSL --connect-timeout 5 --max-time 30 "https://api.github.com/repos/$REPO/releases?per_page=10") ||
        { echo "Couldn't reach GitHub."; return 1; }
    read -r sha256 size url < <(printf '%s\n' "$releases" | newest_appimage)
    [[ "${size:-}" =~ ^[0-9]+$ ]] || size=0
    if [[ ! "${sha256:-}" =~ ^[0-9a-f]{64}$ || ! "${url:-}" =~ ^https://github\.com/ ]]; then
        echo "No release has an AppImage with a checksum."
        return 1
    fi

    if [[ -x "$APPIMAGE" && "$(sha256sum "$APPIMAGE" | cut -d ' ' -f 1)" == "$sha256" ]]; then
        echo "Already up to date with $url."
        return 0
    fi

    # The download goes to a file of its own, so a failed one leaves the AppImage that works in place. It gives up
    # when it stalls for 30 seconds. The progress window closes when the download ends, and the download carries on
    # if the window can't open.
    local download="$APPIMAGE.download"
    echo "Downloading $url."
    rm -f "$download"
    curl -fsSL --connect-timeout 5 --speed-limit 1024 --speed-time 30 -o "$download" "$url" &
    local pid=$!
    if can_show_window; then
        print_progress "$pid" "$download" "$size" |
            zenity --progress --title="Modern Morrow" --text="Downloading the Modern Morrow update" --width=480 \
                --auto-close --no-cancel
    fi
    if ! wait "$pid"; then
        rm -f "$download"
        echo "The download failed."
        return 1
    fi
    if ! echo "$sha256  $download" | sha256sum -c --status; then
        rm -f "$download"
        echo "The download doesn't match its checksum."
        return 1
    fi
    chmod +x "$download"
    mv -f "$download" "$APPIMAGE"
    echo "Updated."
}

update > "$LOG" 2>&1

if [[ ! -x "$APPIMAGE" ]]; then
    message="Modern Morrow couldn't download the game. Check the internet connection, then start it again. \
What went wrong is in $LOG."
    echo "$message" >&2
    can_show_window && zenity --error --title="Modern Morrow" --width=480 --no-markup --text="$message"
    exit 1
fi
exec "$APPIMAGE" "$@"
