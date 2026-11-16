# Automated testing plan

This is a plan for the automated tests that Modern Morrow's own code needs. It is written to be carried out one commit at a time by an agent that has not seen the analysis behind it. Every claim about the code below was checked against the `ui-rework` branch at commit `8ecdb1a9c5`.

The fork split from upstream OpenMW at `16b14001cf`. Its own tests today are three save record tests in `apps/components_tests/esm3/testsaveload.cpp` and four quicksave history tests in `apps/openmw_tests/mwstate/testquicksavemanager.cpp`. The 48 harness scenarios in `ui-rework/scenarios/` check nothing by themselves. They save pictures and UI dumps for a person to look at.

## 1. The tiers

| Tier | What it covers | Where it runs | What it costs |
| --- | --- | --- | --- |
| A. Unit tests | Rules that can be called as plain functions: save folders, quest list files, journal grouping, touch gestures, list scrolling, the ini importer, the Android shader merge, the dialogue camera maths. Google Test, inside the existing `components-tests` and `openmw-tests` programs. | GitHub CI on every push, Linux and Windows. Locally in seconds. | `openmw-tests` runs its 558 tests in about 1 second here, and `components-tests` its 1744 in about 4.5 seconds. New tests add milliseconds. The cost is the one-off work of moving rules out of UI classes. |
| B. Consistency checks | Mistakes that only show at run time and need no game: a localisation key that is missing, a MyGUI layer that doesn't exist, a layout that doesn't parse, a data file that CMake doesn't ship, an Android setting that doesn't exist. One Python script, plus one Google Test. | GitHub CI on every push. Locally in under a second. | Almost nothing. |
| C. Script tests | `ModernMorrow-AutoUpdate.sh` and the AppImage's `AppRun`, run in bash with stand-in programs on `PATH`. Optionally `actionlint` on the workflows. | GitHub CI on every push, Linux job. Locally in WSL or Git Bash, apart from `actionlint`, which isn't installed here. | A few seconds. |
| D. Android JVM tests | The app's file handling: unpacking a zip, refusing a zip entry that points outside the folder, finding `Data Files`, fixing the case of file names, the `openmw.cfg` it writes. JUnit 4 through Gradle, on a normal JVM with no device. | GitHub CI on every push, as a new job in `android.yml`. **Not on this Windows machine**, which has no Android SDK. It also runs on the Mac. | About 2 minutes of CI, in parallel with the long APK build. Gradle downloads JUnit, which is a new dependency. |
| E. Upstream's Lua integration tests | Upstream's own tests of the Lua API, run against the real engine and the free example-suite data. They guard the engine parts the fork touched: the camera scripts, the input settings script, saving and loading from Lua. | GitHub CI on every push, Linux job, under `xvfb`. | About 1 to 2 minutes, and a shallow clone of the example suite with git-lfs. |
| F. Engine smoke tests | The real engine under `xvfb`, driven through the automation server with the example-suite data: every controller screen opens, a save round trip works, a save without the controller menus' record loads, and the log has no errors. | GitHub CI on every push, Linux job. Also locally, once the example suite is cloned. | About 1 to 2 minutes of CI. It needs the first commit of the phase to find out which screens the example suite can open. |
| G. Harness checks with Morrowind's data | The existing scenarios, with checks added: text or a widget is in the UI dump, texts are in a given order, the log has no errors, and optionally the UI dump matches a stored copy. Also new scenarios for OpenMW's saves folder, archiving, the Weight tab and touch. | **Only locally**, before a release. Morrowind's data is proprietary, so public CI can't have it. | About 2 to 5 seconds per scenario and resolution, so a few minutes for all of them. |

Tiers A to F run in GitHub CI. Tier G can't.

## 2. Inventory of the fork's logic

Risk says what breaks for a player. "High" means lost saves, a game that won't start, or a screen that can't be used. "Medium" means wrong behaviour that a player notices and can work around. "Low" means a cosmetic or rare fault.

### Saves

| Logic | Where | Risk | Tier |
| --- | --- | --- | --- |
| The controller menus' save record, `ESM::UiState` ("UIST") | `components/esm3/uistate.cpp` | High. Tested already (`uiStateShouldNotChange`). | A, done |
| Legacy formats 42 and 43: recent items and spells inside the player record | `components/esm3/player.cpp` | Medium. The existing `playerShouldNotChange` test doesn't set them, so nothing checks that old Modern Morrow saves keep them. | A |
| Which formats load: up to 43 from our folder, up to 41 from OpenMW's | `StateManager::loadGame` in `mwstate/statemanagerimp.cpp` | High. A wrong limit loads a save from a newer OpenMW as if it were ours, and misreads it. | A after a move, F |
| Pinned and archived quest files, `<saves>/<character folder>.pinned-quests.txt` and `.archived-quests.txt`: written on save, removed when empty, read when a save has no UIST record | anonymous functions in `statemanagerimp.cpp` | Medium. Pins and the archive are lost when a game comes back from OpenMW. | A after a move, F |
| Choosing OpenMW's saves folder: the setting, the Flatpak folder on Linux, the newest save wins, our own folder is never chosen | `findOpenMWSaves`, `newestSave` in `statemanagerimp.cpp` | Medium. The wrong folder is shown, or our own folder is shown twice. | A after a move |
| Backing up an OpenMW save before saving over it. A failed backup must leave the save as it was. | `StateManager::backUpOpenMWSave` and the end of `saveGame` | High. A player's OpenMW save is overwritten with no copy. | A after a move, G |
| Character folders: `findCharacter`, `findOrCreateCharacter`, and a game loaded from OpenMW's folder carrying on in our folder of the same name | `mwstate/charactermanager.cpp`, `statemanagerimp.cpp` | High. Saves go into the wrong character's folder. | A, G |
| Recent items and spells: the newest first, 200 kept, items past the first 40 dropped once seen, 40 spells | `MWWorld::Player::addRecentItem`, `addRecentSpell` in `mwworld/player.cpp` | Low. "New" markers are wrong. | A |
| Journal sources matched to entries by quest, entry and day | `Journal::readUiState` in `mwdialogue/journalimp.cpp` | Low. The wrong quest giver shows. | A |
| The quicksave history | `mwstate/quicksavemanager.cpp` | High. Tested already. | A, done |
| The timed autosave: not in menus, not in combat, not during character creation | `StateManager::updateTimedAutosave` | Medium. It needs the whole engine. | G, by hand |
| Starting straight into the newest save | `apps/openmw/engine.cpp` | Medium. | G |

### Journal

| Logic | Where | Risk | Tier |
| --- | --- | --- | --- |
| Quests that share a name are one quest. Finished, pinned or archived when any of its IDs is. Notes are IDs with no name and no finishing entry. | `QuestLog::collect` in `mwgui/questlog.cpp` | Medium. | A |
| Finding the quest giver and place from the conversations' result scripts | `forEachJournalCommand`, `conversationSources`, `describeCell`, `factionFromName`, `untitledName` in `questlog.cpp` | Low. Wrong grouping on the Places and Factions tabs. | A after a small move |
| Archiving unpins, and pinning unarchives | `QuestLog::setPinned`, `setArchived` | Low. | A |
| A new journal entry takes the quest, and others with its name, out of the archive | `Journal::addEntry` in `mwdialogue/journalimp.cpp` | Medium. An archived quest that moves on stays hidden at the bottom. | A after a small move, G |
| Order of the list: Pinned, in progress (by town or faction on those tabs), Finished, Archived. Notes only on the Notes tab, unless pinned. | `JournalScreen::order`, `groupOf`, `buildRows`, `groupStarts` in `mwgui/journalscreen.cpp` | Medium. | A after a move |
| Topic links in the entries | `JournalScreen::linkTopics` | Low. | A after a move |
| Topics marked as new until heard | `DialogueManager::mHeardAnswers` in `mwdialogue/dialoguemanagerimp.cpp` | Low. | G |

### Touch controls

| Logic | Where | Risk | Tier |
| --- | --- | --- | --- |
| Screen zones: the corners for quicksave, quickload and sneak, the left half to move, the right half to look | `TouchManager::roleForGame` in `mwinput/touchmanager.cpp` | High on Android. The game can't be played if a zone is wrong. | A after a move |
| Edge swipes: the middle half of each side edge, 5 mm wide, decided after 8 mm inwards | `edgeAt`, `updateEdge` | High on Android. | A after a move |
| Tap or drag: 250 ms and 3 mm | `isTap` | High on Android. | A after a move |
| Walking: a 2 mm dead zone, full speed at 15 mm | `setMovement` | Medium. | A after a move |
| Looking: direct turning inside a 10 mm circle, then a turn speed that grows over the next 20 mm | `look`, `keepTurning` | Medium. | A after a move |
| Switching tabs every 12 mm of a sideways drag, and scrolling every 6 mm | `menuSwipeTabs`, `menuScroll` | Medium. | A after a move |
| The mode picker: the item nearest the thumb | `updatePicker` | Medium. | A after a move |
| Millimetres to pixels from the display's DPI | `pixelsPerMm` | High on Android. Every distance above depends on it. | A after a move |
| The tutorial's record file, and when the tutorial shows again: a new character, or 14 days away | `readTutorialRecord`, `writeTutorialRecord`, `checkTutorial` | Low. | A after a move |
| What a tap does in each mode, quick keys by touch, taps in menus | the rest of `touchmanager.cpp`, `mwgui/touchoverlay.cpp` | Medium. | G, with the harness's `touch` command |
| Drawing the fingers for videos | `mwgui/touchtrace.cpp` | Low. | none |

### Other rules inside UI and input code

| Logic | Where | Risk | Tier |
| --- | --- | --- | --- |
| Scrolling a controller list: the focused row stays in view with its group header, and the list never scrolls past its end. The wheel focuses the item under the pointer. | `ControllerList::draw` in `mwgui/controllerscreen.cpp` | Medium. Every list in every screen uses it. | A after a move |
| The tab row's status line going onto more lines on a narrow screen, and button hints wrapping onto a second row | `ControllerUi::drawTabRow`, `drawHints` | Low. | A after a move |
| The d-pad's quick keys: a press uses slots 1 to 4, a hold of 0.35 seconds uses 5 to 8, and a press that starts in a menu does nothing | `ActionManager::updateDpadQuickKeys` in `mwinput/actionmanager.cpp`, `mwgui/quickkeyslots.hpp` | Medium. | A after a move |
| Run Fatigue: scales what running costs beyond what comes back | `mwmechanics/character.cpp` | Medium. It changes game balance if wrong. | A after a move, G |
| The inventory Weight tab's sort, by a stack's total weight | `mwgui/inventoryscreen.cpp` | Low. | G |
| Layers sized by a fixed height, and their coordinates on the screen | `components/myguiplatform/scalinglayer.cpp` | Medium. The mouse and touch point at the wrong place if wrong. | A after a move |
| Keys standing in for controller buttons in the menus | `ControllerManager::keyToMenuButton` in `mwinput/controllermanager.cpp` | Medium. | F, G |
| Morrowind's Xbox messages naming the player's own buttons | `mwscript/interpretercontext.cpp` | Low. | none |
| The HUD's fades, the item comparisons, the stats screen's level-up numbers | `mwgui/hud.cpp`, `itemdetails.cpp`, `statsscreen.cpp` | Low. | G |

### Setup, platform and tools

