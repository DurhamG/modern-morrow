# Releasing

GitHub Actions builds the release files. The release workflow, `.github/workflows/release.yml`, builds:

- a Windows installer, `ModernMorrow-<tag>-Windows-x64.exe`, made by NSIS,
- a Windows zip of the same files, `ModernMorrow-<tag>-Windows-x64.zip`, to unpack and run anywhere,
- a Linux AppImage, `ModernMorrow-<tag>-Linux-x86_64.AppImage`, for the Steam Deck and other Linux systems.

The Linux build runs on AlmaLinux 9, like OpenMW's portable Linux package, so it works on newer systems such as SteamOS. Its libraries are inside the AppImage.

Both builds run the game on SDL3 through sdl2-compat, SDL's library for running SDL2 programs on SDL3, so newer controllers such as the 2026 Steam Controller work without Steam. `CI/sdl2_compat_versions.sh` pins both releases. To update them, change the versions there and copy each file's SHA-256 digest from its GitHub release page.

## Making a release

1. Push a tag that starts with `v`, such as `git tag v0.1.0` and `git push origin v0.1.0`.
2. Wait for the Release workflow on the Actions tab. It takes about an hour.
3. It leaves a draft pre-release for the tag with the three files. Write the release notes on GitHub and publish it.

To try the builds without a release, start the Release workflow by hand from the Actions tab. The files are then on the run's page, under Artifacts.

The builds aren't signed, so Windows SmartScreen warns the first time someone runs the installer. "More info" and "Run anyway" get past it.

## Playing on the Steam Deck

1. In Desktop Mode, download the AppImage, and make it executable: right-click it, Properties, Permissions, "Is executable".
2. Run it. It opens the launcher, which finds the Morrowind data files the first time, including a Steam copy of Morrowind installed with Proton.
3. In Steam, "Add a Non-Steam Game", and pick the AppImage. In the shortcut's properties, set the launch options to `--game`, so it starts the game itself instead of the launcher.
4. Back in Game Mode, start it from the library. Steam Input passes the controller through as a gamepad.

The AppImage also takes `--wizard`, which opens the setup wizard again.
