# Android port plan

This is the plan for running Modern Morrow on Android with a gamepad. It targets Android handhelds such as the Retroid Pocket and AYN Odin, and phones or tablets with a controller attached. There will be no touch controls. A player who has no gamepad can't play.

## What already exists

Upstream OpenMW has the basics of an Android build, and this branch inherits them:

- When CMake targets Android, it builds the game as a shared library, `libopenmw.so`, instead of an executable (`apps/openmw/CMakeLists.txt`). The Android app loads that library and SDL calls its `SDL_main`.
- `apps/openmw/androidmain.cpp` has the functions that the Java side calls through JNI. One of them adds a controller mapping for a "Virtual" gamepad, which only the touch overlays of the community ports use.
- `components/files/androidpath.cpp` gets the config, data and save folders from the Java side, through a JNI function named `Java_ui_activity_GameActivity_getPathToJni`. That name ties it to a Java class called `ui.activity.GameActivity`.
- `CI/before_script.android.sh` configures an arm64 build with the NDK. The GitLab job that runs it, `.Ubuntu_AndroidNDK_arm64-v8a`, is switched off, so nothing checks that the build still works.

Upstream has no Android app, no build of the dependencies, and no way of drawing with OpenGL ES.

## The community ports

Three community projects package OpenMW for Android. All three are GPLv3, the same license as OpenMW, so we can reuse their build steps and patches as long as we keep their copyright notices. Copies are in `D:\Code\android-ref` on the Windows machine.

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

## Storage and the game data

The desktop builds find Morrowind's data with the Qt setup wizard. Qt isn't part of the Android build, so the app needs another way.

- The app asks for "All files access" (`MANAGE_EXTERNAL_STORAGE`). This lets the engine read the data where the player put it, with normal file paths. Google Play limits that permission, but the APK is installed from GitHub, so that doesn't matter.
- On first start, a small Java screen asks for the Morrowind folder: the one that holds `Data Files` and `Morrowind.ini`. It then writes `openmw.cfg` with the `data=`, `content=` and fallback lines. On the desktop, `mwiniimporter` copies the fallback lines out of `Morrowind.ini`. On Android, either build that code into the app, or ship a ready-made list of the fallback lines. The first version can instead look in a fixed place, such as `/sdcard/Morrowind`, and show an error when it isn't there.
- Settings, saves and logs live in the app's own folder, `/sdcard/Android/data/<package>/files`. Players can reach that folder over USB.

## Steps

Each step ends in something that can be tried.

1. **Set up the tools on the Mac.** This needs Android Studio, or the SDK command-line tools, plus NDK r29 and CMake. These are downloads, and need the user's go-ahead.
2. **Build the dependencies.** Turn the cavebros Dockerfile into a CMake "superbuild" in the repository, under `android/deps/`, using `ExternalProject_Add` the way xyzz's `buildscripts/CMakeLists.txt` does. It must run on a macOS host as well as Linux, so it should use the NDK's own CMake toolchain file and not hard-code `linux-x86_64` paths. ICU needs a build for the host machine first, as `CI/before_script.android.sh` shows. The output is one folder of arm64 libraries and headers. Downloading each library's source also needs the user's go-ahead.
3. **Build `libopenmw.so` from this tree.** Configure it against that folder, with the launcher, wizard, editor and tools turned off. Port the needed engine patches as ordinary commits on this branch, one change per commit, and not as patch files. The likely set:
   - linking against gl4es,
   - shader and post-processing changes for gl4es,
   - keeping the OpenGL context when the app is sent to the background,
   - the Android folder lookup.

   Check that our own engine changes, such as the automation server in `apps/openmw/mwautomation/`, build for Android or are left out of it.
4. **Make the Android app.** Put a Gradle project in `android/app/`.
   - It holds SDL's Java files, copied from the same SDL release as the native library, because the two must match.
   - It has one activity that extends `SDLActivity`, loads the libraries, and passes the folders to the engine.
   - It runs in landscape and full screen, and traps the Back button.
   - Rename the JNI function in `androidpath.cpp` to match our package. This keeps the Java names our own.
   - Nothing for touch.
5. **Start the game.** Test on the Android emulator first. On Apple Silicon it runs arm64 system images natively. Then test on a real handheld. The game must reach the main menu, load a save and be playable with a gamepad. The automation harness should work over `adb forward`, which forwards its port to the device, so the existing scenarios can run against the phone.
6. **Fix and tune.** Expect rendering bugs from gl4es and slow frames. Pick Android defaults in `settings-default.cfg` or an Android-only defaults file. Candidates are view distance, shadows, post-processing and the resolution scale. Read Alpha3's patches for problems it has already solved.
7. **Data setup.** Replace the fixed data folder with the first-run folder picker.
8. **Release builds.** Add an Android job to `.github/workflows/release.yml` that builds the dependencies, caching them, and builds a signed APK. The signing key goes in a GitHub secret. Document the install in the README and in [releasing.md](releasing.md).

## Risks

- **Drawing.** gl4es is proven with OpenMW, but each OpenMW update can break it, and some effects may not work at all. If it can't keep up, ANGLE is the fallback. It is maintained by Google, and Alpha3 already uses it.
- **Speed.** Alpha3's README reports about 35 frames per second through the phone's own OpenGL ES on a Galaxy Tab S6, which has a 2019 Snapdragon 855. Handhelds with newer Snapdragon chips should do better, but gl4es adds its own cost.
- **Upkeep.** Every upstream merge can break the Android build. A CI job that at least builds the APK catches that early.
- **Effort.** Steps 1 to 5 are mostly mechanical. Step 6 is open-ended.