| Logic | Where | Risk | Tier |
| --- | --- | --- | --- |
| The ini importer leaving out archives that aren't in any data folder, whatever their capitalisation | `MwIniImporter::importArchives` in `apps/mwiniimporter/importer.cpp` | High. The game won't start when an archive in `openmw.cfg` is missing. | A, with a CMake change |
| Android: unpacking the zip, the guard against entries outside the folder, finding `Data Files`, fixing name case, moving `Morrowind.esm` last | `DataImport.java` | High on Android. A bad zip could write outside the app's folder, and a wrong move leaves a half-installed game. | D |
| Android: the user `openmw.cfg`, and pointing the global one at the copied resources | `Files.java` | High on Android. | D |
| Android: merging linked shaders into one per stage | `declareUniformOnce`, `foldForMerge` in `components/shader/shadermanager.cpp`, inside `#ifdef ANDROID` | High on Android. A wrong merge fails to compile every shader, which is a black screen. No desktop build compiles this code today. | A after a move |
| Android's default settings and its extra `Morrowind.ini` values | `android/settings.cfg`, `android/morrowind-ini.cfg` | Medium. A misspelled key is ignored silently. | B |
| The dialogue camera framing | `files/data/scripts/omw/camera/dialogue_framing.lua` | Low. | A, run from C++ with stand-in `openmw.*` packages |
| The 3D map and the local map's fog | `mwrender/map3d.cpp`, `mwrender/localmap.cpp` | Medium. | G only. It is rendering. |
| The update script following GitHub's "Latest" release | `CI/linux/appimage/ModernMorrow-AutoUpdate.sh` | High on the Steam Deck. A bug stops the game starting, or stops updates for good. | C |
| The AppImage's start script running the wizard first | `CI/linux/appimage/AppRun` | Medium. | C |
| The workflows | `.github/workflows/*.yml` | Medium. A mistake shows only on the next tag. | C, `actionlint`, in CI only |
| Localisation keys, layers, layouts, shipped data files | `files/data/` | Medium. A missing key shows as raw text, and a missing layer hides a whole panel without an error. | B |
| The automation server's JSON reader | `apps/openmw/mwautomation/json.cpp` | None for players. The harness depends on it. | A |
| The harness client | `ui-rework/tools/omw.py` | None for players. | Python `unittest`, in CI |

### What the earlier gap list missed

These are in the tables above, and are listed here so they aren't overlooked.

1. **The Android shader merge is untested text handling inside `#ifdef ANDROID`.** No desktop build compiles it. It is two pure functions and can be moved out and tested everywhere.
2. **`CharacterManager::findCharacter` ignores case**, though it was described as matching the exact folder name. `findOrCreateCharacter` creates the folder with the name as given. The tests should pin this down as it is. Whether it is right on Linux, where `Lendias` and `lendias` can both exist, is an open question below.
3. **A missing settings default is already caught.** `SettingValue`'s constructor throws when `files/settings-default.cfg` has no value, and both `components-tests` and `openmw-tests` build every setting in `main()`. So a new setting without a default makes both programs fail to start. No new test is needed for that direction.
4. **Two upstream layouts are not well-formed XML.** `openmw_console.layout` has a bare `<` in an attribute, and `openmw_settings_window.layout` repeats the `name` attribute on one widget. MyGUI's own parser accepts both. So the layout check must use MyGUI's parser, not a strict one.
5. **All three consistency checks pass today.** Every `#{Context:Key}` in the C++ and layouts has an `en.yaml` entry, every layer named in `createPanelRoot` exists, and every file under `files/data/` is listed in `files/data/CMakeLists.txt`. The checks can go in without any fixes.
6. **Nine places build a localisation key from a prefix and a name**, such as `"#{Interface:InventoryTab" + name + "}"`. A literal search can't see those keys. The check can only require that some key has the prefix.
7. **Legacy saves' recent items** have no test, as the Saves table says.
8. **The d-pad quick key timing, the tutorial record, `MWWorld::Player`'s recent lists, and the scaling layer's coordinate maths** are small rules with no test.
9. **Upstream has a second Lua test suite, `scripts/data/morrowind_tests/`**, which runs against Morrowind's data through the same portable `scripts/integration_tests.py`. It belongs in the local pre-release run.
10. **GitHub CI doesn't run upstream's format and file name checks** (`CI/check_file_names.sh`, `CI/check_clang_format.sh`). The file name check needs nothing installed.
11. **This machine has clang-format 21, not 14.** It is at `C:\Program Files\LLVM\bin`, off `PATH`. CI won't check formatting (see the decisions below), so new C++ is formatted by hand to the `.clang-format` rules, with version 21 as a guide for the lines a commit adds.
12. **In CI there is no real controller**, so the smoke tier can test what the local harness can't: the d-pad in the pause menu and the journal, and the sticks.

## 3. The commits

### Rules for every commit

- Each commit builds and passes all tests on its own. Build with `cmake --build MSVC2022_64 --config RelWithDebInfo --target openmw-tests` or `--target components-tests`. Add `-- //p:CL_MPCount=8` if memory is short. Run with `MSVC2022_64/RelWithDebInfo/openmw-tests.exe --gtest_filter='Suite.*'`.
- GitHub CI already runs the whole of `components-tests` and `openmw-tests` on Linux and Windows for every push (`.github/workflows/push.yml` and `windows.yml`). A new Google Test needs no workflow change. Only commits marked "CI change" touch a workflow.
- A commit that moves logic changes no behaviour and adds no tests. The tests come in the next commit. This keeps the move easy to review.
- New C++ files have lowercase names with no underscores. A new `.cpp`/`.hpp` pair in `apps/openmw/<dir>/` is added by its base name to that directory's `add_openmw_dir(...)` list in `apps/openmw/CMakeLists.txt`. One in `components/<dir>/` goes in the `add_component_dir(...)` list in `components/CMakeLists.txt`. A new test file is added to the `file(GLOB UNITTEST_SRC_FILES ...)` list in `apps/openmw_tests/CMakeLists.txt` or `apps/components_tests/CMakeLists.txt`.
- Tests include the code under test as `"apps/openmw/mwstate/questlists.hpp"`, as the existing tests do.
- Tests that need files use `TestingOpenMW::outputDirPath(...)` from `components/testing/util.hpp`, which makes a folder under the system's temporary folder that the test program removes when every test passes.
- No Lua API changes are planned, so `OPENMW_LUA_API_REVISION` stays as it is.
- Update `ui-rework/progress.md` at the end of each phase, and `ui-rework/harness.md` in the same commit as any change to the harness's commands.
- Commit messages open with the outcome in plain words. The titles below are suggestions.

Each phase ends at a good stopping point. Phases 1 to 6 don't depend on each other, so they can be done in any order after phase 0. Phase 8 needs phase 7.

---

### Phase 0: checks that need no code changes

#### C01. Check localisation keys, layers and shipped data files in CI

- **Adds** `CI/check_ui_data.py`, using only Python's standard library. It exits with 1 and prints each problem with its file and line.
- **Changes** `.github/workflows/push.yml`: a step `Check UI data files` in the `Ubuntu` job, straight after the checkout, running `python3 CI/check_ui_data.py`. It runs before the build, so a mistake fails in seconds. **CI change.**
- **Checks**, each a function in the script:
  - `check_l10n_keys`: every literal `#{Context:Key}` in `apps/openmw/**/*.cpp`, `*.hpp`, `components/**/*.cpp`, `*.hpp` and `files/data/mygui/*` has `Key` in `files/data/l10n/<Context>/en.yaml`. Tags without a colon, such as `#{sJournal}`, are Morrowind's own strings and are skipped. The one example in a comment, `#{Context:Key}` in `mwgui/controllerscreen.hpp`, is on an ignore list in the script.
  - `check_l10n_prefixes`: for every key built from a prefix, matched by `"#\{(\w+):(\w*)" *\+`, some key in that context's `en.yaml` starts with the prefix. There are nine such places today.
  - `check_layers`: every layer named in `createPanelRoot("...")` in `apps/openmw/mwgui/`, and every `layer="..."` in `files/data/mygui/*.layout`, is a `<Layer name="...">` in `files/data/mygui/openmw_layers.xml`.
  - `check_data_files_listed`: every file under `files/data/`, apart from `CMakeLists.txt`, is named in `files/data/CMakeLists.txt`.
  - `check_android_settings`: every `[Section]` and key in `android/settings.cfg` is in `files/settings-default.cfg`.
- **Run locally:** `python CI/check_ui_data.py`. It passes on the current tree, which was checked while writing this plan.

#### C02. Test that every MyGUI layout parses

- **Adds** `apps/openmw_tests/mwgui/testlayouts.cpp`, and lists it in `apps/openmw_tests/CMakeLists.txt`.
- **How:** the test walks `OPENMW_PROJECT_SOURCE_DIR/files/data/mygui/` and opens every `.layout` and `.xml` file with `MyGUI::xml::Document::open(std::istream&)`, which is the parser the game uses. It needs no MyGUI start-up. If it turns out to need one, fall back to a Python check in `CI/check_ui_data.py` with the two upstream files from finding 4 on an allow list.
- **Test cases:**
  - `MWGuiLayoutsTest.everyLayoutShouldParse`: `open` returns true for each file. The failure message names the file and `getLastError()`.
  - `MWGuiLayoutsTest.everyLayoutShouldHaveAMyGuiRoot`: each file's root element is `MyGUI`.
- **Run:** `openmw-tests --gtest_filter='MWGuiLayoutsTest.*'`.

#### C03. Test the Steam Deck update script against a stand-in GitHub

- **Adds** `CI/test_auto_update.sh` and `CI/testdata/autoupdate/release.json`, a cut-down copy of GitHub's description of a release with one AppImage file, its `size`, `digest` and `browser_download_url`.
- **How:** for each case the test copies `CI/linux/appimage/ModernMorrow-AutoUpdate.sh` into a new temporary folder, since the script keeps its AppImage and log beside itself. It puts a folder with a stand-in `curl` first on `PATH`. The stand-in reads what to do from environment variables: the HTTP status, the body, whether to fail, and what to write for a download. `DISPLAY` and `WAYLAND_DISPLAY` are unset so no window is tried. The "AppImage" is a two-line shell script that prints its arguments, and the release JSON's digest is worked out from it with `sha256sum`.
- **Changes** `.github/workflows/push.yml`: a step `Test the update script` in the `Ubuntu` job, running `CI/test_auto_update.sh`. **CI change.**
- **Test cases**, each a bash function that prints `ok` or `FAILED` with its name:
  - `no_latest_release_starts_existing_copy`: status 404. The existing AppImage runs, and the log says no release is marked as the latest.
  - `server_error_starts_existing_copy`: status 500.
  - `unreachable_starts_existing_copy`: `curl` exits with 6.
  - `up_to_date_downloads_nothing`: the digest matches the copy. The stand-in records that no download was asked for.
  - `newer_release_is_downloaded_and_started`: the digest differs. The new file replaces the copy, is executable, and runs with the script's arguments.
  - `failed_download_keeps_existing_copy`: the download exits with an error. The copy is unchanged and no `.download` file is left.
  - `wrong_checksum_keeps_existing_copy`: the downloaded bytes don't match the digest.
  - `first_run_without_network_fails_with_message`: no copy and status 404. The script exits with 1 and says where the log is.
  - `release_without_appimage_is_ignored`: the JSON lists only a Windows zip.
  - `arguments_reach_the_appimage`: `--launcher` is passed through.
- **Run locally:** `bash CI/test_auto_update.sh` in WSL. Git Bash has `sha256sum` and `awk` too, but `readlink -f` and `stat -c` behave differently there, so trust WSL or CI.

#### C04. Test the AppImage's start script

- **Adds** `CI/test_apprun.sh`.
- **How:** like C03. The test copies `CI/linux/appimage/AppRun` into a temporary folder beside stand-in `openmw`, `openmw-launcher` and `openmw-wizard` programs that record that they ran, and sets `XDG_CONFIG_HOME` to a temporary folder. Read `AppRun` first and list its branches. The cases below are the ones its comments describe.
- **Changes** `.github/workflows/push.yml`: the step from C03 also runs `CI/test_apprun.sh`. **CI change.**
- **Test cases:**
  - `no_config_runs_the_wizard_then_the_game`.
  - `config_with_a_game_file_starts_the_game`: `openmw.cfg` has a `content=` line, so only `openmw` runs.
  - `wizard_cancelled_does_not_start_the_game`.
  - `data_folder_without_game_file_opens_the_launcher`.
  - `launcher_option_opens_the_launcher`, `wizard_option_runs_the_wizard`, `game_option_starts_the_game`.

