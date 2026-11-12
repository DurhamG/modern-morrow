# Android port plan

This is the plan for running Modern Morrow on Android with a gamepad. It targets Android handhelds such as the Retroid Pocket and AYN Odin, and phones or tablets with a controller attached. There will be no touch controls. A player who has no gamepad can't play.

The app's package name is `io.github.durhamg.modernmorrow`. Testing starts in the Android emulator on the Mac, then moves to a Galaxy Z Fold 8 with a Bluetooth controller.

## What already exists

Upstream OpenMW has the basics of an Android build, and this branch inherits them:

- When CMake targets Android, it builds the game as a shared library, `libopenmw.so`, instead of an executable (`apps/openmw/CMakeLists.txt`). The Android app loads that library and SDL calls its `SDL_main`.
- `apps/openmw/androidmain.cpp` has the functions that the Java side calls through JNI. One of them adds a controller mapping for a "Virtual" gamepad, which only the touch overlays of the community ports use.
- `components/files/androidpath.cpp` gets the config, data and save folders from the Java side, through a JNI function named `Java_ui_activity_GameActivity_getPathToJni`. That name ties it to a Java class called `ui.activity.GameActivity`.
- `CI/before_script.android.sh` configures an arm64 build with the NDK. The GitLab job that runs it, `.Ubuntu_AndroidNDK_arm64-v8a`, is switched off, so nothing checks that the build still works.

Upstream has no Android app, no build of the dependencies, and no way of drawing with OpenGL ES.

## The community ports

Three community projects package OpenMW for Android. All three are GPLv3, the same license as OpenMW, so we can reuse their build steps and patches as long as we keep their copyright notices. Clones of all three are in `~/openmw-ref` on the Mac.

