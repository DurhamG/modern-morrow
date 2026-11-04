# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

OpenMW is an open-source reimplementation of the Morrowind engine in C++20, plus OpenMW-CS, a replacement for the Construction Set. Upstream lives on GitLab at https://gitlab.com/OpenMW/openmw. Contributions go there as merge requests.

## UI rework project

This checkout is being used to rework the in-game UI for gamepads, TVs and handhelds, on the `ui-rework` branch. Read `ui-rework/README.md` first. It links the plan, the progress log, and the automation harness docs.

- `ui-rework/harness.md` explains how to launch the game, send it keyboard, mouse and gamepad input, take screenshots, and dump the UI tree, using `python ui-rework/tools/omw.py`.
- Update `ui-rework/progress.md` after each significant piece of work.
- On this branch, every commit must build, but the unit tests only need to run at good stopping points.

## Building

CMake drives the build. The default build type is RelWithDebInfo.

### Windows (MSVC)

Windows builds need Visual Studio 2022 with the C++ desktop workload, Git for Windows, CMake, and 7-Zip. CMake and 7-Zip must be on `PATH`.

`CI/before_script.msvc.sh` downloads the prebuilt dependencies into `deps/` and configures a build directory named after the Visual Studio version, such as `MSVC2022_64`. It is a bash script. From PowerShell, run it through Git for Windows' `sh.exe`, the same way CI does. A standard Git install does not put `sh.exe` on `PATH`, so call it by its full path. Don't use a bare `bash` from PowerShell. On machines with WSL, it starts WSL's Linux bash instead of Git's, and the script fails there. Run `-h` to see every option. The common ones are:

- `-v 2022` picks the Visual Studio version.
- `-p Win64` picks the platform.
- `-t` turns on all three unit test targets.
- `-b` turns on the benchmarks.
- `-N` generates Ninja files instead of a Visual Studio solution.
- `-k` keeps the existing build directory. Without it, the script deletes and recreates the directory.

```powershell
& "C:\Program Files\Git\bin\sh.exe" CI/before_script.msvc.sh -v 2022 -p Win64 -t -k
```

The default mode generates a Visual Studio solution, `MSVC2022_64/OpenMW.sln`. That solution builds from any shell, because CMake finds Visual Studio on its own:

```powershell
cmake --build MSVC2022_64 --config RelWithDebInfo --parallel 4
cmake --build MSVC2022_64 --config RelWithDebInfo --target openmw
```

The setup script turns on `OPENMW_MP_BUILD`, which adds the `/MP` compiler flag. With `/MP`, the compiler builds the files within one project in parallel on every core. MSBuild still builds one project at a time unless `--parallel` is given. `--parallel N` lets MSBuild work on up to N projects at once, and each of those runs its own `/MP` compiles. Keep N small, because the Lua binding files are heavily templated and each compile uses a lot of memory.

The Ninja (`-N`) and NMake (`-n`) modes need the compiler on `PATH`. In those modes, the script needs `vswhere` on `PATH` too. `vswhere` ships in `C:\Program Files (x86)\Microsoft Visual Studio\Installer`. The script writes `ActivateMSVC.ps1`, `ActivateMSVC.bat` and `activate_msvc.sh` into the build directory. Run the one for your shell before each build.

Binaries land in `MSVC2022_64/RelWithDebInfo/`. That folder also gets a generated `openmw.cfg` and a `resources/` copy, so `openmw.exe` runs straight from there.

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

### Useful CMake options

- `BUILD_COMPONENTS_TESTS`, `BUILD_OPENMW_TESTS`, `BUILD_OPENCS_TESTS`: the unit test executables. All are OFF by default.
- `BUILD_BENCHMARKS`: the Google Benchmark executables in `apps/benchmarks/`.
- `BUILD_OPENMW`, `BUILD_OPENCS`, `BUILD_LAUNCHER`, `BUILD_WIZARD` and the `BUILD_<TOOL>` options: turn off applications you don't need to cut build time.
- `OPENMW_UNITY_BUILD`: fewer, larger compilation units.

