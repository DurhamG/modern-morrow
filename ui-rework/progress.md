# Progress log

## 2026-11-02

- Reset the local repository to upstream `master` (commit `16b14001cf`) and created the `ui-rework` branch.
- Regenerated the Visual Studio build folder `MSVC2022_64` and built everything in RelWithDebInfo. All unit tests pass.
- Surveyed the engine for automation hooks, and researched how other projects let an agent drive a game or GUI. The findings are summarized in [harness.md](harness.md).
- Wrote the first versions of [plan.md](plan.md) and [harness.md](harness.md).

- The user approved the harness design. They also asked to run the unit tests at good stopping points rather than on every commit, and to review iteration speed every so often. [plan.md](plan.md) now says so.

- Built the automation harness described in [harness.md](harness.md):
  - `omw.py` launches the game in an isolated profile under `.automation/`. With the startup movies skipped, the game answers about a second after launch.
  - The automation server in `apps/openmw/mwautomation/` accepts commands on a local port. It can take screenshots from the renderer, inject key, text, mouse and virtual gamepad input, run console and Lua commands, and dump the widget tree as text.
  - Nine scenarios cover the main menu, HUD, inventory (default and controller menus mode), dialogue, barter, journal, the in-game menu and race selection. They all pass at all three resolutions, in about 90 seconds in total.
- Lessons learned:
  - `osgViewer::ScreenCaptureHandler::getCaptureOperation()` crashes before the handler's first capture, so the engine passes its capture operation in.
  - SDL drops joystick events while the window is in the background, unless the `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` hint is set.
  - The default start cell for a game without character creation is outdoors, and the player was killed there during a test. Scenarios start in a Balmora interior instead.
  - The first version of the client read replies through Python's socket file object. That object can't be read again after a timeout, so every command slower than half a second failed. The client now buffers reply lines itself.
- Timing for one iteration:
  - rebuilding `openmw` after a change in `mwautomation/` or a single UI file takes 10 to 30 seconds,
  - launching the game takes about a second,
  - a scenario takes 2 to 5 seconds per resolution.

- Started phase 2. Wrote [research/gamepad-ui-references.md](research/gamepad-ui-references.md), a reading list of games with well-regarded gamepad UIs, and [screens.md](screens.md), a list of every piece of Morrowind's UI.
- Reworked the HUD with the user, as the first piece:
  - The minimap and the weapon or spell name text are gone.
  - Everything sits in the bottom left corner. The effect icons are in the corner, the energy bars above them, and the weapon, spell and sneak boxes to the right.
  - The HUD is sized relative to the screen height, through a fixed-height `ScalingLayer`, and ignores the GUI scaling factor. The `[HUD] size` setting tunes it.
  - Full bars fade out. The weapon and spell boxes show for five seconds after a change, and while the weapon or a spell is ready. Everything shows during combat.
  - The `hud`, `hud-fades` and `hud-combat` scenarios cover it.
- Working style agreed with the user:
  - After each change, launch the user's test save on their right monitor for them to look at: `omw.py launch -p review --steam --user-settings --set "Terrain.distant terrain=true" --resolution 3840x2160 --fullscreen --screen 1 --save "C:/Users/Durham/Documents/My Games/OpenMW/saves/Lendias/Test.omwsave"`.
  - The commit stack should show the final design, not the iteration that led to it. Fix-ups and tweaks get folded into the commit they belong to.
- Lessons learned:
  - When the user is running another game, 32 parallel compiles run out of memory. Add `-- //p:CL_MPCount=8` to the build command.
  - Python's `Path.write_text` writes Windows line endings on Windows. Pass `newline="\n"` when editing source files from Python.
  - The user's 2026 Steam Controller is only a gamepad under Steam Input, because OpenMW's SDL2 doesn't recognize it. Steam Input only works in the program a Steam shortcut starts, so `launch --steam` uses a shortcut to `openmw.exe` itself.
  - After adding a member to a settings category in `components/settings/categories/`, the incremental Visual Studio build didn't recompile the game files that read settings, apparently because the header comes in through the precompiled header. The game then crashed at startup with "bad allocation", while reading other settings. Delete `MSVC2022_64/apps/openmw/openmw-lib.dir/RelWithDebInfo/*.obj` and its `cmake_pch.pch` after changing a settings category, which costs a 4 to 5 minute rebuild.
  - OpenMW reads stick positions only from the first controller connected. If the harness's virtual gamepad is still attached, a real controller plugged in later seems not to work. Send `pad action=disconnect` before handing the game to the user.

Next: choose the next piece of the UI to rework with the user.
