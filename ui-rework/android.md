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

Both start in landscape and have 6 GB of memory, 6 cores and a 16 GB data partition. Start one with `emulator -avd MM_Fold8_Main -gpu swiftshader`. Step 5 explains why it draws in software. The emulator's frame rates say nothing about the phone. The test Fold 8 (model SM-F971U1) runs Android 17 (API 37). An image for that API level would match it better, and would be one more download.

## Storage and the game data

The desktop builds find Morrowind's data with the Qt setup wizard. Qt isn't part of the Android build, so the app needs another way.

- The app asks for "All files access" (`MANAGE_EXTERNAL_STORAGE`). This lets the engine read the data where the player put it, with normal file paths. Google Play limits that permission, but the APK is installed from GitHub, so that doesn't matter.
- On first start, a small Java screen asks for the Morrowind folder: the one that holds `Data Files` and `Morrowind.ini`. It then writes `openmw.cfg` with the `data=`, `content=` and fallback lines. On the desktop, `mwiniimporter` copies the fallback lines out of `Morrowind.ini`. On Android, either build that code into the app, or ship a ready-made list of the fallback lines. The first version can instead look in a fixed place, such as `/sdcard/Morrowind`, and show an error when it isn't there.
- Android doesn't grant "All files access" from a dialog. The player turns it on in a system settings screen. The app has to send them there, and say what to do if they refuse.
- Settings, saves and logs live in the app's own folder, `/sdcard/Android/data/io.github.durhamg.modernmorrow/files`. Players can reach that folder over USB. Changing the package name later would leave existing saves in the old folder.

## Steps

Each step ends in something that can be tried.

1. **Set up the tools on the Mac.** Done. Android Studio, the SDK, the emulator, NDK 29.0.13846066 (r29 beta 3) and Docker Desktop are installed, and the two emulator devices above exist. The Linux build downloads its own NDK r29, so the Mac's NDK is only needed if we later build there.
2. **Build the dependencies in Docker.** All Android builds run in a Linux container, on the Mac and in CI alike. Nothing cross-builds on macOS itself. `android/Dockerfile` builds every dependency into `/root/prefix` in the `modernmorrow-android-deps` image. Docker caches each dependency's layer, so a rebuild only redoes what changed. `android/build.sh deps` builds it. It started from the cavebros Dockerfile, with these changes:
   - It leaves out what OpenMW doesn't use: the other games that file also builds (UQM and dethrace), the Rust mod tools, the texture tool, libogg and libvorbis, the MinGW packages, the SDK tools and the prebuilt ANGLE libraries.
   - It builds OpenSceneGraph at `9b384d54`, the commit that `extern/CMakeLists.txt` pins, not cavebros' older `8aa8d917`. Its 11 OSG patches are copied into `android/patches/osg/`, and apply without fuzz. A 12th patch of ours skips a `dlinfo` call in that newer commit, which Android's C library doesn't have.
   - It pins every source. Cavebros cloned the heads of bzip2 and NG-GL4ES, and LuaJIT's moving `v2.1.ROLLING` tarball. NG-GL4ES is pinned at `a26bd21`, which was its branch's head when cavebros last changed its Dockerfile.
   - It turns off LTO. With LTO, every link of `libopenmw.so` would redo the code generation for all the static dependencies.
   - gl4es's config and log folder is our package's `files/config`, set with `sed`, in place of the patch that pointed it at Alpha3's folder.
   - The NDK only ships Linux tools for x86-64, so the image is `linux/amd64` and runs under Rosetta on Apple Silicon. Docker Desktop's "Use Rosetta for x86_64/amd64 emulation" setting is on, and its VM has 16 GB of memory.
   - FFmpeg is built with `--disable-asm --disable-optimizations`, as cavebros did. If video or sound decoding turns out slow on the phone, try turning optimizations back on.
3. **Build `libopenmw.so` from this tree.** Done. `android/build.sh openmw` runs `android/container-build.sh` in the image, with this checkout mounted at `/src`. The build folder and ccache live in Docker volumes, so an incremental build takes about a minute. The first build took about 9 minutes under Rosetta. The script then copies `libopenmw.so` and the shared libraries it needs into `android/app/src/main/jniLibs/`, and the built `resources/`, `openmw.cfg`, `defaults.bin` and `gamecontrollerdb.txt` into the app's assets. Both folders are ignored by git. The engine needed three commits:
   - The JNI path function is named after our package.
   - `androidmain.cpp`'s `stderr` workaround only applies below API 23, as later Android declares `stderr` itself.
   - The shader manager merges linked shaders into one shader per stage on Android. Upstream's shaders link library shaders such as `lib/core/fragment.glsl` as separate objects, and OpenGL ES links only one per stage. Cavebros didn't need this, as its OpenMW predates the links. The merging is adapted from Alpha3.

   The automation server builds for Android as it is. Keeping the OpenGL context when the app goes to the background isn't done yet.
4. **Make the Android app.** Done. `android/app/src/main/` holds the manifest, SDL 2.32.4's Java files and two activities of ours. `android/build.sh apk` builds the APK on the Mac with Gradle (`android/gradlew`, Gradle 9.6.0 and the Android Gradle plugin 9.4.1) against Android 35, and signs it with Gradle's usual debug key in `~/.android/`. The Docker build puts the native libraries and assets into `android/app/src/main/`, and Gradle packages them as they are.
   - `MainActivity` copies the resources and global config out of the APK when the APK changes, finds the data, writes the user `openmw.cfg`, then starts `GameActivity`. It passes its intent's extras on. Without the data, it says where to put it.
   - `GameActivity` extends `SDLActivity`. It loads the libraries, passes the folders to the engine, takes command-line options from the `args` extra, and sets SDL's hints for the Back button and landscape.
   - The game data is looked for in the app's own `Morrowind` folder, then in `/sdcard/Morrowind`, which needs "All files access".
