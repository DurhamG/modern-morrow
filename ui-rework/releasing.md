# Releasing

GitHub Actions builds the release files. The release workflow, `.github/workflows/release.yml`, builds:

- a Windows installer, `ModernMorrow-<tag>-Windows-x64.exe`, made by NSIS,
- a Windows zip of the same files, `ModernMorrow-<tag>-Windows-x64.zip`, to unpack and run anywhere,
- a Linux AppImage, `ModernMorrow-<tag>-Linux-x86_64.AppImage`, for the Steam Deck and other Linux systems,
- `ModernMorrow-AutoUpdate.sh`, a script that downloads the newest AppImage and starts it, for a Steam shortcut that keeps itself up to date,
- an Android APK, `ModernMorrow-<tag>-Android-arm64.apk`, built as [android.md](android.md) describes.

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

## The Android signing key

Android only installs an update over an app signed with the same key. Every release APK must be signed with one key, kept for as long as the app exists. If it's lost, players have to uninstall the app to install a newer one, and uninstalling deletes the app's folder with their settings and saves.

The release workflow reads the key from three repository secrets:

- `ANDROID_KEYSTORE_BASE64`, the keystore file in base64,
- `ANDROID_KEYSTORE_PASSWORD`, its password, which is also the key's password,
- `ANDROID_KEY_ALIAS`, the key's name in the keystore.

To make the key once and store it, run these on a machine with a JDK and the `gh` command, outside the repository:

```bash
keytool -genkeypair -keystore modern-morrow-release.keystore -alias modern-morrow -keyalg RSA -keysize 4096 -validity 36500 -dname "CN=Modern Morrow"
```

```bash
base64 -i modern-morrow-release.keystore | gh secret set ANDROID_KEYSTORE_BASE64 --repo DurhamG/modern-morrow
```

```bash
gh secret set ANDROID_KEYSTORE_PASSWORD --repo DurhamG/modern-morrow
```

```bash
gh secret set ANDROID_KEY_ALIAS --repo DurhamG/modern-morrow --body modern-morrow
```

`keytool` asks for the password, and `gh` asks for it again. Keep the keystore file and its password somewhere safe outside GitHub, such as a password manager, since GitHub secrets can't be read back.

The APK's version code is the release workflow's run number, which only grows, so each release installs over the one before. Its version name is the tag without the `v`.

Debug APKs from branch pushes are on each run's page too. A debug build is an app of its own, `io.github.durhamg.modernmorrow.debug`, called Modern Morrow Debug, so it installs beside the released app and has its own settings and saves. Pushes to master sign it with the release key, so each master build installs over the last. Other branches and pull requests sign it with a new throwaway key each run, so installing one over another needs `adb uninstall io.github.durhamg.modernmorrow.debug` first, which deletes the debug app's settings and saves.

To build a signed release APK on the Mac, set the same three values as `ANDROID_KEYSTORE` (the keystore's path), `ANDROID_KEYSTORE_PASSWORD` and `ANDROID_KEY_ALIAS`, and run `android/build.sh release`.

## Reading a Linux crash log

When the game crashes on Linux, it writes `/tmp/openmw-crash.log`. On a Steam Deck, which has no debugger, the log lists each frame of the crashed thread as an address, the file it is in, and an offset in that file, such as `#5  0x000055d0c1a2b3c4 /tmp/.mount_ModernAbCdEf/openmw + 0x1a2b3c4`. The game's own log, `~/.config/modern-morrow/openmw.log`, ends with any message written just before the crash, such as the C library's report of corrupted memory.

The Linux build saves the debug information for its programs as the `modern-morrow-linux-symbols` artifact, on the Release workflow's run page. It isn't a file of the release. It holds one `<program>.debug` file for each program, such as `openmw.debug`, and only matches the programs from that same run. GitHub deletes artifacts after 90 days.

To name the function and line at a frame, take one off its offset, since the address is just after the call, and run:

```bash
addr2line -f -C -i -e openmw.debug 0x1a2b3c3
```

Frames in files the build didn't make, such as the C library or OpenSceneGraph, don't get names this way.

## Playing on the Steam Deck

The steps for installing the AppImage on a Steam Deck or a Steam Machine are in the README, in [Steam Deck and Steam Machine](../README.md#steam-deck-and-steam-machine).