- [xyzz/openmw-android](https://github.com/xyzz/openmw-android) is the original port. Its last change was in 2021.
- [cavebros/openmw-android-docker](https://gitlab.com/cavebros/openmw-android-docker) is the one to copy from. It is maintained, and its `Dockerfile` pins versions that work together: NDK r29, SDL 2.32.4, Boost 1.88, FFmpeg 7.1.1, Bullet 3.25, OpenAL 1.24.3, ICU 70-1, MyGUI 3.4.3, LuaJIT 2.1, and pinned commits of OpenMW's OpenSceneGraph fork and of NG-GL4ES. Its README says it will be archived in favour of Alpha3.
- [duron27/alpha3](https://gitlab.com/duron27/alpha3) replaces it. It is a large launcher for Android and iOS, with mod downloads, multiplayer, Vulkan and many performance patches. It changes too much of the engine for us to follow, but its patches are worth reading when we chase a specific rendering or speed problem.

Each port adds three things to OpenMW:

1. **A build of every dependency** with the Android NDK.
2. **A way to draw.** OpenMW draws with desktop OpenGL, and Android only offers OpenGL ES. The ports translate one into the other with gl4es, a library that implements desktop OpenGL on top of OpenGL ES. They also patch OpenMW's shaders and post-processing for it. In the cavebros port these patches come to about 800 lines. Alpha3 also offers ANGLE and Zink, which run OpenGL on top of Vulkan, and a native OpenGL ES renderer.
3. **An Android app.** It is built from SDL's stock Java code (`SDLActivity` and friends). On top of that sit a launcher for mods, settings and the data folder, and about 1000 lines of touch controls and an on-screen keyboard.

We take the dependency recipes and the engine patches. We drop the touch controls and the launcher.

We don't take any prebuilt files from those repositories. These include ANGLE libraries, a hooking library, a code obfuscator and a signing keystore. Everything we ship is built from source.

## Gamepads

SDL's Android code turns Android's gamepad events into ordinary SDL game controllers. The game reads controllers only through SDL's GameController API. So the controller menus, glyphs and bindings should work unchanged.

Things to check on a device:

- Some devices send the B button as Android's Back key as well. SDL's `SDL_HINT_ANDROID_TRAP_BACK_BUTTON` hint stops Android from closing the app on it.
- Text entry, such as naming a character or a save, opens Android's on-screen keyboard through `SDL_StartTextInput`. Check that it can be used with a gamepad.
- Controllers that SDL doesn't recognise need a line in `files/gamecontrollerdb.txt`.
- Check whether the back paddles of Android handhelds reach the game. Some devices may map them to other buttons in their own settings.
- Glyphs come from `SDL_GameControllerGetType` (`apps/openmw/mwinput/controllermanager.cpp`). Check what type the controllers report on the phone. If they report as unknown, or the printed labels don't match, add a setting that picks the glyph set.

The emulator can't use the Mac's controllers. There, gamepad input comes from the automation harness's virtual gamepad, which SDL treats as a real controller.

## Screens

The Fold 8 has two screens, and the game should work on both. The harness has a profile for each one: `fold` is the 7.6" main screen at 2448x1848 (4:3), and `fold-cover` is the 5.5" cover screen at 1972x1248 (16:10). Both have about twice the Steam Deck's pixel density, so Android needs a UI scaling factor near 2 by default. [plan.md](plan.md) has the text sizes to check against.

Two emulator devices match these screens. Both use the Android 15 (API 35) Google Play image that was already installed:

| Device | Screen | Density |
| --- | --- | --- |
| `MM_Fold8_Main` | 1848x2448 | 400 dpi |
| `MM_Fold8_Cover` | 1248x1972 | 420 dpi |

Both start in landscape and have 6 GB of memory, 6 cores, a 16 GB data partition and the host GPU. Start one with `emulator -avd MM_Fold8_Main`. The emulator draws with OpenGL ES 3.0, translated onto the Mac's Metal, so frame rates there say nothing about the phone. The Fold 8 runs Android 16 (API 36). An API 36 image would match it better, and would be one more download.

## Storage and the game data

The desktop builds find Morrowind's data with the Qt setup wizard. Qt isn't part of the Android build, so the app needs another way.

- The app asks for "All files access" (`MANAGE_EXTERNAL_STORAGE`). This lets the engine read the data where the player put it, with normal file paths. Google Play limits that permission, but the APK is installed from GitHub, so that doesn't matter.
- On first start, a small Java screen asks for the Morrowind folder: the one that holds `Data Files` and `Morrowind.ini`. It then writes `openmw.cfg` with the `data=`, `content=` and fallback lines. On the desktop, `mwiniimporter` copies the fallback lines out of `Morrowind.ini`. On Android, either build that code into the app, or ship a ready-made list of the fallback lines. The first version can instead look in a fixed place, such as `/sdcard/Morrowind`, and show an error when it isn't there.
- Android doesn't grant "All files access" from a dialog. The player turns it on in a system settings screen. The app has to send them there, and say what to do if they refuse.
- Settings, saves and logs live in the app's own folder, `/sdcard/Android/data/io.github.durhamg.modernmorrow/files`. Players can reach that folder over USB. Changing the package name later would leave existing saves in the old folder.

## Steps

Each step ends in something that can be tried.

1. **Set up the tools on the Mac.** Done. Android Studio, the SDK, the emulator, NDK 29.0.13846066 (r29 beta 3) and Docker Desktop are installed, and the two emulator devices above exist. The Linux build downloads its own NDK r29, so the Mac's NDK is only needed if we later build there.
2. **Build the dependencies in Docker.** All Android builds run in a Linux container, on the Mac and in CI alike. Nothing cross-builds on macOS itself. `android/Dockerfile` builds every dependency into `/root/prefix` in the `modernmorrow-android-deps` image. Docker caches each dependency's layer, so a rebuild only redoes what changed. The command is in the Dockerfile's header. It started from the cavebros Dockerfile, with these changes:
   - It leaves out what OpenMW doesn't use: the other games that file also builds (UQM and dethrace), the Rust mod tools, the texture tool, libogg and libvorbis, the MinGW packages, the SDK tools and the prebuilt ANGLE libraries. The SDK tools come back when step 4 builds the APK.
   - It builds OpenSceneGraph at `9b384d54`, the commit that `extern/CMakeLists.txt` pins, not cavebros' older `8aa8d917`. Its 11 OSG patches are copied into `android/patches/osg/`, and apply without fuzz.
   - It pins every source. Cavebros cloned the heads of bzip2 and NG-GL4ES, and LuaJIT's moving `v2.1.ROLLING` tarball. NG-GL4ES is pinned at `a26bd21`, which was its branch's head when cavebros last changed its Dockerfile.
   - It turns off LTO. With LTO, every link of `libopenmw.so` would redo the code generation for all the static dependencies.
   - gl4es's config and log folder is our package's `files/config`, set with `sed`, in place of the patch that pointed it at Alpha3's folder.
   - The NDK only ships Linux tools for x86-64, so the image is `linux/amd64` and runs under Rosetta on Apple Silicon. Docker Desktop's "Use Rosetta for x86_64/amd64 emulation" setting is on, and its VM has 16 GB of memory.
   - FFmpeg is built with `--disable-asm --disable-optimizations`, as cavebros did. If video or sound decoding turns out slow on the phone, try turning optimizations back on.
3. **Build `libopenmw.so` from this tree.** Run the container with this checkout mounted into it, and a ccache volume so incremental builds stay quick. Configure it against the dependency folder, with the launcher, wizard, editor and tools turned off. Port the needed engine patches as ordinary commits on this branch, one change per commit, and not as patch files. The likely set:
   - linking against gl4es,
   - shader and post-processing changes for gl4es,
   - keeping the OpenGL context when the app is sent to the background,
   - the Android folder lookup.

   Check that our own engine changes build for Android, such as the automation server in `apps/openmw/mwautomation/`. Its socket code uses plain `sys/socket.h` outside Windows, so it should build.
4. **Make the Android app.** Put a Gradle project in `android/app/`.
   - It holds SDL's Java files, copied from the same SDL release as the native library, because the two must match.
   - It has one activity that extends `SDLActivity`, loads the libraries, and passes the folders to the engine.
   - It runs in landscape and full screen, and traps the Back button.
   - Rename the JNI function in `androidpath.cpp` to match our package, for example `Java_io_github_durhamg_modernmorrow_GameActivity_getPathToJni`. This keeps the Java names our own.
   - It passes command-line options to the engine through SDL's `getArguments()`, read from the intent's extras. The harness starts the game with `--automation-port`, through `adb shell am start`.
   - Its manifest asks for the `INTERNET` permission, which Android requires before an app can open any socket, even one on localhost.
   - Nothing for touch.
5. **Start the game.** Test in the emulator first, then on the Fold. The game must reach the main menu, load a save and be playable with a gamepad. The automation harness works over `adb forward`, which forwards its port to the device. `omw.py` needs a way to start the game through `adb` and copy a profile's settings and saves to the device, so the existing scenarios can run against it.
6. **Fix and tune.** Expect rendering bugs from gl4es and slow frames. Pick Android defaults in `settings-default.cfg` or an Android-only defaults file. Candidates are view distance, shadows, post-processing, the resolution scale and a UI scaling factor near 2. Any shaders or post-processing that this branch added for the UI need the same gl4es checks as upstream's. Read Alpha3's patches for problems it has already solved.
7. **Data setup.** Replace the fixed data folder with the first-run folder picker.
8. **Release builds.** Add an Android job to `.github/workflows/release.yml`. It builds the APK with the same Dockerfile, caches the dependency layers, and signs the APK. The signing key goes in a GitHub secret. Document the install in the README and in [releasing.md](releasing.md).

## Risks

- **Drawing.** gl4es is proven with OpenMW, but each OpenMW update can break it, and some effects may not work at all. If it can't keep up, ANGLE is the fallback. It is maintained by Google, and Alpha3 already uses it.
- **Speed.** Alpha3's README reports about 35 frames per second through the phone's own OpenGL ES on a Galaxy Tab S6, which has a 2019 Snapdragon 855. The Fold 8 and other phones with newer Snapdragon chips should do better, but gl4es adds its own cost.
- **Upkeep.** Every upstream merge can break the Android build. A CI job that at least builds the APK catches that early.
- **Effort.** Steps 1 to 5 are mostly mechanical. Step 6 is open-ended.
