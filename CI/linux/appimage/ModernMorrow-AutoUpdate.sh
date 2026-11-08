#!/bin/bash
# Starts Modern Morrow's Linux AppImage, after updating it to the newest release on GitHub. Point a Steam shortcut at
# this script instead of at an AppImage, and the game stays up to date without a trip to Desktop Mode.
#
# The script keeps the AppImage in its own folder, as ModernMorrow.AppImage. GitHub lists each release file with its
# SHA-256 checksum. The script downloads the newest release's AppImage when that checksum differs from its own copy's,
# so a release rebuilt under the same version number still updates. When GitHub can't be reached, or the download
# fails or doesn't match its checksum, the script starts the AppImage it already has.
#
# Any arguments, such as "--launcher" or "--wizard", go to the AppImage. What the last update did is in
# ModernMorrow-AutoUpdate.log, next to the script.
set -u

REPO=DurhamG/modern-morrow

DIR="$(dirname "$(readlink -f "$0")")"
APPIMAGE="$DIR/ModernMorrow.AppImage"
LOG="$DIR/ModernMorrow-AutoUpdate.log"

# Prints the checksum and the download link of the AppImage in the newest release that has one, from GitHub's list of
# releases. The list is JSON, one field per line. Each release file starts with a "url" field that ends in
# /releases/assets/<id>, and its "digest" and "browser_download_url" fields follow before the next file starts.
newest_appimage() {
    awk '
        function check() {
            if (digest != "" && link ~ /\/ModernMorrow-[A-Za-z0-9._-]+-Linux-x86_64\.AppImage$/) {
                print digest, link
                found = 1
                exit
            }
        }
        /"url": *"[^"]*\/releases\/assets\/[0-9]+"/ { check(); digest = ""; link = "" }
        /"digest": *"sha256:/ { digest = $0; sub(/.*"sha256:/, "", digest); sub(/".*/, "", digest) }
        /"browser_download_url":/ { link = $0; sub(/.*"browser_download_url": *"/, "", link); sub(/".*/, "", link) }
        END { if (!found) check() }
    '
}

update() {
    # The newest releases come first. Pre-releases count, and drafts aren't visible without signing in.
    local releases sha256 url
    releases=$(curl -fsSL --connect-timeout 5 --max-time 30 "https://api.github.com/repos/$REPO/releases?per_page=10") ||
        { echo "Couldn't reach GitHub."; return 1; }
    read -r sha256 url < <(printf '%s\n' "$releases" | newest_appimage)
    if [[ ! "${sha256:-}" =~ ^[0-9a-f]{64}$ || ! "${url:-}" =~ ^https://github\.com/ ]]; then
        echo "No release has an AppImage with a checksum."
        return 1
    fi

    if [[ -x "$APPIMAGE" && "$(sha256sum "$APPIMAGE" | cut -d ' ' -f 1)" == "$sha256" ]]; then
        echo "Already up to date with $url."
        return 0
    fi

    # The download goes to a file of its own, so a failed one leaves the AppImage that works in place. It gives up
    # when it stalls for 30 seconds.
    local download="$APPIMAGE.download"
    echo "Downloading $url."
    if ! curl -fSL --connect-timeout 5 --speed-limit 1024 --speed-time 30 -o "$download" "$url"; then
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
    echo "Modern Morrow isn't downloaded yet, and the download failed. See $LOG." >&2
    exit 1
fi
exec "$APPIMAGE" "$@"