#### C05. Run upstream's Lua integration tests in CI

- **Changes** `.github/workflows/push.yml`: after the unit test steps of the `Ubuntu` job, a step `Run integration tests` with `OPENMW_BINARY: ./openmw` and `EXAMPLE_SUITE_REVISION` set to the value in `.gitlab-ci.yml` (`a1ba6fc9d7315c2158ef4c60349fc4e71b7ca343`), running `CI/run_integration_tests.sh`. Add a step that uploads `integration_tests_output/` as an artifact when the job fails. The runner image has `xvfb` and `git-lfs`. If either is missing, install it with `apt-get` in the step. **CI change.**
- **No new tests.** This runs upstream's `scripts/data/integration_tests/test_lua_api/` suite, which includes saving and loading a game from Lua and the camera API.
- **Expect to fix things.** These tests have never run against the fork. Its defaults differ from upstream's, such as `load newest save`, `autosave interval`, `quicksave history`, `controller menus` and `window mode`. If a test fails because of a default, override the setting in the step. `scripts/integration_tests.py` takes `--config <dir>` for an extra `settings.cfg`, so add `CI/integration_tests_settings/settings.cfg` for the overrides. If a test fails because the fork broke something, that is a real finding: stop and report it.
- **Run locally:** this needs the example suite, which is a download. See the open questions. With it cloned into `example-suite/`, the Python driver runs on Windows without `xvfb`: `python scripts/integration_tests.py --omw MSVC2022_64/RelWithDebInfo/openmw.exe --workdir integration_tests_output --config example-suite --data example-suite/game_template/data --data example-suite/example_animated_creature/data --data example-suite/the_hub/data --data example-suite/integration_tests/data --data example-suite/example_static_props/data scripts/data/integration_tests`.

#### C06. Check file names, and lint the workflows, in CI

- **Changes** `.github/workflows/push.yml`: a new small job `Lint` with two steps. The first runs `CI/check_file_names.sh`, which needs nothing installed. The second runs `actionlint` from its published Docker image, `docker run --rm -v "$PWD:/repo" -w /repo rhysd/actionlint:latest -color`. **CI change.**
- **CI only.** `actionlint` isn't installed here and this plan doesn't install it. Expect it to report existing problems in the workflows on its first run. Fix them in this commit, or in a commit before it if there are many.
- Skip the `actionlint` step if the user prefers not to pull a third-party image. See the open questions.

**Stopping point.** CI now catches missing keys, layers and data files, broken layouts, a broken update script, and regressions in upstream's Lua tests.

---

### Phase 1: save logic

#### C07. Move the pinned and archived quest files into their own source file

- **Adds** `apps/openmw/mwstate/questlists.hpp` and `questlists.cpp`. Adds `questlists` to `add_openmw_dir (mwstate ...)`.
- **Moves**, unchanged, from the anonymous namespace at the top of `apps/openmw/mwstate/statemanagerimp.cpp`: `sPinnedQuests`, `sArchivedQuests`, `questListPath`, `writeQuestList` and `readQuestList`, into `namespace MWState`. The comment that explains the files moves with them.
- **Changes** `statemanagerimp.cpp` to include the new header.
- No tests. The existing tests still pass.

#### C08. Test the pinned and archived quest files

- **Adds** `apps/openmw_tests/mwstate/testquestlists.cpp`, listed in `apps/openmw_tests/CMakeLists.txt`.
- **Test cases** (suite `MWStateQuestListsTest`):
  - `pathShouldSitBesideTheCharacterFolder`: for `<saves>/Lendias`, the pinned file is `<saves>/Lendias.pinned-quests.txt`.
  - `pathShouldKeepDotsInTheFolderName`: a folder named `J. Doe` gives `J. Doe.pinned-quests.txt`, not a name cut at the dot.
  - `shouldReadBackWhatWasWritten`: several IDs, in order.
  - `shouldKeepIdsWithSpacesAndUpperCase`: such as `"MS_Fargoth Ring"`, through `serializeText` and `deserializeText`.
  - `emptyListShouldRemoveTheFile`: write a list, then write an empty one, and the file is gone.
  - `emptyListShouldNotFailWhenThereIsNoFile`.
  - `missingFileShouldReadAsEmpty`.
  - `blankLinesShouldBeSkipped`.
  - `damagedLineShouldKeepTheIdsBeforeIt`: a line that `deserializeText` rejects stops the read with a warning, and the earlier IDs are returned. If `deserializeText` accepts any text, drop this case.
  - `pinnedAndArchivedShouldBeSeparateFiles`.

#### C09. Move the rules for OpenMW's saves folder into their own source file

- **Adds** `apps/openmw/mwstate/savefolders.hpp` and `savefolders.cpp`. Adds `savefolders` to `add_openmw_dir (mwstate ...)`.
- **Moves and splits**, with no change in behaviour, into `namespace MWState`:
  - `newestSave(saves)`, as it is.
  - `findOpenMWSaves(ownSaves)` becomes two functions. `openMWInstalls()` returns the candidate pairs of config folder and saves folder: the Flatpak's on Linux, then `Files::FixedPath<>("openmw")`'s. `chooseOpenMWSaves(setting, installs, ownSaves)` holds the rest: the setting wins, an install needs its config folder or its saves folder to exist, our own folder is skipped, and the newest save wins. `findOpenMWSaves` stays in `statemanagerimp.cpp` as a three-line call of the two.
  - `StateManager::isOpenMWSave` becomes a call of `isSaveInFolder(filepath, saves)`.
  - `StateManager::backUpOpenMWSave` becomes a call of `backUpSave(filepath, backupsFolder, now)`, which returns the copy's path. `backupsFolder` is `<our user data>/backups/OpenMW`.
  - The format check in `loadGame` becomes `isSaveFormatTooNew(version, fromOpenMW)`.
  - The condition for reading the quest files becomes `usesQuestListFiles(hasUiState, version)`.
  - The end of `saveGame`, from "A save in OpenMW's folder may hold a game played there" to the check of `filestream.fail()`, becomes `writeSaveFile(stream, path, backupsFolder)`, where an empty optional `backupsFolder` means no backup. It throws what the code throws today.
- No tests.

#### C10. Test the rules for OpenMW's saves folder

- **Adds** `apps/openmw_tests/mwstate/testsavefolders.cpp`, listed in CMake. The tests build small folder trees under `TestingOpenMW::outputDirPath`, and set file times with `std::filesystem::last_write_time`.
- **Test cases** (suite `MWStateSaveFoldersTest`):
  - `newestSaveShouldBeTheLatestFileInAnyCharacterFolder`.
  - `newestSaveShouldIgnoreFilesBesideTheCharacterFolders`: such as `Lendias.pinned-quests.txt`.
  - `newestSaveOfAMissingFolderShouldBeTheOldestTime`.
  - `settingShouldWinOverEveryInstall`.
  - `settingShouldBeUsedEvenWhenTheFolderDoesNotExist`: this is what the code does today. `rescanSaveFolder` then shows no characters.
  - `installWithoutConfigOrSavesFolderShouldBeSkipped`.
  - `installWithOnlyAConfigFolderShouldBeFound`: OpenMW is installed but has never saved.
  - `ownSavesFolderShouldNeverBeChosen`: an install whose saves folder is our own.
  - `installSavedToMostRecentlyShouldWin`: two installs, standing for the Flatpak and the usual folder.
  - `firstInstallShouldWinWhenNeitherHasSaves`.
  - `noInstallShouldGiveAnEmptyPath`.
  - `saveInFolderShouldMatchTwoLevelsUp`: `<saves>/Lendias/1.omwsave` is in `<saves>`, and `<saves>/1.omwsave` is not.
  - `backupShouldGoUnderTheCharactersName`: `<backups>/Lendias/<stem> <timestamp>.omwsave`, with the same bytes.
  - `backupOfAMissingSaveShouldThrow`.
  - `ownFolderShouldLoadFormats42And43`: `isSaveFormatTooNew(43, false)` is false, and `isSaveFormatTooNew(44, false)` is true.
  - `openMWFolderShouldRefuseFormatsAbove41`: `isSaveFormatTooNew(42, true)` is true, and `isSaveFormatTooNew(41, true)` is false.
  - `saveWithoutUiStateShouldUseTheQuestFiles`: true for format 41 with no record. False for format 43 with no record, whose pins are in its quest records. False when the record is there.
  - `writeShouldReplaceTheFile`.
  - `writeIntoOpenMWShouldBackUpTheOldFileFirst`.
  - `failedBackupShouldLeaveTheOldFileAsItWas`: make the backups folder's path an ordinary file, so `create_directories` throws. `writeSaveFile` throws, and the old save has its old bytes.
  - `writeOfANewFileShouldNeedNoBackup`.

#### C11. Test character folders

- **Adds** `apps/openmw_tests/mwstate/testcharactermanager.cpp`, listed in CMake. No code moves. `CharacterManager` and `Character` only need folders.
- **Test cases** (suite `MWStateCharacterManagerTest`):
  - `findCharacterShouldMatchTheFolderName`.
  - `findCharacterShouldIgnoreCase`: this pins down what `findCharacter` does today, which is `ciEqual`. See the open questions.
  - `findCharacterShouldReturnNullForAnUnknownName`.
  - `findOrCreateShouldReturnTheExistingCharacter`: the same pointer as `findCharacter`.
  - `findOrCreateShouldCreateAFolderWithTheGivenName`: the path is `<saves>/<name>` exactly, without the suffix that `createCharacter` adds to avoid clashes.
  - `createCharacterShouldNotReuseAnExistingFolder`: two characters with the same name get different folders. This is upstream's behaviour, tested so a later change to `findOrCreateCharacter` can't leak into it.
  - `getPathShouldBeTheSavesFolder`.

#### C12. Test that old Modern Morrow saves keep their recent items and spells

- **Changes** `apps/components_tests/esm3/testsaveload.cpp`.
- **Test cases:**
  - `Esm3SaveLoadRecordTest.playerShouldKeepRecentItemsInModernMorrowFormats`: an `ESM::Player` with two recent items, one seen, and two recent spells. For formats above `MaxNoRecentItemsFormatVersion` they come back. For other formats they come back empty. Set the player's other fields as `playerShouldNotChange` does.
- **Run:** `components-tests --gtest_filter='*Esm3SaveLoadRecordTest.player*'`.

#### C13. Test the player's lists of recent items and spells

- **Adds** `apps/openmw_tests/mwworld/testplayerrecent.cpp`, listed in CMake. `MWWorld::Player` can be built from an `ESM::NPC` on the stack, and these functions touch nothing else.
- **Test cases** (suite `MWWorldPlayerRecentTest`):
  - `newItemShouldGoFirstAndBeNew`.
  - `addingAnItemAgainShouldMoveItFirstAndMakeItNew`.
  - `markingAnItemSeenShouldClearNew`.
  - `unknownItemShouldNotBeNew`.
  - `seenItemsPastTheFirst40ShouldBeDropped`.
  - `unseenItemsPastTheFirst40ShouldBeKept`.
  - `atMost200ItemsShouldBeKept`.
  - `recentSpellShouldGoFirstWithoutDuplicates`.
  - `atMost40SpellsShouldBeKept`.
  - `uiStateShouldCarryBothLists`: `writeUiState` then `readUiState` on another player.

**Stopping point.** The save rules that can lose a player's data are covered in CI.

---

### Phase 2: journal rules

#### C14. Share the test helpers that build dialogue records

- **Adds** `apps/openmw_tests/mwworld/dialoguecontent.hpp`.
- **Moves** `DialogueData`, `generateDialogueWithInfos`, `saveDialogueWithInfos` and `loadEsmStore` out of the anonymous namespace near the end of `apps/openmw_tests/mwworld/teststore.cpp` into the header, as `inline` functions in `namespace MWWorld::Testing`. `teststore.cpp` includes the header. No test changes.
- Header-only, so no CMake change.