### Adding source files

Source files are not globbed per directory. Each new `.cpp`/`.hpp` pair must be added by its base name to the matching `add_openmw_dir(...)` list in `apps/openmw/CMakeLists.txt`, or the `add_component_dir(...)` list in `components/CMakeLists.txt`. Test files are listed explicitly in `apps/*_tests/CMakeLists.txt`. Built-in data files (Lua scripts, textures, MyGUI layouts, l10n YAML) are listed in `files/data/CMakeLists.txt`. CMake copies them into the build's `resources/` folder at configure time, and an edit to a listed file triggers that copy again on the next build.

## Tests

Tests use Google Test. There are three executables: `components-tests` (`apps/components_tests/`), `openmw-tests` (`apps/openmw_tests/`) and `openmw-cs-tests` (`apps/opencs_tests/`).

```bash
MSVC2022_64/RelWithDebInfo/components-tests.exe --gtest_filter='SettingsFileParserTest.*'
build/openmw-tests --gtest_filter='SuiteName.TestName'
```

Integration tests run the real engine against the OpenMW example-suite game data, using Lua test scripts from `scripts/data/integration_tests/`. `CI/run_integration_tests.sh` shows how CI runs them. It is Linux-only because it uses `xvfb-run`.

## Formatting and lint checks

CI runs these checks on every merge request:

- `CI/check_clang_format.sh` runs clang-format 14 against `.clang-format` on every tracked `.cpp`/`.hpp`/`.h` file outside `extern/`. To format just the files you touched, run `clang-format -i <files>`. Avoid reformatting lines you did not otherwise change.
- `CI/check_file_names.sh` requires C++ file names to be all lowercase letters and digits, such as `windowmanagerimp.cpp`. No underscores, dashes or capitals.
- `CI/check_cmake_format.sh` checks CMake file formatting.
- `.clang-tidy` enforces naming. Types, namespaces, enums and concepts use `CamelCase`. Local variables and parameters use `camelBack`.

The main style points from `.clang-format` are 4-space indentation, a 120-column limit, and Allman braces. Nested namespaces are indented, pointers bind left (`int* p`), and constructor initializer lists break before the comma. Member variables use an `m` prefix (`mIndex`) and static members an `s` prefix (`sThis`).

## Architecture

### Layout

- `components/` holds the libraries shared by all applications. It covers file formats (`esm3/` for Morrowind data and saves, `esm4/` for later Bethesda formats, `nif/`, `bsa/`), the virtual file system (`vfs/`), rendering helpers (`nifosg/`, `sceneutil/`, `shader/`, `terrain/`), navigation meshes (`detournavigator/`), the legacy script compiler and interpreter, Lua infrastructure (`lua/`, `lua_ui/`), settings, and localisation (`l10n/`).
- `apps/openmw/` is the game. `apps/opencs/` is the editor. `apps/launcher/` and `apps/wizard/` are Qt front ends. The other folders in `apps/` are command-line tools.
- `files/` holds runtime data. That includes `settings-default.cfg`, the built-in Lua scripts in `files/data/scripts/omw/`, MyGUI layouts and skins in `files/data/mygui/`, localisation strings in `files/data/l10n/<Context>/<lang>.yaml`, and shaders in `files/shaders/`.
- `extern/` holds vendored third-party code. Don't edit or reformat it.

### The game (`apps/openmw/`)

