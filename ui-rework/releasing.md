# Releasing

GitHub Actions builds the release files. The release workflow, `.github/workflows/release.yml`, builds:

- a Windows installer, `ModernMorrow-<tag>-Windows-x64.exe`, made by NSIS,
- a Windows zip of the same files, `ModernMorrow-<tag>-Windows-x64.zip`, to unpack and run anywhere,
- a Linux AppImage, `ModernMorrow-<tag>-Linux-x86_64.AppImage`, for the Steam Deck and other Linux systems,
- `ModernMorrow-AutoUpdate.sh`, a script that downloads the newest AppImage and starts it, for a Steam shortcut that keeps itself up to date.

The script lives in `CI/linux/appimage/`. It finds the newest published release, pre-releases included, through GitHub's API. GitHub lists each release file with its SHA-256 checksum, and the script downloads the AppImage when that checksum differs from its own copy's. It goes by the file rather than the version number, so a release rebuilt under the same tag still reaches it.

The Linux build runs on AlmaLinux 9, like OpenMW's portable Linux package, so it works on newer systems such as SteamOS. Its libraries are inside the AppImage.

Both builds run the game on SDL3 through sdl2-compat, SDL's library for running SDL2 programs on SDL3, so newer controllers such as the 2026 Steam Controller work without Steam. `CI/sdl2_compat_versions.sh` pins both releases. To update them, change the versions there and copy each file's SHA-256 digest from its GitHub release page.

## Making a release

1. Push a tag that starts with `v`, such as `git tag v0.1.0` and `git push origin v0.1.0`.
2. Wait for the Release workflow on the Actions tab. It takes about an hour.
3. It leaves a draft pre-release for the tag with the files. Write the release notes on GitHub and publish it.

To rebuild a published release, move its tag with `git tag -f v0.1.0 <commit>` and `git push -f origin v0.1.0`. The workflow then replaces the release's files and keeps it published, and the update script picks up the new build. A release that is still a draft can't be found by its tag, so moving the tag of a draft makes a second draft. Delete the old draft by hand.

To try the builds without a release, start the Release workflow by hand from the Actions tab. The files are then on the run's page, under Artifacts.

The builds aren't signed, so Windows SmartScreen warns the first time someone runs the installer. "More info" and "Run anyway" get past it.

## Playing on the Steam Deck

The steps for installing the AppImage on a Steam Deck or a Steam Machine are in the README, in [Steam Deck and Steam Machine](../README.md#steam-deck-and-steam-machine).