#### C15. Give the journal a function that takes a quest out of the archive

- **Changes** `apps/openmw/mwdialogue/journalimp.hpp` and `journalimp.cpp`.
- **Moves** the loop at the end of `Journal::addEntry`, which clears the archived flag of the quest and of the quests that share its name, into a new public member `void Journal::unarchiveQuest(const ESM::RefId& id)`. `addEntry` calls it. It is on `MWDialogue::Journal` only, not on the `MWBase::Journal` interface.
- **Why:** `addEntry` shows a message through the window manager and reads the date from the world, so a unit test can't call it.

#### C16. Test the journal's part of the controller menus' save record

- **Adds** `apps/openmw_tests/mwdialogue/testjournal.cpp` and a fixture header `apps/openmw_tests/mwdialogue/journalfixture.hpp`, with the test file listed in CMake.
- **The fixture** `JournalFixture`, which phase 2's other tests reuse:
  - It has the members that `MWLuaMWScriptBindingsTest` in `apps/openmw_tests/mwlua/testmwscriptbindings.cpp` has: an `MWBase::Environment`, an `MWWorld::World{ nullptr, -1, {}, {} }` and its `ESMStore`. It also has an `MWDialogue::Journal`, set with `mEnvironment.setJournal`.
  - `addQuest(id, name, finishingStages)` loads an `ESM::Dialogue` of type `Journal` into the store through `loadEsmStore` from C14. Its infos are one with `QS_Name` holding the name, when the name isn't empty, and one per stage, with `QS_Finished` on the finishing stages.
  - `addEntries(...)` writes `ESM::JournalEntry` and `ESM::QuestState` records into a stream with `ESM::ESMWriter`, as `saveAndLoadRecord` in `apps/components_tests/esm3/testsaveload.cpp` does, then feeds them to `Journal::readRecord`. This is how a save fills the journal, and it needs no window manager.
- **Test cases** (suite `MWDialogueJournalTest`):
  - `writeUiStateShouldListPinnedAndArchivedQuests`.
  - `writeUiStateShouldLeaveOutEntriesWithNoActorOrCell`.
  - `readUiStateShouldGiveEntriesTheirActorAndCell`: matched by quest, entry ID and day.
  - `readUiStateShouldNotMatchAnEntryFromAnotherDay`.
  - `readUiStateShouldSkipQuestsTheJournalDoesNotHave`: a pinned ID from the quest file that this save never started.
  - `uiStateShouldRoundTrip`: write, clear the flags, read, and the flags are back.
  - `unarchiveShouldClearTheQuest`.
  - `unarchiveShouldClearOtherQuestsWithTheSameName`: names compare without case.
  - `unarchiveShouldLeaveOtherQuestsAlone`.
  - `unarchiveShouldNotJoinQuestsWithoutNames`: two unnamed IDs, one archived. Unarchiving the other leaves it.

#### C17. Make the quest log's text rules callable

- **Changes** `apps/openmw/mwgui/questlog.hpp` and `questlog.cpp`.
- **Moves** out of the anonymous namespace into `namespace MWGui::QuestLog`, declared in the header:
  - `forEachJournalCommand` becomes `std::vector<std::pair<ESM::RefId, int>> journalCommands(std::string_view script)`. `conversationSources` loops over what it returns.
  - The town part of `describeCell` becomes `std::string townOf(std::string_view place)`, the text before the first `,` or `:`.
  - `untitledName` takes the entry's text: `std::string untitledName(std::string text)`.
- **Changes** `conversationSources()`: its function-local `static` map becomes a file-level `std::optional`, with a new `void resetContentCache()` that clears it. The game never calls it. Tests call it in `SetUp`, because each test has its own content.
- No tests.

#### C18. Test how the journal's quests are gathered

- **Adds** `apps/openmw_tests/mwgui/testquestlog.cpp`, listed in CMake. Uses `JournalFixture` from C16, and calls `QuestLog::resetContentCache()` in `SetUp`. The tests don't call `QuestLog::timestamp`, which needs MyGUI's language manager.
- **Test cases** (suite `MWGuiQuestLogTest`):
  - `journalCommandShouldGiveTheQuestAndStage`: `Journal FG_RatHunt 10`.
  - `journalCommandShouldAllowQuotesAndCommas`: `Journal "FG_RatHunt", 10`.
  - `journalCommandOnAnotherObjectShouldCount`: `"arrille"->Journal ms_fargothring 10`.
  - `journalCommandInACommentShouldBeIgnored`: `; Journal x 10`.
  - `journalCommandWithoutANumberShouldBeIgnored`.
  - `scriptWithSeveralCommandsShouldGiveThemAll`.
  - `townShouldBeTheTextBeforeAComma`: `Balmora, Guild of Fighters` gives `Balmora`.
  - `townShouldBeTheTextBeforeAColon`: `Mournhold Temple: Reception Area`.
  - `townOfAPlainNameShouldBeTheName`.
  - `untitledNameShouldStopAtTheFirstSentence`.
  - `untitledNameShouldBreakLongTextAtAWord`: over 48 characters ends at a space with `...`.
  - `questsWithTheSameNameShouldBeOneQuest`: two IDs with one name give one `Quest` with both in `mIds` and all their entries, oldest first.
  - `questsWithoutANameShouldStayApart`.
  - `questShouldBeFinishedWhenAnyIdIs`. The same for `mPinned` and `mArchived`.
  - `unnamedIdWithNoFinishingEntryShouldBeANote`.
  - `unnamedIdWithAFinishingEntryShouldBeAQuest`.
  - `namedIdShouldNeverBeANote`.
  - `questsShouldComeNewestActivityFirst`.
  - `giverShouldComeFromTheEntrysActor`.
  - `giverShouldFallBackToTheConversationWithTheEarliestStage`: a topic whose info has a result script with `Journal q 10` and an actor.
  - `factionShouldComeFromTheQuestsName`: `Fighters Guild: Exterminator`, with a faction named `Fighters Guild`. `House Hlaalu: ...` matches `Great House Hlaalu`.
  - `pinningShouldUnarchiveEveryId`.
  - `archivingShouldUnpinEveryId`.
  - `unpinningShouldNotArchive`.

#### C19. Move the journal list's order out of the screen

- **Changes** `apps/openmw/mwgui/questlog.hpp`, `questlog.cpp`, `journalscreen.hpp` and `journalscreen.cpp`.
- **Moves** into `namespace MWGui::QuestLog`:
  - `JournalScreen::Tab` becomes `QuestLog::Tab`, and `JournalScreen` uses it.
  - `JournalScreen::Row` becomes `QuestLog::Row`.
  - `JournalScreen::order()` becomes `std::vector<int> order(const std::vector<Quest>& quests, Tab tab)`.
  - `JournalScreen::groupOf()` becomes `std::string groupOf(const Quest& quest, Tab tab)`. It keeps returning the `#{Interface:...}` tags and escaped names, so move or share the `escaped` helper it uses.
  - `JournalScreen::buildRows()` and `groupStarts()` become free functions that take the quests and tab.
- `JournalScreen` keeps thin members that call these, or calls them directly. No behaviour change.

#### C20. Test the journal list's order and groups

- **Changes** `apps/openmw_tests/mwgui/testquestlog.cpp`. The quests are built by hand as `QuestLog::Quest` values. The Factions cases need the fixture's store for faction names.
- **Test cases** (suite `MWGuiQuestLogOrderTest`):
  - `pinnedShouldComeFirst`.
  - `finishedShouldComeAfterQuestsInProgress`.
  - `archivedShouldComeAfterFinished`.
  - `archivedAndFinishedShouldCountAsArchived`.
  - `orderWithinAGroupShouldStayNewestFirst`: the sort is stable.
  - `placesTabShouldGroupQuestsInProgressByTown`: in the order of the towns' names, without case.
  - `placesTabShouldPutQuestsWithoutATownLast`: before Finished.
  - `factionsTabShouldGroupByFactionName`, and `factionsTabShouldPutQuestsWithoutAFactionLast`.
  - `questTabsShouldLeaveOutNotes`.
  - `questTabsShouldKeepPinnedNotes`.
  - `notesTabShouldListOnlyNotes`.
  - `archivedNoteShouldStayOnTheNotesTab`: in its Archived group.
  - `rowsShouldStartEachGroupWithAHeader`.
  - `groupStartsShouldBeTheFirstEntryUnderEachHeader`.
  - `emptyJournalShouldGiveNoRows`.

#### C21. Move the journal's topic linking out of the screen

- **Adds** `apps/openmw/mwgui/journallinks.hpp` and `journallinks.cpp`. Adds `journallinks` to `add_openmw_dir (mwgui ...)`.
- **Moves** `JournalScreen::LinkedText` and the body of `JournalScreen::linkTopics` into `MWGui::linkTopics(text, keywordSearch, translations, topics, page)`. The screen's member calls it with its own `mKeywordSearch` and `mTopics`, and with the window manager's translation storage.

#### C22. Test the journal's topic linking

- **Adds** `apps/openmw_tests/mwgui/testjournallinks.cpp`, listed in CMake. `apps/openmw_tests/mwdialogue/testkeywordsearch.cpp` shows how to seed a `KeywordSearch` and use a default `Translation::Storage`.
- **Test cases** (suite `MWGuiJournalLinksTest`):
  - `knownTopicShouldBecomeASpan`: its start and end cover the topic's name in the text.
  - `textWithoutTopicsShouldBeUnchanged`.
  - `topicThePlayerHasNotAskedAboutShouldNotLink`: it is in the keyword search but not in `topics`.
  - `topicShouldNotLinkToItsOwnPage`.
  - `twoTopicsShouldGiveTwoSpansInOrder`.
  - `longerTopicShouldWinOverAShorterOneInsideIt`: as the journal book does, such as "Mages Guild" over "guild".

**Stopping point.** The journal's rules are covered in CI.

---

### Phase 3: touch controls

#### C23. Move the touch gestures' arithmetic out of TouchManager

- **Adds** `apps/openmw/mwinput/touchgestures.hpp` and `touchgestures.cpp`. Adds `touchgestures` to `add_openmw_dir (mwinput ...)`.
- **Moves** into `namespace MWInput::TouchGestures` the constants these functions use (`sTapTime`, `sSlopMm`, `sMoveDeadZoneMm`, `sMoveFullMm`, `sLookRadiansPerMm`, `sLookCircleMm`, `sLookRampMm`, `sLookFullSpeed`, `sEdgeMm`, `sEdgeSwipeMm`, `sCornerMm`, `sScrollStepMm`, `sTabSwipeMm`, `sPickerAcrossMm`, `sPickerUpDownMm`, `sPickerOffsets`) and the arithmetic, as functions of plain values. Each `TouchManager` member keeps its name and calls the new function with `screenSize()` and `pixelsPerMm()`.
  - `float pixelsPerMm(float dpi, int drawableWidth, int windowWidth)`, from `TouchManager::pixelsPerMm`, with the 160 DPI fallback for a DPI of 0 or less.
  - `Zone zoneAt(point, screenSize, pixelsPerMm)`, from `roleForGame`. `Zone` is `Move`, `Look`, `QuickSave`, `QuickLoad` or `Sneak`, and `roleForGame` maps it to a `Role`.
  - `int edgeAt(point, screenSize, pixelsPerMm)`, from `edgeAt`.
  - `EdgeSwipe edgeSwipe(start, position, edge, farthest, pixelsPerMm)`, from the first half of `updateEdge`. It returns `Undecided`, `NotASwipe` or `SwipedIn`.
  - `bool isTap(downTime, upTime, farthest, pixelsPerMm)`.
  - `Movement movement(start, position, pixelsPerMm)`, from `setMovement`. It returns the two action values, each from 0 to 1.
  - `float lookYaw(startX, previousX, currentX, pixelsPerMm)` and `float lookPitch(previousY, currentY, pixelsPerMm)`, from `look`.
  - `float turnSpeed(startX, currentX, pixelsPerMm)`, from `keepTurning`. Radians a second, signed.
  - `int tabsSwiped(startX, currentX, pixelsPerMm)`, from `menuSwipeTabs`.
  - `int scrollSteps(...)`, from `menuScroll`. Read `menuScroll` for what it needs.
  - `int pickerItem(origin, position, pixelsPerMm)`, from `updatePicker`.