- `engine.cpp` creates every subsystem in `OMW::Engine::prepareEngine()` and runs the main loop in `OMW::Engine::frame()`.
- `mwbase/` declares abstract interfaces such as `World`, `WindowManager`, `MechanicsManager` and `LuaManager`. Subsystems reach each other through the `MWBase::Environment::get()` singleton, for example `MWBase::Environment::get().getWindowManager()`. Each interface has its implementation in a matching folder, such as `mwgui/windowmanagerimp.cpp`.
- Game objects are handled through `MWWorld::Ptr`, which points at a live reference to a record. Behaviour that depends on the record type lives in `mwclass/`. Each type, such as NPC, creature, weapon or door, subclasses `MWWorld::Class` and has a `registerSelf()` call in `mwclass/classes.cpp`. Code calls `ptr.getClass().someMethod(ptr)` rather than switching on the record type.
- `MWWorld::ESMStore` holds the loaded content records. Record IDs are `ESM::RefId` values.
- `mwgui/` windows are MyGUI widgets built from `.layout` XML files in `files/data/mygui/`. A window class usually names its layout file in its `WindowBase` constructor.
- `mwmechanics/` holds stats, AI, combat, magic and crime. `mwrender/` builds the OpenSceneGraph scene. `mwphysics/` wraps Bullet. `mwscript/` binds the legacy Morrowind script opcodes to the game. `mwdialogue/` holds topics and the journal. `mwstate/` saves and loads games.

### Lua scripting (`apps/openmw/mwlua/`)

Read `apps/openmw/mwlua/README.md` before changing bindings. The key rules are:

- By default, Lua runs on a worker thread while the main thread runs the OpenSceneGraph cull traversal. A binding must never change anything that can affect the scene graph directly. It queues the change with `LuaManager::addAction`, and the main thread applies it later. A queued lambda should capture the Lua object, not a `Ptr`, because the `Ptr` may be invalid by the time the action runs.
- Bindings use three object types to control access. Global scripts get `GObject`, which can change anything. Local scripts get `LObject`, which is read-only. A local script's own object is `SelfObject`, which it can change.
- The Lua API reference is generated from the specs in `files/lua_api/`. Update them whenever you change a binding. Any change to the Lua API also bumps `OPENMW_LUA_API_REVISION` in the top-level `CMakeLists.txt`. Upstream usually does that in a separate commit titled "Lua API revision bump".
- Much of the gameplay logic, such as camera, controls, combat, music and the settings UI, is implemented in the built-in Lua scripts under `files/data/scripts/omw/`. They talk to the engine through these bindings.

### Settings

Each settings section is a struct in `components/settings/categories/<section>.hpp`. Each setting is a `SettingValue<T>` member, constructed with its section name and key, and optionally a sanitizer. A new setting also needs:

- its default value and a comment in `files/settings-default.cfg`,
- a description in `docs/source/reference/modding/settings/<section>.rst`.

Code reads settings with typed accessors such as `Settings::hud().mCrosshair`.

### Save games and content files

`components/esm3/formatversion.hpp` versions the save format. Any change to what gets written to a save adds a new `Max...FormatVersion` constant and bumps `CurrentSaveGameFormatVersion`. Loading code then checks the version to keep reading older saves. Older saves back to `MinSupportedSaveGameFormatVersion` must still load.

### Localisation

Strings shown to players don't belong in C++ code. Add them to `files/data/l10n/<Context>/en.yaml` and look them up through `MWBase::Environment::get().getL10nManager()->getContext("<Context>")`. Lua scripts use `core.l10n`.

## Upstream conventions

From `CONTRIBUTING.md`:

- Reference the issue in commit messages or the merge request description, such as `Bug #1234` or `Fixes #1234`.
- Keep one feature or bugfix per merge request. Every commit must build on its own, so `git bisect` keeps working.
- Prefer rebasing onto master over merging master in.
- User-visible bug fixes and features get an entry in `CHANGELOG.md` under the upcoming version, in the form `    Bug #1234: Description` or `    Feature #1234: Description`.
- OpenMW aims to be faithful to the original engine. Don't "fix" an original engine behaviour unless it was obviously unintended, has little effect on game balance, or was fixed in an official Morrowind patch.
- New features should be generic. Anything a mod could do should be done as a mod.
