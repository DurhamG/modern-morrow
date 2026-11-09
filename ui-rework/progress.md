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
  - After each change, launch the user's test save on their right monitor for them to look at: `omw.py launch -p review --user-settings --set "Terrain.distant terrain=true" --set "Fog.radial fog=true" --set "Fog.sky blending=true" --resolution 3840x2160 --fullscreen --screen 1 --save "C:/Users/Durham/Documents/My Games/OpenMW/saves/Lendias/Test.omwsave"`.
  - The test save stands next to Fargoth in Seyda Neen. To open a conversation with him for review, send `omw.py send -p review console "command=\"fargoth\"->forcegreeting"`. ForceGreeting works on any NPC near enough to be loaded, wherever the player stands.
  - To open a loot window in the test save, kill Fargoth with `omw.py send -p review console "command=\"fargoth\"->sethealth 0"`, then open his body with `omw.py send -p review lua "context=global" "code=local w = require('openmw.world'); for _, a in ipairs(w.activeActors) do if a.recordId == 'fargoth' then a:activateBy(w.players[1]) end end"`. Setting his health isn't a crime, so the guards stay calm. Don't save afterwards.
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

- Designed the dialogue window with the user, recorded in [design/dialogue.md](design/dialogue.md). `ui-rework/mockups/dialogue.py` builds its mock-ups, over the game with the NPC in view. The mock-up generators now share their helpers in `ui-rework/mockups/mockup.py`.

- Built the dialogue panel. In controller menus mode, an opaque panel along the bottom of the screen replaces the dialogue window. It groups topics into New and Asked, puts services in their own tab, and shows only the choices when a reply is needed.

- During controller conversations, the camera turns to frame the NPC's face above the panel, and turns back when the conversation ends.

- The camera framing in conversations only happens in first person. In third person the camera looked steeply down past the player's head.
- Replaced the menu the start button opens during a game, in controller menus mode. It's a panel in the style of the other screens, over a darkened view of the game, with Save, Load, Options and Exit. B returns to the game. The `controller-pause-menu` scenario covers it. The save dialog, the options and the exit confirmation still use the old windows.

- Started on the loot window. The `controller-loot` scenario opens Fargoth's body in Seyda Neen.

- Built the loot panel, recorded in [design/loot.md](design/loot.md). In controller menus mode, an opaque panel on the right side of the screen lists what a container or body holds, with details and comparisons beside the list. A takes, Y takes all, X takes and equips, and the window closes after the last item. The left stick press swaps to the player's own items, to put one in. The item details code the inventory screen used moved into `itemdetails.cpp`, so both screens share it.

- Built the barter panel while the user was away, recorded in [design/barter.md](design/barter.md). It uses the loot panel's look, with Buy and Sell tabs, a deal box, and haggling on the right stick. The loot panel's item groups and summaries moved into `itemdetails.cpp` for it.

- The user reviewed the loot and barter panels and was happy with them.

- Built the journal tab, recorded in [design/journal.md](design/journal.md):
  - Quest entries in the save now keep the actor the player was talking to and the cell the player was in. They share save format 42 with the recent items and spells.
  - For older entries, `questlog.cpp` finds the quest giver from the conversation that sets the quest's earliest stage, and the place from where that person stands.
  - `JournalScreen` is a fifth tab of the menus in controller menus mode, and the journal key opens it. It lists quests in progress, then finished ones, sorted by the last update, by town, or by faction.
  - The `controller-journal` scenario covers it.
- Lessons learned:
  - `ESMStore::getRefCells` says which cells an object is placed in, without loading the cells.
  - The console's Journal command records the player's cell. With `"arrille"->journal ...` it also records Arrille as the one who gave the quest.

- The user reviewed the journal tab and was happy with it.

- Reworked the rest of the screens while the user was away, recorded in [design/other-screens.md](design/other-screens.md). Each keeps its old window hidden as the model, and shows a panel in the controller screens' style:
  - the merchant services: spells, training, travel and repair,
  - message boxes, confirmations and the class questions, in a shared dialog,
  - rest and wait, its progress, and the level up,
  - books and scrolls, as one scrolling page,
  - the save and load lists,
  - the count dialog,
  - companion sharing,
  - the loading and jail progress,
  - character creation: the name, with an on-screen keyboard, the race, the class and birthsign lists, the class result, and the review,
  - the options, as rows built from the settings window's layout.