- No behaviour change, and no tests.

#### C24. Test the touch gestures' arithmetic

- **Adds** `apps/openmw_tests/mwinput/testtouchgestures.cpp`, listed in CMake. The tests use a 2448x1848 screen at 16 pixels a millimetre, close to the Fold 8's inner screen, and a 1280x800 screen at 6.3.
- **Test cases** (suite `MWInputTouchGesturesTest`):
  - `pixelsPerMmShouldComeFromTheDpi`: 254 DPI gives 10.
  - `pixelsPerMmShouldScaleWithTheDrawable`: a drawable twice the window's width doubles it.
  - `unknownDpiShouldFallBackTo160`.
  - `topLeftCornerShouldQuickSave`, `topRightCornerShouldQuickLoad`, `bottomRightCornerShouldSneak`. Each is 12 mm, with a point just inside and one just outside.
  - `bottomLeftCornerShouldMove`: it has no corner of its own.
  - `leftHalfShouldMove` and `rightHalfShouldLook`, with the middle column going to Look.
  - `edgeBandShouldBeTheMiddleHalfOfEachSide`: 5 mm wide, and nothing in the top or bottom quarter.
  - `edgeSwipeShouldWaitUntilTheFingerMoves`: under 3 mm is `Undecided`.
  - `edgeSwipeShouldWaitFor8mmInwards`.
  - `edgeSwipeInwardsShouldSwipeIn`, for both edges.
  - `edgeFingerMovingAlongTheEdgeShouldNotBeASwipe`: more movement up or down than inwards.
  - `edgeFingerMovingOutwardsShouldNotBeASwipe`.
  - `quickStillTouchShouldBeATap`.
  - `slowTouchShouldNotBeATap`: over 250 ms.
  - `touchThatMoved3mmShouldNotBeATap`: even when it came back to where it started.
  - `movementInsideTheDeadZoneShouldBeNone`: both values 0.5.
  - `movementAt15mmShouldBeFull`: up the screen gives 0 for forward and back.
  - `movementPast15mmShouldStayFull`.
  - `movementHalfwayShouldBeProportional`.
  - `diagonalMovementShouldKeepItsDirection`.
  - `lookInsideTheCircleShouldTurnWithTheFinger`: 1 mm is 0.026 radians.
  - `lookPastTheCircleShouldNotTurnDirectly`.
  - `lookCrossingTheCirclesEdgeShouldTurnOnlyForThePartInside`.
  - `pitchShouldNotBeLimitedByTheCircle`.
  - `turnSpeedShouldBeZeroInsideTheCircle`.
  - `turnSpeedShouldGrowOver20mm`: half speed 10 mm past the edge.
  - `turnSpeedShouldStopGrowingAtFullSpeed`: 130 degrees a second.
  - `turnSpeedShouldTakeTheFingersSide`.
  - `tabsShouldSwitchEvery12mm`: 11 mm is 0, 12 mm is 1, 25 mm is 2.
  - `tabsShouldSwitchBackWhenTheFingerReturns`: -12 mm is -1, and -11 mm is 0.
  - `pickerShouldStartOnUse`.
  - `pickerShouldChooseTheNearestItem`: attack one step to the left, the quick keys two steps, magic above, jump below.

#### C25. Move the touch tutorial's record out of TouchManager

- **Adds** `apps/openmw/mwinput/touchtutorial.hpp` and `touchtutorial.cpp`. Adds `touchtutorial` to `add_openmw_dir (mwinput ...)`.
- **Moves** `TutorialRecord`, `readTutorialRecord`, `writeTutorialRecord` and `sTutorialAgain` into `namespace MWInput`. The decision in `TouchManager::checkTutorial` becomes `bool tutorialDue(const TutorialRecord& record, std::string_view character, std::int64_t now)`, which `checkTutorial` calls.

#### C26. Test the touch tutorial's record

- **Adds** `apps/openmw_tests/mwinput/testtouchtutorial.cpp`, listed in CMake.
- **Test cases** (suite `MWInputTouchTutorialTest`):
  - `missingFileShouldReadAsNeverPlayed`.
  - `recordShouldRoundTrip`: the time and several characters.
  - `characterFolderWithSpacesShouldRoundTrip`.
  - `unknownLinesShouldBeIgnored`.
  - `newGameShouldShowTheTutorial`: an empty character name.
  - `characterThatHasNotSeenItShouldShowIt`.
  - `characterThatHasSeenItShouldNotShowIt`.
  - `fourteenDaysAwayShouldShowItAgain`: one second under 14 days doesn't.
  - `firstEverPlayShouldNotCountAsAway`: a last-played time of 0.

**Stopping point.** The touch rules that have fixed numbers are covered in CI. What each gesture does in the game is left to tier G.

---

### Phase 4: other rules

#### C27. Move the Run Fatigue arithmetic into a function

- **Adds** `apps/openmw/mwmechanics/runfatigue.hpp`, a header with one inline function. Adds `runfatigue` to `add_openmw_dir (mwmechanics ...)`, whose glob picks up a lone header.
- **Moves** the last three lines of the block in `apps/openmw/mwmechanics/character.cpp` into `float MWMechanics::scaleRunFatigueLoss(float loss, float recovered, float scale)`.

#### C28. Test the Run Fatigue arithmetic

- **Adds** `apps/openmw_tests/mwmechanics/testrunfatigue.cpp`, listed in CMake.
- **Test cases** (suite `MWMechanicsRunFatigueTest`):
  - `fullScaleShouldKeepMorrowindsLoss`.
  - `zeroScaleShouldLoseOnlyWhatComesBack`: so fatigue stays level.
  - `halfScaleShouldHalveTheLossBeyondRecovery`: a loss of 6 with 4 recovered gives 5.
  - `lossBelowRecoveryShouldBeUnchanged`: whatever the scale.
  - `noRecoveryShouldScaleTheWholeLoss`: full fatigue recovers nothing.

#### C29. Move the controller list's scrolling arithmetic out of its drawing

- **Adds** `apps/openmw/mwgui/listscroll.hpp` and `listscroll.cpp`. Adds `listscroll` to `add_openmw_dir (mwgui ...)`.
- **Moves** from `ControllerList::draw` in `apps/openmw/mwgui/controllerscreen.cpp` into `namespace MWGui::ListScroll`. `ControllerList::Row` moves there too, with `ControllerList::Row` kept as an alias.
  - `int itemCount(rows)`.
  - `int rowOfItem(rows, item)`.
  - `int scrollToShow(rows, scroll, focusRow, height)`: the clamp, the step back to a group's header, and the loop that scrolls down until the focused row fits.
  - `int maxScroll(rows, height)`.
  - `int itemUnderPoint(rows, scroll, height, pointY)`: from the wheel handler, the first item that ends below the point among the rows that fit, or the last item below the last row. It returns an item index, or -1.
- `ControllerList::draw` calls these. No behaviour change.

#### C30. Test the controller list's scrolling

- **Adds** `apps/openmw_tests/mwgui/testlistscroll.cpp`, listed in CMake.
- **Test cases** (suite `MWGuiListScrollTest`):
  - `headersShouldNotCountAsItems`.
  - `rowOfItemShouldSkipHeaders`.
  - `focusInViewShouldNotScroll`.
  - `focusBelowTheViewShouldScrollJustFarEnough`.
  - `focusAboveTheViewShouldScrollToIt`.
  - `scrollingUpToAGroupsFirstItemShouldShowItsHeader`.
  - `rowsOfDifferentHeightsShouldBeMeasured`.
  - `listShouldNotScrollPastItsEnd`.
  - `listThatFitsShouldNotScroll`.
  - `scrollPastTheEndShouldComeBackWhenTheListShrinks`.
  - `wheelShouldFocusTheItemUnderThePointer`.
  - `pointerOnAHeaderShouldFocusTheItemBelowIt`.
  - `pointerBelowTheLastRowShouldFocusTheLastItem`.
  - `rowThatDoesNotFitShouldNotTakeTheFocus`.

#### C31. Move the packing of status parts and button hints into a function

- **Changes** `apps/openmw/mwgui/listscroll.hpp` and `listscroll.cpp`, or adds a new pair `textpacking` if that reads better.
- **Moves** the loop in `ControllerUi::drawTabRow` that puts the status's parts on as many lines as needed into `std::vector<std::string> packParts(parts, gap, room, measure)`, where `measure` is a `std::function<int(const std::string&)>` that gives a text's width. `drawTabRow` passes a lambda that measures with its hidden text box. Do the same for the row breaking in `ControllerUi::drawHints` and `hintsHeight`, if their loop can take the hints' widths as numbers.

#### C32. Test the packing of status parts and hints

- **Changes** `apps/openmw_tests/mwgui/testlistscroll.cpp`, or adds `testtextpacking.cpp`. The tests measure text as 10 units a character.
- **Test cases** (suite `MWGuiTextPackingTest`):
  - `partsThatFitShouldStayOnOneLine`.
  - `partsShouldMoveToANewLineWhenTheyRunOut`.
  - `partWiderThanTheRoomShouldGetItsOwnLine`.
  - `threePartsShouldFillLinesGreedily`.
  - `emptyStatusShouldGiveOneEmptyLine`: as the code does today.
  - `hintsShouldStartANewRowWhenARowIsFull`, if `drawHints` was moved.

#### C33. Move the d-pad quick keys' press and hold rule into a function

- **Changes** `apps/openmw/mwgui/quickkeyslots.hpp`, `apps/openmw/mwinput/actionmanager.hpp` and `actionmanager.cpp`.
- **Moves** the `DpadPress` struct and the body of the loop in `ActionManager::updateDpadQuickKeys` into `quickkeyslots.hpp` as `int MWGui::QuickKeySlots::updatePress(DpadPress& press, int direction, bool down, bool playing, bool holdSlotAssigned, float dt)`. It returns the slot to use now, or 0. `updateDpadQuickKeys` gathers `down`, `playing` and `holdSlotAssigned` as it does today, and calls `quickKey` when the result isn't 0.

#### C34. Test the d-pad quick keys' press and hold rule

- **Adds** `apps/openmw_tests/mwgui/testquickkeyslots.cpp`, listed in CMake.
- **Test cases** (suite `MWGuiQuickKeySlotsTest`):
  - `pressesShouldUseSlots1To4AndHoldsSlots5To8`: `slotFor`.
  - `directionOfShouldFindOnlyTheDpad`.
  - `pressWithNothingOnTheHoldShouldActAtOnce`.
  - `pressWithAHoldAssignedShouldWaitForTheRelease`.
  - `shortPressShouldUseThePressSlotOnRelease`.
  - `holdShouldUseTheHoldSlotAfter350ms`, and only once.
  - `releaseAfterAHoldShouldDoNothing`.
  - `pressThatStartsInAMenuShouldDoNothing`: even if the menu closes while the direction is down.
  - `menuOpeningDuringAPressShouldCancelIt`.

#### C35. Move the scaling layer's arithmetic into functions

- **Changes** `components/myguiplatform/scalinglayer.hpp` and `scalinglayer.cpp`.
- **Moves** the arithmetic of `ScalingLayer::updateFixedHeightViewSize` and `layerToScreenCoords` into two free functions in the header, which take the sizes rather than reading `MyGUI::RenderManager`: `MyGUI::IntSize fixedHeightViewSize(screenSize, height)` and `MyGUI::IntCoord layerToScreen(coord, viewSize, screenSize, scale)`. The members call them.

