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
  - After each change, launch the user's test save on their right monitor for them to look at: `omw.py launch -p review --steam --user-settings --set "Terrain.distant terrain=true" --set "Fog.radial fog=true" --set "Fog.sky blending=true" --resolution 3840x2160 --fullscreen --screen 1 --save "C:/Users/Durham/Documents/My Games/OpenMW/saves/Lendias/Test.omwsave"`.
  - The test save stands next to Fargoth in Seyda Neen. To open a conversation with him for review, send `omw.py send -p review console "command=\"fargoth\"->forcegreeting"`. ForceGreeting works on any NPC near enough to be loaded, wherever the player stands.
  - The commit stack should show the final design, not the iteration that led to it. Fix-ups and tweaks get folded into the commit they belong to.
- Lessons learned:
  - When the user is running another game, 32 parallel compiles run out of memory. Add `-- //p:CL_MPCount=8` to the build command.
  - Python's `Path.write_text` writes Windows line endings on Windows. Pass `newline="\n"` when editing source files from Python.
  - The user's 2026 Steam Controller is only a gamepad under Steam Input, because OpenMW's SDL2 doesn't recognize it. Steam Input only works in the program a Steam shortcut starts, so `launch --steam` uses a shortcut to `openmw.exe` itself.
  - After adding a member to a settings category in `components/settings/categories/`, the incremental Visual Studio build didn't recompile the game files that read settings, apparently because the header comes in through the precompiled header. The game then crashed at startup with "bad allocation", while reading other settings. Delete `MSVC2022_64/apps/openmw/openmw-lib.dir/RelWithDebInfo/*.obj` and its `cmake_pch.pch` after changing a settings category, which costs a 4 to 5 minute rebuild.
  - OpenMW reads stick positions only from the first controller connected. If the harness's virtual gamepad is still attached, a real controller plugged in later seems not to work. Send `pad action=disconnect` before handing the game to the user.
- Reviewed OpenMW's gamepad support before reworking the menus. See [research/controller-support.md](research/controller-support.md).
- The user's 2026 Steam Controller only works under Steam Input, so `omw.py launch --steam` starts the game through a Steam shortcut.
- Reworked the map screen with the user, in controller menus mode:
  - The button hint bar along the bottom of every menu is gone. Windows will show their own hints where they need them.
  - The map fills the screen below the tab bar, with no window frame, like the inventory screen.
  - The map has Local and World tabs, switched with L1 and R1 like the other screens' tabs, with the current cell's name beside them. The World button is hidden in this mode.
  - The map shows a "Y Center" hint in its bottom left corner, the same size as the inventory screen's hints.
  - The map has an opaque black background, and the world map scales to fit the window.
  - The right stick pans the local map smoothly.
  - With distant terrain on, the local map shows 7x7 cells, set by the new `[Map] local map cell distance` setting. It no longer needs "allow zooming". Terrain loads far enough to draw the whole map, even past the viewing distance.
  - The `controller-map` scenario covers it.
- Fixed distant scenery that was cut off differently as the camera turned. The far clip plane is flat, so it reaches further at the sides of the view. The user now plays with `[Fog] radial fog` and `sky blending` on, which hide the cut. With sky blending on, the scene is now drawn past the viewing distance, so the fade to sky starts where the fog is fully opaque. Before, mountain tops faded into the sky while their detail still showed.
- Ideas for later: a tilted, Skyrim-style 3D map. The local map is drawn once per cell from straight above, so this needs a live camera instead.

- Designed the inventory screen with the user, recorded in [design/inventory.md](design/inventory.md). Static mock-ups came first, built by `ui-rework/mockups/inventory.py` and shown with the harness's new `layout` command.
- Built the inventory screen for real, working alone:
  - The player's save now keeps a log of the items that recently entered the inventory, and whether each has been seen. That needed a save format bump, to 42.
  - The tab bar above the map, inventory, magic and stats screens moved to a new `ControllerMenus` layer, sized relative to the screen height like the HUD. It now has the mock-up's styling.
  - `InventoryScreen` replaces the inventory window in controller menus mode. It has the Recent, Equipment, Consumables, Notes, Tools and Misc tabs, the equipment areas, ring hand choice, the details panel with comparisons, dropping and sorting.
  - The `inventory-screen` scenario covers it.
- Lessons learned:
  - The console's Lua mode mangles backslashes in code, so Lua sent through the harness builds them with `string.char(92)`.

- First review changes from the user:
  - Each list starts sorted by its main stat, such as armor rating or damage, and the sort button cycles through the list's columns.
  - Armor ratings and damage in the lists are green or red against what the slot holds now.
  - Unseen items past the Recent tab's 40 stay in the log, so they keep their "New" markers.
  - The Notes tab shows the highlighted book's text in a wide details panel, which the right stick scrolls.
- The user's test save now carries a wide spread of items for testing: armour and clothing for every area, every weapon kind, potions, ingredients, scrolls, books, tools and misc items. The character's strength was raised to 240 so they can carry it all. The original save is kept next to it as `Test-before-items.omwsave.bak`.
- Built the magic screen, recorded in [design/magic.md](design/magic.md):
  - The player's save now also keeps the spells, powers, magic items and scrolls the player recently readied or cast. It shares save format 42 with the recent items, and older saves load with an empty list.
  - `MagicScreen` replaces the magic menu in controller menus mode. The Active tab shows the effects on the character, the spell ready to cast, and recent spells to switch to. The other tabs list spells by school, powers, magic items and scrolls, with a details panel.
  - The inventory and magic screens share a base class, sizes and drawing code in `controllerscreen.cpp`.
  - The `magic-screen` scenario covers it.
  - Y jumps between the spell schools, and between the Self, Touch and Target groups of items and scrolls.
- Fixed the menus reopening on the inventory tab after the player closed them on the map tab. They now reopen on whichever tab was open.
- The user's test save now also knows 24 spells from every school. The save from before that is kept as `Test-before-spells.omwsave.bak`.
- Built the stats screen, recorded in [design/stats.md](design/stats.md):
  - `StatsScreen` replaces the stats menu in controller menus mode, with Character, Skills and Factions tabs and a details panel.
  - The Character tab shows each attribute's bonus at the next level up, which the normal menu only shows in a tooltip.
  - The `stats-screen` scenario covers it.

- Started on the dialogue window. The `controller-dialogue` scenario talks to Arrille, a trader in Seyda Neen, and picks a topic.

Next: design the dialogue window with the user.