5. **Start the game.** In progress. In the emulator, the game loads Morrowind, Tribunal and Bloodmoon, shows the main menu, and starts a new game in Seyda Neen with the world, sky, water and HUD drawn. On the Fold 8 it starts a new game on the prison ship, drawn on the GPU. The automation server works over `adb forward`, and its virtual gamepad moves the main menu's highlight. To start it with options:

   ```bash
   adb shell "am start -n io.github.durhamg.modernmorrow/.MainActivity --esa args '--automation-port,4242,--skip-menu,--start,Seyda Neen'"
   ```

   Found so far:
   - The emulator's default GPU mode crashed the emulator itself, in Apple's OpenGL under `glClear`. Start it with `-gpu swiftshader`, which draws in software. It is correct but slow: a frame of Seyda Neen takes seconds at 2448x1848, and Android's own processes stop responding at times.
   - The built-in `openmw.cfg` has no fallback values for the Ashstorm and Blight weathers, which on the desktop `mwiniimporter` copies from `Morrowind.ini`. Without them the built-in weather script stops with "cloudsMaximumPercent must be greater than 0". Step 7 has to import `Morrowind.ini`.
   - gl4es reports "invalid enumerant" after many draws. Nothing looks wrong yet.
   - SDL's HIDAPI controller code throws on Android 14 and later, because it registers a broadcast receiver without saying whether it is exported. Android's own controller input still works. SDL 2.32.4 has the bug; setting `SDL_JOYSTICK_HIDAPI` to 0 would avoid it.
   - Android shows its "Viewing full screen" card the first time the game starts. Check on the phone that a controller can dismiss it.
   - The Mac's Python is 3.9, where `socket.timeout` isn't yet a `TimeoutError`, so `omw.py` fails on its first slow reply. `omw.py` also has no Android launch yet: it needs to start the game through `adb`, copy a profile's settings and saves over, and pull screenshots back.

   On the Fold 8:
   - Install over USB with `adb install -r android/out/modern-morrow-debug.apk`. Wireless debugging dropped in the middle of copying the game data when the phone's Wi-Fi was poor, and USB copied the 1.1 GB in 33 seconds.
   - Copy the data with `adb push --sync ~/Morrowind/"Data Files" /sdcard/Morrowind/`, which skips files already there. `adb shell appops set --uid io.github.durhamg.modernmorrow MANAGE_EXTERNAL_STORAGE allow` gives the app the all-files access it needs to read them, as the app's own button would.
   - The phone has two displays, so `screencap` needs `-d` with an ID from `adb shell dumpsys SurfaceFlinger --display-id`.
   - If the phone is locked when the game starts, Android stops the activity at once. It comes back when the phone is unlocked.
   - The APK built with the SDK tools was signed with another key, so the emulator needs `adb uninstall io.github.durhamg.modernmorrow` before the Gradle build installs there.

   Coming back from the home screen used to show a black screen while the music played. OpenSceneGraph draws on a thread of its own by default, and that thread deadlocked with SDL's when Android took the window's surface away. The app now sets `OSG_THREADING=SingleThreaded`, so drawing happens on SDL's thread. In the emulator that was also much faster, about 20 frames a second instead of 1 to 9. Whether it costs frame rate on the phone's GPU is still to check.

   A Nintendo Switch Pro Controller paired over Bluetooth had scrambled buttons: ZR opened the pause menu, + changed the camera, and so on. The community `gamecontrollerdb.txt` that OpenMW ships has an Android entry for it whose button numbers come from an older Android driver or SDL version. Android turns every controller's buttons into its standard key codes through its own key layout files, and SDL maps those itself, so the APK no longer ships the file. A player can still put one in the user config folder.

   SDL's own mapping for the Switch Pro Controller goes by the buttons' labels, so the right button, labelled A, was A. The app sets `SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0`, so Nintendo controllers go by position, as an Xbox controller does: the bottom button is A. The tutorial's messages and the menus' hints name the Xbox buttons.

   Still to do: load a save on the Fold, play with a real controller, and measure frame rates on the Fold.
6. **Fix and tune.** Expect rendering bugs from gl4es and slow frames. Pick Android defaults in `settings-default.cfg` or an Android-only defaults file. Candidates are view distance, shadows, post-processing, the resolution scale and a UI scaling factor near 2. Any shaders or post-processing that this branch added for the UI need the same gl4es checks as upstream's. Read Alpha3's patches for problems it has already solved.
7. **Data setup.** Import `Morrowind.ini` into the user `openmw.cfg`, as `mwiniimporter` does on the desktop. Replace the fixed data folders with a first-run folder picker.
8. **Release builds.** Add an Android job to `.github/workflows/release.yml`. It builds the APK with the same Dockerfile, caches the dependency layers, and signs the APK. The signing key goes in a GitHub secret. Document the install in the README and in [releasing.md](releasing.md).

## Risks

- **Drawing.** gl4es is proven with OpenMW, but each OpenMW update can break it, and some effects may not work at all. If it can't keep up, ANGLE is the fallback. It is maintained by Google, and Alpha3 already uses it.
- **Speed.** Alpha3's README reports about 35 frames per second through the phone's own OpenGL ES on a Galaxy Tab S6, which has a 2019 Snapdragon 855. The Fold 8 and other phones with newer Snapdragon chips should do better, but gl4es adds its own cost.
- **Upkeep.** Every upstream merge can break the Android build. A CI job that at least builds the APK catches that early.
- **Effort.** Steps 1 to 5 are mostly mechanical. Step 6 is open-ended.