#### C36. Test the scaling layer's arithmetic

- **Adds** `apps/components_tests/myguiplatform/testscalinglayer.cpp`, listed in `apps/components_tests/CMakeLists.txt`.
- **Test cases** (suite `MyGuiPlatformScalingLayerTest`):
  - `fixedHeightShouldKeepTheScreensShape`: 720 high on 1920x1080 is 1280 wide, and on 2448x1848 it is 954 wide.
  - `layerCornersShouldMapToScreenCorners`.
  - `layerCentreShouldMapToTheScreensCentre`.
  - `sizeShouldScale`: a 100-unit box is 300 pixels wide at a scale of 3.

#### C37. Build the Android shader merge on every platform

- **Adds** `components/shader/mergeshaders.hpp` and `mergeshaders.cpp`. Adds `mergeshaders` to `add_component_dir (shader ...)`.
- **Moves** `declareUniformOnce` and `foldForMerge` out of the `#ifdef ANDROID` block in `components/shader/shadermanager.cpp` into `namespace Shader`, with their comments and the note of where they were adapted from. The Android-only code that calls them stays inside its `#ifdef`. The functions are plain text handling and now compile everywhere.

#### C38. Test the Android shader merge

- **Adds** `apps/components_tests/shader/mergeshaders.cpp`, listed in `apps/components_tests/CMakeLists.txt`.
- **Test cases** (suite `ShaderMergeShadersTest`):
  - `plainUniformShouldBeGuardedByItsName`: `uniform sampler2D diffuseMap;` is wrapped in `#ifndef OMW_UNIFORM_diffuseMap`.
  - `indentedUniformShouldBeGuarded`.
  - `uniformArrayShouldBeGuardedByItsNameWithoutTheSize`: `uniform vec4 lights[8];`.
  - `uniformWithAnInitialValueShouldBeLeftAlone`.
  - `uniformListShouldBeLeftAlone`: `uniform float a, b;`.
  - `uniformBlockShouldBeLeftAlone`: a line with no `;`.
  - `uniformWithALayoutQualifierShouldBeLeftAlone`: it has a `(`.
  - `otherLinesShouldBeUnchanged`.
  - `sameUniformTwiceShouldBeGuardedBothTimes`: so the preprocessor keeps the first.
  - `mainShaderShouldKeepItsVersionLine`.
  - `linkedShaderShouldLoseItsVersionLine`.
  - `lineCountShouldStayTheSameApartFromGuards`: so error messages still point near the right line.
  - `sourceWithoutAFinalNewlineShouldNotGainOne`.
  - `windowsLineEndingsShouldStillBeGuarded`: the code trims `\r` before it looks for the `;`.

#### C39. Test the automation server's JSON reader

- **Adds** `apps/openmw_tests/mwautomation/testjson.cpp`, listed in CMake. No code moves. `MWAutomation::Json::parse` and `dump` are public.
- **Test cases** (suite `MWAutomationJsonTest`):
  - `shouldParseAnObjectWithEachKindOfValue`: a string, a number, true, false, null, an array and an object inside it.
  - `shouldParseEscapes`: `\"`, `\\`, `\n` and `é`.
  - `shouldParseNegativeAndFractionalNumbers`.
  - `dumpShouldRoundTrip`.
  - `dumpShouldEscapeQuotesAndNewlines`.
  - `shouldRejectTrailingText`, `shouldRejectAnUnclosedString`, `shouldRejectAnUnclosedObject`. Read `json.cpp` for how it reports an error, and expect that.
  - `deeplyNestedArraysShouldNotCrash`: 10,000 `[` characters either parse or fail cleanly. If this crashes, that is a finding, though only for the harness.

#### C40. Test the dialogue camera's framing script

- **Adds** `apps/components_tests/lua/testdialogueframing.cpp`, listed in `apps/components_tests/CMakeLists.txt`.
- **How:** the test reads `OPENMW_PROJECT_SOURCE_DIR/files/data/scripts/omw/camera/dialogue_framing.lua` from disk, puts its text in a `TestingOpenMW::VFSTestFile`, and runs it in a `LuaUtil::LuaState` sandbox. `apps/components_tests/lua/testlua.cpp` shows the pattern. It gives the script stand-in packages with `addCommonPackage`: `openmw.util` is the real one from `LuaUtil::initUtilPackage`, and `openmw.camera`, `openmw.core`, `openmw.input` and `openmw.ui` are Lua tables written in the test, whose functions return values the test sets and record the last `setExtraYaw` and `setExtraPitch`. The speaker is a Lua table with `isValid`, `enabled` and `getBoundingBox`.
- **Test cases** (suite `LuaDialogueFramingTest`):
  - `shouldDoNothingBeforeAConversation`: `active()` is false and the extra yaw and pitch are 0.
  - `shouldTurnTowardsTheSpeakerInFirstPerson`: after `onUiModeChanged` with `Dialogue` and many `update()` calls, the extra yaw is near the angle to the speaker's face.
  - `shouldPutTheFaceAboveTheMiddleOfTheScreen`: with the face level with the camera, the extra pitch looks down by `atan((1 - 2 * 0.28) * tan(fov / 2))`.
  - `shouldNotFrameInThirdPerson`.
  - `shouldNotFrameWhenControllerMenusAreOff`.
  - `shouldKeepFramingDuringBarter`: and the yaw moves the face to the left of the panel.
  - `shouldTurnBackWhenTheConversationEnds`: the values ease to exactly 0 and `active()` becomes false.
  - `shouldEaseRatherThanJump`: one `update()` with a small frame time moves part of the way.
  - `speakerThatBecomesInvalidShouldTurnBack`.

**Stopping point.** Every rule that could be made a plain function is covered in CI.

---

### Phase 5: the ini importer

#### C41. Build the ini importer's code as a library

- **Changes** `apps/mwiniimporter/CMakeLists.txt`: `importer.cpp` goes into a new static library `openmw-iniimporter-lib`, which links `components`. The `openmw-iniimporter` program is built from `main.cpp` and links the library. The precompiled header and coverage settings apply to both, as they do to the program today.
- No source changes. The program behaves as before.
- **Run:** `cmake --build MSVC2022_64 --config RelWithDebInfo --target openmw-iniimporter`, then run it once on a test ini as a check.

#### C42. Test which archives the ini importer keeps

- **Adds** `apps/components_tests/mwiniimporter/testimporter.cpp`.
- **Changes** `apps/components_tests/CMakeLists.txt`: inside `if (BUILD_MWINIIMPORTER)`, add the test file with `target_sources(components-tests PRIVATE ...)` and link `openmw-iniimporter-lib`. The top-level `CMakeLists.txt` adds `apps/mwiniimporter` before `apps/components_tests`, so the target exists. Both CI jobs build the importer, so the tests run there. See the open questions for the choice of test program.
- **How:** each test makes a temporary folder with a `Morrowind.ini` whose `[Archives]` section lists some archives, and data folders holding empty files with those names. It calls `MwIniImporter::loadIniFile`, fills a `cfg` multimap with `data` entries, calls `importArchives(cfg, ini, iniPath)` and reads `cfg["fallback-archive"]`. Read `importArchives` to see which archives it always adds, such as `Morrowind.bsa`, and write the expected lists to match.
- **Test cases** (suite `MwIniImporterArchivesTest`):
  - `archiveInTheInisDataFilesFolderShouldBeKept`.
  - `archiveInADataFolderFromTheCfgShouldBeKept`: under `data` and under `data-local`.
  - `missingArchiveShouldBeLeftOut`: such as `PC_Data.bsa`.
  - `archiveWithDifferentCapitalisationShouldBeKept`: the ini says `tribunal.bsa` and the file is `Tribunal.bsa`. The name is written as the ini has it.
  - `dataFolderThatDoesNotExistShouldBeSkipped`: without an error.
  - `orderShouldFollowTheIni`.
  - `iniWithoutAnArchivesSectionShouldNotFail`.
  - `MwIniImporterGameFilesTest.missingPluginShouldBeLeftOut`: one case for `importGameFiles`, so the plugin rule that the archive rule copies is pinned down too.

**Stopping point.** The setup bug that stopped a player's game from starting can't come back unnoticed.

---

### Phase 6: Android JVM tests

None of this phase can run on this Windows machine, which has no Android SDK or JDK set up for Gradle. It runs in CI and on the Mac. Push each commit and read the CI result before the next.

#### C43. Run JVM unit tests for the Android app in CI

- **Changes** `android/app/build.gradle.kts`: a `dependencies { testImplementation("junit:junit:4.13.2") }` block. This is a new dependency, which Gradle downloads from Maven Central. See the open questions.
- **Changes** `android/build.sh`: a `test` mode that runs `./gradlew testDebugUnitTest` without the check for the native libraries and assets, which unit tests don't need.
- **Changes** `.github/workflows/android.yml`: a second job, `Android unit tests`, on `ubuntu-latest`, with the checkout, `gradle/actions/setup-gradle@v4` and `android/build.sh test`. It runs beside the APK job and doesn't wait for the Docker build. **CI change.**
- **Adds** `android/app/src/test/java/io/github/durhamg/modernmorrow/DataImportTest.java` with one case, so the job has something to run:
  - `sizeShouldUseGigabytesFromOneGigabyte`: `DataImport.size(1L << 30)` is `"1.0 GB"`, and `DataImport.size(512L << 20)` is `"512 MB"`. `size` is already static and package-private.
- **Run:** `android/build.sh test` on a machine with the Android SDK. In CI, the new job.

#### C44. Split the Android app's file handling from its Android calls

- **Adds** `android/app/src/main/java/io/github/durhamg/modernmorrow/DataFiles.java`, a class of static functions that imports nothing from `android.*`.
- **Moves** from `DataImport.java`, making each package-private: `zipTarget`, `findDataFiles(File, int)`, `gameFileName` with `sGameFiles`, `moveTree`, `moveFile`, `checkSpace`, `makeDir`, `deleteRecursively` and `size`. `DataImportTest` from C43 becomes `DataFilesTest`.
- **Moves** the loop of `DataImport.unpackZip` that reads the zip's entries into `static boolean DataFiles.unpack(InputStream in, File dir, BooleanSupplier cancelled)`, which returns whether the zip had any entry. `unpackZip` keeps the `ContentResolver` calls, the size query and the counting stream, and passes `() -> mCancelled`.
- **Moves** the last five lines of `unpackZip`, which find `Data Files` in the unpacked folder and move it into place, into `static void DataFiles.install(File unpacked, File target)`.
- No behaviour change. `DataImport.java` keeps everything that touches `ContentResolver`, `Uri` and `DocumentsContract`.

#### C45. Test the Android app's zip unpacking and file moves

- **Changes** the test file from C43, now `DataFilesTest.java`. The tests build zips in memory with `java.util.zip.ZipOutputStream`, and use JUnit's `TemporaryFolder` rule.
- **Test cases:**
  - `zipEntryShouldLandInsideTheFolder`.
  - `zipEntryWithDotDotShouldBeRefused`: `../evil.txt` throws an `IOException`, and nothing is written outside the folder.
  - `zipEntryWithDotDotInTheMiddleShouldBeRefused`: `Data Files/../../evil.txt`.
  - `zipEntryWithBackslashesShouldBeSplitIntoFolders`: `Data Files\Morrowind.esm`.
  - `zipEntryWithBackslashDotDotShouldBeRefused`: `..\evil.txt`.
  - `absoluteZipEntryShouldLandInsideTheFolder`: `/etc/passwd` becomes `<dir>/etc/passwd`.
  - `unpackShouldWriteEveryFileAndFolder`.
  - `unpackOfAnEmptyZipShouldSayThereWasNothing`: it returns false.
  - `unpackOfSomethingThatIsNotAZipShouldSayThereWasNothing`.
  - `unpackShouldStopWhenCancelled`.
  - `dataFilesShouldBeFoundAtTheTop`, `dataFilesShouldBeFoundThreeFoldersDown`, `dataFilesShouldNotBeFoundFourFoldersDown`.
  - `dataFilesShouldBeFoundWhateverTheEsmsCase`: `morrowind.ESM`.
  - `folderWithoutMorrowindEsmShouldNotBeFound`.
  - `gameFileNamesShouldGetTheirUsualCase`: `MORROWIND.BSA` becomes `Morrowind.bsa`, and `readme.txt` is unchanged.
  - `installShouldFixTheCaseOfTopLevelGameFilesOnly`: a `tribunal.esm` inside `Data Files/Meshes` keeps its name.
  - `installShouldMoveFoldersIntoExistingFolders`: files already in the target stay, and files with the same name are replaced.
  - `installShouldMoveMorrowindEsmLast`: make a file earlier in the move fail, by putting a non-empty folder where a file must go, and check that the target has no `Morrowind.esm`. The app finds the data by that file, so a half-finished move isn't taken for an install.
  - `installWithoutMorrowindEsmShouldFail`.
  - `spaceCheckShouldNameBothSizes`: asking for more than the folder's free space throws with both sizes in the message.