- Videos can now be skipped with a controller.
- Each piece has a scenario, from `controller-spells` to `controller-options`. Screenshots for the user's review are in `ui-rework/review/`, which git ignores.
- Lessons learned:
  - The window manager only gives the top dialog its frame updates. A dialog owned by another window, such as the save dialog, still gets them while it is on top.
  - The harness can't take pictures while the game loads or plays a video, because both run their own loops.
  - `GoToJail` and `SetPCCrimeLevel` in the console send the player to jail, for testing.
  - Sleeping is illegal in towns, so the rest scenario moves to the wilderness with `coe -3 -8`.

Next: the user reviews the other screens. Still left: quick keys, crafting, and creating a custom class.

- Designed quick keys for controllers with the user, recorded in [design/quick-keys.md](design/quick-keys.md), and built them:
  - `QuickKeysMenu` stays the model. It gained functions to read, assign and clear a slot, and to find the slot that holds an item or a spell.
  - During play, `ActionManager` reads the d-pad every frame. A press of a direction uses slots 1 to 4, and a hold uses slots 5 to 8. `quickkeyslots.hpp` holds the mapping and the hold time.
  - `QuickKeyAssigner` is the left stick's assigning mode, shared by the inventory and magic screens. It also draws the d-pad marker.
  - The HUD shows the used quick key's icon and name for two seconds.
  - `ControllerUi::drawHints` lays out button hints, and wraps them onto a second row when they don't fit.
  - The `controller-quick-keys` scenario covers assigning from both screens, and using a press and a hold in play.
- Lessons learned:
  - The harness's `pad button=dpup hold=40` holds a button for 40 frames, which is long enough to test holds.
  - Python on Windows writes CRLF line endings unless the file is opened with `newline=''`.

Next: the user tries the quick keys. Still left: crafting, and creating a custom class.

- Pushed the quick keys.
- The short messages shown during play, such as a failed spell, were drawn in screen pixels, so they were tiny on a TV. In controller menus mode, `MessageBoxManager` now draws them on the `ControllerNotification` layer, which scales with the screen height, in the panel style. The `controller-messages` scenario shows a failed cast and three stacked messages.
- The user asked for the crafting screens to be designed and built without a review first, one at a time, recorded in [design/crafting.md](design/crafting.md).
  - Alchemy is a full screen. `AlchemyPanel` lists the ingredients with their known effects, groups the ones that share an effect with the potion, and shows the potion on the right. The alchemy window stays hidden and brews.
  - The `controller-alchemy` scenario adds ingredients, brews two potions and renames one.
  - Enchanting is a full screen. `EffectCraftPanel` shows the enchantment on the right and a list to pick from on the left, and edits effects on the left. The enchanting window and its effect dialog stay hidden. The effect dialog hands its buttons to the panel through a handler.
  - Spellmaking uses the same `EffectCraftPanel`, without the item, soul gem and cast type lines. A new spell is named after its first effect. The `controller-spellmaking` scenario makes a fire spell with Estirdalin and buys it.
  - Repairing with your own tools and recharging with a soul gem share `ToolPanel`, a full screen with the items that need it on the left and the tool on the right. The `controller-repair-tools` scenario repairs a sword, switches tools, and recharges an amulet.
  - The `controller-enchanting` scenario enchants a ring with a soul gem. The `controller-enchanting-service` scenario buys an enchantment from Galbedir and adds an effect on an attribute.
- Built the custom class screen in character creation without a review first, as the user asked. `CreateClassPanel` shows the class on the left and the choices for the highlighted line on the right. The `controller-create-class` scenario names a class, picks a specialization and swaps a skill.
- Persuasion now shows in the question dialog, with the person's disposition and the player's gold. The `controller-persuasion` scenario admires Arrille.
- The inventory's Misc tab is split into keys, soul gems and other things, and the Notes tab into skill books, books, and letters and notes. The `controller-inventory-groups` scenario shows both.
- The next and previous spell keys step through the recent spells in controller menus mode, like switching windows on a desktop. Checked by hand with the harness: three readied spells came up in order, and a pause started over.
- Lessons learned:
  - `Alchemy::countPotionsToBrew` returns 0 until the potion has a name, so the panel gives it the window's name before showing the count.
  - Lua's `I.UI.addMode('Alchemy')` opens alchemy without going through the inventory, for testing. `addMode('Enchanting', {target = gem})` opens enchanting with a soul gem, and `{target = npc}` with an enchanter.
  - The enchanting window can open before it is told who enchants, so the panel starts over when it is told.
  - A global Lua script can teach the player a spell with `types.Actor.spells(player):add(id)`, which helps when a spell's ID isn't known.
  - A global Lua script can damage an item with `types.Item.itemData(item).condition`, and drain a magic item with `enchantmentCharge`, for testing repairs and recharges.