#### C46. Split the Android app's config writing from its Android calls

- **Adds** `android/app/src/main/java/io/github/durhamg/modernmorrow/GameConfig.java`, with no `android.*` imports.
- **Moves** from `Files.java`: the body of `writeUserConfig` into `static void GameConfig.writeUserConfig(File configDir, File dataFiles)`, `fixGlobalConfig` as it is, and `readFile` and `readStream` if `fixGlobalConfig` needs them. `Files.java` calls them with the folders it gets from the `Context`.

#### C47. Test the config files the Android app writes

- **Adds** `android/app/src/test/java/io/github/durhamg/modernmorrow/GameConfigTest.java`.
- **Test cases:**
  - `userConfigShouldNameTheDataFolderInQuotes`: with a path that has a space.
  - `userConfigShouldListOnlyTheGameFilesThatExist`: with `Morrowind.esm`, `Morrowind.bsa` and `Tribunal.esm` but no `Tribunal.bsa`, it has two `content=` lines and one `fallback-archive=` line.
  - `userConfigShouldListGameFilesInMorrowindTribunalBloodmoonOrder`.
  - `userConfigShouldBeWrittenAgainOverAnOldOne`.
  - `userConfigShouldCreateItsFolder`.
  - `globalConfigShouldPointResourcesAtTheCopiedFolder`.
  - `globalConfigShouldPointDataFoldersUnderResourcesAtTheCopiedFolder`: `data=./resources/vfs` becomes `data="<resources>/vfs"`.
  - `globalConfigShouldKeepOtherLines`: including the `fallback=` lines from `android/morrowind-ini.cfg`.

**Stopping point.** The Android app's file handling is covered in CI.

---

### Phase 7: checks in the harness, with Morrowind's data

These run only on a machine with Morrowind's data, before a release. The commits that change `omw.py` also add Python tests that run in CI, since they need no game.

#### C48. Test the harness client's own helpers in CI

- **Adds** `ui-rework/tools/test_omw.py`, using Python's `unittest`.
- **Changes** `.github/workflows/push.yml`: the `Check UI data files` step from C01 also runs `python3 -m unittest discover -s ui-rework/tools -p 'test_*.py'`. **CI change.**
- **Test cases**, for functions `omw.py` already has:
  - `test_parse_value_reads_json_and_falls_back_to_text`.
  - `test_build_request_splits_key_value_words`.
  - `test_parse_resolution_knows_the_named_screens`.
  - `test_read_settings_cfg_keeps_sections_and_order`.
  - `test_find_widget_by_text_skips_hidden_widgets`.
  - `test_find_widget_by_name`.
  - `test_read_binary_vdf_reads_a_shortcut`.
- **Run locally:** `python -m unittest discover -s ui-rework/tools -p "test_*.py"`.

#### C49. Let scenarios check the UI and wait for it

- **Changes** `ui-rework/tools/omw.py`, in `run_lines`, and `ui-rework/harness.md`. Each new line fails the scenario with the file, line number and what was found.
  - `expect-text TEXT` and `expect-no-text TEXT`: a visible widget shows TEXT, or none does.
  - `expect-name NAME` and `expect-no-name NAME`: a visible widget has the name.
  - `expect-order TEXT TEXT ...`: visible widgets show the texts, each lower on the screen than the one before, or on the same row and further right.
  - `expect-mode MODE` and `expect-no-mode`: the topmost UI mode from `state`, or no menu open.
  - `expect-state STATE`: `menu`, `running` or `ended`.
  - `expect-lua CODE VALUE`: the `lua` command's printed output equals VALUE. It takes `context=`.
  - `wait-text TEXT [timeout=SECONDS]` and `wait-mode MODE`: ask every few frames until it holds, then go on. They replace fixed waits such as `wait frames=900`, which are slow here and too short under `xvfb`.
- The matching is done by pure functions that take a UI dump, `texts_in_order(dump, texts)` and so on.
- **Python tests** added to `test_omw.py`, on small hand-written dumps: `test_expect_text_ignores_hidden_widgets`, `test_expect_order_passes_top_to_bottom`, `test_expect_order_fails_when_swapped`, `test_expect_order_fails_when_a_text_is_missing`, `test_expect_order_allows_the_same_row_left_to_right`.

#### C50. Fail a scenario when the game logs an error

- **Changes** `ui-rework/tools/omw.py` and `ui-rework/harness.md`. Adds `ui-rework/scenarios/log-allowlist.txt`.
- At the end of every scenario, and at an `expect-log-clean` line, the client reads the profile's `openmw.log` from where the scenario started. It fails on any line with `[E]`, `Error in frame`, `Bad LiveCellRef` or `Fatal error`, unless a pattern in the allow list matches. The allow list starts with the one known message from `progress.md`, "Error in frame: class does not have services" after a failed load at the main menu, and whatever else the first full run shows to be noise. Each entry has a comment saying why it is allowed.
- A scenario can expect an error with `expect-log TEXT`, which also marks matching lines as seen.
- **Python tests:** `test_log_errors_are_found`, `test_allowlisted_lines_are_skipped`, `test_lines_before_the_scenario_started_are_skipped`.

#### C51. Add a tool that reads and edits save files

- **Adds** `ui-rework/tools/savefile.py`, standard library only, and its tests in `ui-rework/tools/test_savefile.py`.
- **What it does**, on the ESM3 layout of a save: a list of records, each a 4-character name, a size and flags, with subrecords inside, and the format version in the `TES3` record's `FORM` subrecord. Read `components/esm3/esmwriter.cpp` and `esmreader.cpp` for the exact layout.
  - `savefile.py records FILE` prints each record's name and size.
  - `savefile.py format FILE` prints the format version, and `savefile.py format FILE 42` sets it.
  - `savefile.py strip FILE UIST -o OUT` writes a copy without the named records, and lowers the header's record count, which is what a save that OpenMW wrote looks like.
- Scenario lines in `omw.py`: `save-format PATH EXPECTED`, `save-has-record PATH NAME`, `save-strip PATH NAME OUT` and `save-set-format PATH N`, with paths relative to the profile's saves folder. A `*` in a path matches one name, since save file names come from the description.
- **Python tests**, on a tiny save built in the test from bytes: `test_lists_records_in_order`, `test_reads_the_format_version`, `test_sets_the_format_version`, `test_strip_removes_the_record_and_fixes_the_count`, `test_strip_leaves_other_records_byte_for_byte`, `test_truncated_file_is_an_error`.

#### C52. Add checks to the scenarios that exist

- **Changes** files in `ui-rework/scenarios/`. Do it in a few commits of about ten scenarios each, so each commit is one full run to check. In every scenario, each `shot` gets an `expect-mode` and one or two `expect-text` lines for something that must be on that screen, and fixed waits become `wait-text` or `wait-mode` where there is something to wait for. Take the texts from the current UI dumps in `.automation/out/`.
- Particular checks to add:
  - `controller-journal.txt`: `expect-order` for the group headers on each tab, such as `"In Progress" Finished` on the first, and the town names in order on Places.
  - `controller-saves.txt`: after the save, `save-format "*/*.omwsave" 41` and `save-has-record "*/*.omwsave" UIST`.
  - `controller-quick-keys.txt`: `expect-text` for the used item's name on the HUD after a press and after a hold.
  - `controller-pause-menu.txt`: `expect-order Save Load "New Game" Options Exit`.
- **Run:** `python ui-rework/tools/omw.py scenario all`. It exits with 1 if any scenario fails.

#### C53. Add scenarios for OpenMW's saves folder

- **Adds** `ui-rework/scenarios/saves-openmw.txt`, and a launch option `--openmw-saves` in `omw.py` that makes an empty scratch folder in the profile and sets `[Saves] openmw saves folder` to it.
- **The scenario**, each step with its checks:
  1. Start a game, save it, and copy the save into the scratch folder under a character folder, without its UIST record (`save-strip`). The OpenMW tab shows on the load screen (`expect-text OpenMW`).
  2. Load that save. `expect-state running`, and `expect-log-clean`.
  3. Save. The save goes to our own folder, in a character folder of the same name (`save-has-record`).
  4. Save over the save in OpenMW's folder. A copy is in `backups/OpenMW/<character>/` (a new `expect-file GLOB` line), and the saved file has UIST.
  5. Put a copy at format 42 in OpenMW's folder (`save-set-format`) and load it. The load is refused (`expect-log` for the version message, and `expect-state` shows the game didn't change).
- **Adds** `ui-rework/scenarios/saves-legacy.txt`: a save set to format 43 in our own folder loads. This only proves the version check, since the records inside are still format 41's. The record tests in `testsaveload.cpp` cover the contents.

#### C54. Add scenarios for archiving, the Weight tab, Run Fatigue and the newest save

- **Adds:**
  - `ui-rework/scenarios/journal-archive.txt`: archive a quest (`expect-order Finished Archived`), quicksave, quickload, and it is still archived. Add a journal entry from the console, and it is back under In Progress. Pin an archived quest, and it is under Pinned and not Archived. After the quicksave, `expect-file "*.archived-quests.txt"`, and after unarchiving and saving, `expect-no-file`. Use the arrow keys, since the virtual d-pad doesn't work while a real controller is connected.
  - `ui-rework/scenarios/inventory-weight.txt`: add 50 of a light item and one heavy item from the console, open the Weight tab, and `expect-order` shows the heavier stack first.
  - `ui-rework/scenarios/run-fatigue.txt`: with `--set "Game.run fatigue=0"`, run for three seconds with the left stick, and `expect-lua` shows fatigue didn't drop. This needs no real controller connected.
  - `ui-rework/scenarios/load-newest-save.txt`: save, quit, and launch again with no start option. `expect-state running`. This needs `omw.py` to launch twice in one scenario, as a `relaunch` line.
  - `ui-rework/scenarios/touch-play.txt`: with `--set "Input.touch controls=true"`, a left-edge swipe opens the inventory (`expect-mode Inventory`), a swipe in from the edge closes it, and a tap in the top left corner quicksaves (`expect-file`).

#### C55. Compare UI dumps with stored copies

- **Changes** `ui-rework/tools/omw.py` and `ui-rework/harness.md`.
- `golden NAME` in a scenario takes a UI dump and reduces it to what shouldn't change by accident: each visible widget's depth, type, name and text. It drops positions, sizes, font heights and focus flags. It compares the result with a stored file and fails with a line diff. `omw.py scenario NAME --update-golden` writes the stored files instead.
- Where the stored files live depends on the open question about Morrowind's text. Until it is answered, they go in `.automation/golden/`, which git ignores, so they are a check between two runs on one machine: before a change and after it.
- **Python tests:** `test_golden_ignores_positions_and_sizes`, `test_golden_notices_changed_text`, `test_golden_notices_a_missing_widget`, `test_golden_ignores_hidden_widgets`.