- The HUD leaves out effects that last for good: abilities, such as from race and birthsign, and worn gear with a constant enchantment. Diseases, curses and effects that run out still show.
- Set up release builds, recorded in [releasing.md](releasing.md). Controller menus are now on by default, and the fork keeps its settings and saves in its own folders with its own package names. Pushes build and test on Linux and Windows only. A `v*` tag builds a Windows installer and zip, and a Linux AppImage on AlmaLinux 9, into one draft release. None of it has run on GitHub yet, because GitHub turned Actions off on the fork.
- Tested the release builds locally. CPack built the Windows installer and zip with NSIS. The Linux build ran in an AlmaLinux 9 Docker container, and its AppImage starts the game with or without a system OpenAL, because it carries a fallback copy. GCC's warnings in the new screens are fixed. The launcher, wizard, game window and crash dialogs now say Modern Morrow, and bug reports go to the fork's GitHub issues.
- The release builds run on SDL3 through sdl2-compat, so the 2026 Steam Controller works without Steam. The pinned versions are in `CI/sdl2_compat_versions.sh`. A build folder gets them from `CI/fetch_sdl2_compat_windows.sh`, so the review launch no longer needs `--steam`. On a controller, the tutorial now shows Morrowind's Xbox text, because the `Xbox` script function answers 1.
- The name of whatever is under the crosshair is now a larger panel just below the crosshair, on the HUD's scaled layer. It shows the item's icon, its name, OpenMW's details for the object, the item's effects, and "Stealing" or "Better" markers. "Stealing" always shows, as on the loot screen, and the "show owned" setting only decides the red colouring.
- With no options, the AppImage's start script runs the setup wizard when `openmw.cfg` names no game file, then starts the game. So a Steam shortcut needs no launch options. Dolphin refused to start the AppImage on a Steam Deck even when it was executable, so the README now has players start it from Steam.
- `window mode` now defaults to 1, windowed fullscreen, so a first start fills the screen at its own resolution. The 800x600 window looked almost square in the Steam Deck's Game Mode.
- Tried the upstreaming plan for real, on a local branch outside this checkout: `full` in the worktree `D:\Code\openmw-upstreaming`. It is upstream OpenMW plus 177 small engine commits, in 60 planned merge requests, that give Lua what the UI needs. On top of them, the whole Modern Morrow UI is rebuilt as a Lua mod in `modern-morrow/`, without the map, the race preview's 3D head, the settings window, tooltips and keyboard support. Every engine commit builds on its own, and upstream's tests pass. Nothing is pushed. [upstreaming.md](upstreaming.md) now has a section on what the attempt showed, and the full write-up is `upstreaming-notes/summary.md` in that worktree.
- Each release now has `ModernMorrow-AutoUpdate.sh`, a script for a Steam shortcut that downloads the newest AppImage and starts it. GitHub's API lists each release file with its SHA-256 checksum, and the script updates when that checksum differs from its own copy's, so moving a published release's tag reaches it. It starts the copy it has when GitHub can't be reached. Tested in a container against a fake GitHub, and its parsing against GitHub's real release JSON, but not yet on a real release or a Steam Deck.
- In controller menus mode, the game tells SDL not to show an on-screen keyboard. On a Steam Deck, SDL opened Steam's keyboard whenever a text box took focus, such as the save screen's name box, and closed it at once when the focus moved. Afterwards, some controller buttons stopped reaching the game. Controller screens draw their own keyboard where typing is needed. Not yet tried on a Deck.
- Quicksaves and autosaves now keep a history, with the new `[Saves] quicksave history` setting on by default. Each one makes a new save. The newest 10 are kept, and older ones thin out by time played: one every 5 minutes of play for the last hour, every 30 minutes for the last 6 hours, every 2 hours for the last day, and every 4 hours before that. Each stretch of play keeps its oldest save, so a save that is kept once stays kept until a longer stretch takes it in. Saving every 30 seconds for 60 hours of play leaves 50 saves. Time played rather than game days, because resting, waiting and travel make game days jump.
- The game autosaves after 5 minutes of play without a save, set by `[Saves] autosave interval`. Time in menus doesn't count, and the autosave waits until no menu is open and the player isn't in combat. It goes into the same thinned history as the autosaves from resting.