#### C56. Add one command for the checks before a release

- **Adds** `ui-rework/tools/prerelease.py`, standard library only, and a section in `ui-rework/releasing.md`.
- It runs, in order, and stops at the first failure: `CI/check_ui_data.py`, the Python tests, `components-tests` and `openmw-tests` from the build folder, upstream's Morrowind Lua tests (`python scripts/integration_tests.py --omw <build>/openmw.exe --data "<Morrowind>/Data Files" --ini "<Morrowind>/Morrowind.ini" scripts/data/morrowind_tests`, with the Morrowind folder taken from the user's `openmw.cfg`), then `omw.py scenario all`. It prints a one-line result for each.
- The Morrowind Lua tests have never run against the fork. As in C05, a failure caused by a changed default is fixed with a setting, and a failure caused by the fork is a finding.

**Stopping point.** There is one command to run before a release, and it fails when a screen is broken.

---

### Phase 8: engine smoke tests in Linux CI

This phase needs the example suite, which is a download. Do C57 first. It decides what the rest can cover.

#### C57. Run the harness on Linux against the example suite

- **Changes** `ui-rework/tools/omw.py` and `ui-rework/harness.md`:
  - `--base-settings FILE` on `launch` merges a `settings.cfg` under the harness's own. The example suite's `settings.cfg` names its player model in `[Models]`, and the game needs those.
  - `launch --example-suite DIR` writes the profile's `openmw.cfg` with the suite's data folders and content files, as `CI/run_integration_tests.sh` and `scripts/data/integration_tests/test_lua_api/openmw.cfg` list them (`template.omwgame`, `landracer.omwaddon`, `the_hub.omwaddon`), and passes the suite's `settings.cfg` as the base settings.
  - Scenarios can live in another folder: `omw.py scenario --dir ui-rework/smoke all`.
- **Adds** `CI/run_ui_smoke_tests.sh`, modelled on `CI/run_integration_tests.sh`. It reuses the `example-suite` clone, sets `OMW_BUILD_DIR` to the folder of the built `openmw`, sets `OMW_SCREEN=0`, and runs `xvfb-run --auto-servernum --server-args='-screen 0 1280x800x24' python3 ui-rework/tools/omw.py scenario --dir ui-rework/smoke all`.
- **Adds** `ui-rework/smoke/start.txt`: `launch --example-suite example-suite --skip-chargen`, then `expect-state running` and `expect-log-clean`.
- **Find out, and write down in `ui-rework/harness.md`:**
  - Whether the game starts a new game with the template and reaches `running`.
  - Which controller screens open without an error. The suite has a player, a hub cell and a creature. It may have no items, spells, quests or people to talk to.
  - Whether the fork's code reads a game setting (GMST) that the template lacks. That would show as "Error in frame" in the log. Each such read is either a fork bug to fix, where the code should use `search` and a fallback, or an entry in the smoke tier's own allow list with a reason.
  - What content can be made at run time. The `lua` command with `context=global` can create records with `world.createRecord` for potions, weapons, armour and books, and put them in the player's inventory. `scripts/data/integration_tests/test_lua_api/global.lua` has examples.
- **Not a CI change yet.** Run it locally if the user allows the example-suite download, or in a throwaway branch's CI.

#### C58. Smoke-test every controller screen that the example suite can open

- **Adds** scenarios in `ui-rework/smoke/`, one per group of screens, according to what C57 found:
  - `menus.txt`: open the menus with the keyboard, step through the Map, Inventory, Magic, Stats and Journal tabs with Tab, and each of their own tabs with Q and E. After each, `expect-mode` and one `expect-text` for a tab caption.
  - `inventory-items.txt`: make a potion, a weapon and a piece of armour from Lua, add them to the player, and `expect-text` their names on the right tabs. Open the Weight tab and `expect-order` them by weight.
  - `pause-menu.txt`: Start opens it. `expect-order Save Load "New Game" Options Exit`. The virtual d-pad's Up from Save reaches Exit, which can't be tested locally while a real controller is connected. Open Options and step through its tabs.
  - `messages.txt`: a message box from Lua (`ui.showMessage`) shows on the controller notification layer.
- Each ends with the automatic log check from C50.

#### C59. Smoke-test a save round trip, and a save from another OpenMW

- **Adds** `ui-rework/smoke/saves.txt`, with the lines from C51:
  1. Start, make and add an item from Lua so the recent items list isn't empty, and save from the pause menu.
  2. `save-format` is 41, and `save-has-record UIST`.
  3. Load it. `expect-state running`, and the item's name is on the Recent tab.
  4. `save-strip` the UIST record into a second save, and load that. `expect-state running` and `expect-log-clean`. This is a save as another OpenMW version writes it.
  5. `save-set-format` a copy to 44, and load it. The load is refused with the version message, and the game carries on.
  6. With `--openmw-saves`, repeat step 5 with format 42 in OpenMW's folder, which is refused, and save over a save there, which leaves a backup.
- If the example suite turns out to have a journal quest, also pin it, save, strip UIST, load, and check the pin came back from the quest file.

#### C60. Run the smoke tests in CI

- **Changes** `.github/workflows/push.yml`: after the integration tests in the `Ubuntu` job, a step `Run UI smoke tests` running `CI/run_ui_smoke_tests.sh`, and a step that uploads `.automation/out/` and the profiles' `openmw.log` files as an artifact when the job fails. **CI change.**
- If the run is flaky under `xvfb`, raise the `wait-text` timeouts before anything else. Software rendering is slow, and the fixed frame waits were tuned on a fast machine.

**Stopping point.** Every push starts the real game, opens its screens, and saves and loads.

---

### Phase 9: keep it going

#### C61. Say in CLAUDE.md how new fork logic is tested

- **Changes** `CLAUDE.md`, with the text in section 4, and `ui-rework/README.md`, with a link to this plan or to a short `testing.md` that replaces it once the work is done.

**Total: 10 phases, numbered 0 to 9, and 61 commits.** C52 is several commits in practice.

## 4. A rule for CLAUDE.md

Suggested text, for the "UI rework project" section:

```markdown
### Tests for the fork's own logic

- A rule goes in a plain function, outside the window or manager class that uses it, in a file of its own when it is more than a few lines. A rule is anything with a number, an order or a condition that a player would notice if it were wrong: a sort, a limit, a distance, which file is chosen. The class calls the function. The function gets a Google Test in `apps/openmw_tests/` or `apps/components_tests/`, in the same commit or the next.
- Anything written to a save or read from one gets a round-trip test in `apps/components_tests/esm3/testsaveload.cpp`, for every format that still loads.
- A new screen, or a change to what a screen shows, gets `expect-` lines in its scenario in `ui-rework/scenarios/`. If the example suite can show it, it also gets a scenario in `ui-rework/smoke/`, which CI runs.
- A bug fix starts with a test that fails because of the bug, where a test at any tier can show it.
- Shell scripts that players run, and the Android app's Java, are tested too: see `CI/test_auto_update.sh` and `android/app/src/test/`.
- Run `python CI/check_ui_data.py` after adding a localisation key, a layer or a data file. Run `python ui-rework/tools/prerelease.py` before a release.
- `ui-rework/testing-plan.md` lists the tiers, what each can and can't cover, and where each runs.
```

## 5. Decisions

The user answered the open questions on 2026-11-14:

1. **The example suite may be downloaded** to this machine, as a git-lfs clone of `https://gitlab.com/OpenMW/example-suite`, so C05 and phase 8 can be run locally.
2. **Character folders prefer an exact match**, and fall back to ignoring case. This is a change in behaviour, in a commit of its own after C11, which then tests both.
3. **The ini importer's tests live in `components-tests`**, as C42 plans.
4. **JUnit 4 is acceptable** for the Android tests.
5. **Stored UI dumps are not committed.** They stay in the git-ignored `.automation/golden/`.
6. **CI runs `actionlint` from its Docker image, but doesn't check formatting.** A format mistake must never fail CI. No commit adds `CI/check_clang_format.sh` to the workflows.
7. **Game settings that the example suite lacks are fixed with a fallback** where that is cheap, and otherwise go on the smoke tier's allow list with a reason.
8. **The Weight tab's sort is checked only through the harness.**
9. **The timed autosave gets a unit test.** Its condition moves into a function in one commit, and is tested in the next.

## 6. Moves that a player could notice if they went wrong

These commits move code that runs in the game. Each is meant to change nothing, but a slip in one would reach players. Once every phase is done, a separate review reads these commits against the code they replaced:

| Commit | What a slip would break |
| --- | --- |
| C09, saving into OpenMW's folder and its backups | Every save goes through the moved code. A slip could lose a save or skip the backup. |
| C29, the controller list's scrolling | Every list in every controller screen. |
| C35, the scaling layer's arithmetic | Where the mouse and touch point, everywhere. |
| C19, the journal list's order | Which quests show, and in what order. |
| C23, the touch gestures' arithmetic | Playing on Android. |
| C33, the d-pad quick keys | Quick keys from the d-pad. |
| C15 and C17, the journal's archive and text rules | Archived quests coming back, and quest givers and places. |
| C27, Run Fatigue | Fatigue while running. |
| C37, the Android shader merge | Every shader on Android. |
| C44 and C46, the Android app's file handling and config | Setting up the game data on Android. |
| The autosave condition move | When the game autosaves. |
| The exact-match character folders, and any game setting fallbacks | These change behaviour on purpose. |
| Remembering the character folder of a new game's first save into OpenMW's folder | A fix found while checking C09. Before it, each save there made a new character folder. |

The list grows if a move turns out larger than planned.

## 7. Status

Done on 2026-11-14 and 15: phases 0 to 5, 7 and 9 on the Windows machine, and phase 6 on the Mac, with these differences from the plan.

- C14 didn't move upstream's test helpers out of `teststore.cpp`. The journal tests have their own fixture, `apps/openmw_tests/mwdialogue/journalfixture.hpp`, so upstream's file stays as it is.
- Upstream's Lua integration tests (C05) can't run on Windows. The example suite's player model crashes OpenSceneGraph while the game starts, in upstream's own build too. They run in the Linux CI job.
- The automation server's JSON reader now refuses input nested more than 64 levels, which would otherwise overflow the stack (C39).
- Checking the moves found two bugs, both fixed in commits of their own. A new game saved straight into OpenMW's folder made a new character folder for each save there. The fix for that let such a game join another playthrough's folder of the same name, which the review of the risky moves found.
- The scenarios got more than C52 to C54 planned: `saves-openmw-new-game.txt`, a `relaunch` line, and a log check that reads the log from the start of each launch.
- Phase 6's commits weren't pushed one at a time to read CI. They were run with `android/build.sh test` on the Mac, and the moves were checked by importing from a zip and from a folder in the Fold 8 emulator, after C44 and after C46.
- `DataFiles.unpack` also returns false when it stops for a cancel, so that `DataImport` returns before anything is moved, as it did before the move.
- `installShouldMoveMorrowindEsmLast` tries 20 names for the file whose move fails. The disk lists a folder's files in an order of its own, and with one name the test passed even when `Morrowind.esm` was moved first.
- C47's global openmw.cfg tests found that the app added an empty line to the end of that file. A commit of its own, before C47, fixes it.
- A separate review read C44 and C46 against the code they replaced, as section 6 asks, and found no change in behaviour, including when a cancel arrives and which streams are closed. It also checked that the empty-line fix changes nothing but the file's last line, on empty files, files without a final newline and CRLF files.
- `prerelease.py` gives upstream's Lua tests a settings file that stops the game minimizing when its window loses focus, since a minimized game stops running them.

Not done yet:

- **Phase 8, the smoke tests with the example suite.** The example suite crashes on Windows, so the harness can only run it under Linux, which means trying it in CI one push at a time.
- **The new CI steps have never run.** The UI data check, the script tests, the harness client tests, the integration tests, the lint job and the Android unit tests only run on the next push. Expect to fix some of them then.
