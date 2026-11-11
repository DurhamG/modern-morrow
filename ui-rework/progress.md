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
  - After each change, launch the user's test save on their right monitor for them to look at: `omw.py launch -p review --user-settings --resolution 3840x2160 --fullscreen --screen 1 --save "C:/Users/Durham/Documents/My Games/OpenMW/saves/Lendias/Test.omwsave"`.
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
- In controller menus mode, saving shows a small panel in the bottom right corner instead of a box at the bottom middle. After the save, the panel stays for a moment with its bar full, then hides. Before, a save too quick to draw the panel showed "Saving..." as a message in the middle of the screen, which most saves were.
- `[Terrain] distant terrain`, `[Fog] radial fog` and `[Fog] sky blending` are now on by default, so release builds hide the far clip plane the way the review launch did. The review launch no longer sets them.
- The game starts straight into the newest save, without the logos or the main menu, set by `[Saves] load newest save`. With no saves, the main menu shows as usual. The controller pause menu has New Game after Load, since the main menu no longer shows on its own.
- The controller pause menu wraps around, so Up from Save reaches Exit. The harness's virtual pad can't test it, because the menu reads the d-pad from the first controller, which is the user's real one.
- Map pins for controllers. On the local map, X places a red pin, using OpenMW's map notes, so pins are saved with the game. While the crosshair is hidden, the pin goes on the player. The left stick shows and moves the crosshair, which X then pins at, and the view scrolls with it near the edges. On a pin, X removes it, asking first when a mouse gave it a note. Pins in exterior cells also show on the world map, for mouse players too. The harness's virtual left stick works here, unlike its d-pad in the pause menu.
- The d-pad zooms both controller maps: Up zooms in, Down zooms out, with a Zoom hint. It no longer pans, since the right stick does. A zoom keeps the crosshair still when it shows, or the player when in view, or the middle of the view. Mouse-wheel zoom now stops at its limits instead of refusing the last step.
- Turned on by default, after checking the modding-openmw.com settings guide: `best attack`, `show melee info`, `show projectile damage`, `show enchant chance`, `show effect duration`, `swim upward correction`, `prevent merchant equipping`, `[Water] shader`, and `[General] texture mipmap = linear`, which blends between the smaller copies of a texture so no line shows where a distant floor turns blurrier. They keep Morrowind's rules. Left off: water refraction and shadows, which cost the most on a Steam Deck; balance changes such as `rebalance soul gem values`; and settings that need mods. Subtitles stay off.
- In controller menus mode, text uses OpenMW's TrueType copies of Morrowind's fonts, MysticCards and DemonicLetters, even when `openmw.cfg` names Morrowind's bitmap fonts, as the setup wizard writes it. The glyphs are made at the size of the controller screens' large text on the current screen, 22 units of the 720-unit canvas. Before, the bitmap font was stretched about three times on a 4K screen and looked blurry. A TrueType font that a mod names in `openmw.cfg` is still used.
- `ModernMorrow-AutoUpdate.sh` shows a zenity window with a progress bar while it downloads an update, so a Steam start no longer looks stuck. When the first download fails, it shows an error window instead of exiting with nothing on screen. On a system without zenity, or with no display, it downloads as before. Tested in a container with a stand-in zenity, including one that fails to open. Not yet tried on a Deck.
- A 3D view of the local map, for controllers, as a prototype. On the local map, clicking the right stick switches between the flat map and the 3D view. The right stick pans, the d-pad zooms and Y points the view at the player again. A camera of its own draws the game's scene into a texture, tilted 55 degrees, only while the view shows. Near the player, the view shows the loaded cells, NPCs included. Farther out, it shows distant terrain and the merged distant objects, without the view distance setting's limit. Each zoom level has one fixed detail for everything in view, so panning doesn't make objects pop. Not yet done: door markers and pins, interiors without their ceilings, fog of war, and loading ahead while panning.
- The 3D map has fog of war outdoors. The local map already records, for each exterior cell, which parts the player has explored, as a 32 by 32 mask. The map puts the masks of a square of 64 by 64 cells around the view into one texture, from the loaded cells' live masks and the saved masks of cells the player left. After the 3D view draws, a second pass works out each pixel's world position from the depth, and paints unexplored places in the background colour, so nothing of them shows, with a short fade at the edge of what was explored. The square is made again when the view moves 16 cells from its middle. Interiors show no fog in the 3D view yet, and the console's ToggleFogOfWar turns it off, as on the flat map. Closing the map, or leaving the Local tab, now stops the 3D view drawing. Before, it kept drawing behind the game. The map remembers the 3D view, and opening it again points the view at the player with the fog as it is then.
- The 3D map builds new land on a background thread. Panning or zooming used to stall the game while the pieces of land for the new place or zoom were built in the middle of a frame. Now the view keeps drawing the pieces it has while the next ones are built, then switches to them. The pieces reach three times the camera's distance, so a short pan still finds them. Zooming in steps with the harness showed no longer frames than idle frames. The harness can't pan, because the user's real controller owns the sticks.
- The 3D map shows markers. Pins show anywhere in the world outdoors, and in the player's cell indoors. Door markers show for the cells the player has been to, as on the flat map, and hide under the fog of war. Looking up a cell's doors can load the cell, so the view adds the doors of at most two unloaded cells a frame, nearest first. Detected creatures, keys and enchanted items, and the Mark spell's place, show too. A crosshair stays in the middle of the 3D view, and X places a pin there or removes the pin under it, as on the flat map. Door markers now carry the door's height, for the 3D view.
- The 3D map works indoors. A clip plane 200 units above the player's feet cuts away ceilings and upper floors, using the clip position the shaders already write for the water's reflection. Indoors the view looks at the player's height, builds no distant land, and has a zoom of its own, starting at 2000 units. The fog of war covers interiors too: the local map makes the interior's fog image on its turned grid, and the 3D view's fog takes any transform from world positions to the image. Door markers above the cut hide. With the fog on, empty background also takes the fog's colour, which hides water with nothing under it, such as around a cave. Outdoors, the 3D map now starts two zoom steps further out.
- In controller menus mode, the local map is now always the 3D view, without the right stick's switch to it. The flat local map's controller code went with the switch: the left-stick crosshair, the right stick's panning of the flat map and its d-pad zoom. The d-pad still zooms the world map, and mouse players keep the flat local map.
- The controller menus work from the keyboard. While a controller menu shows, ControllerManager::keyToMenuButton turns a key into the button it stands for and sends it through the same path as a real button: arrows for the d-pad, Enter or Space for A, Esc or Backspace for B, X and Y, Q and E for the shoulders, F and R for the sticks' clicks, and Tab and Shift+Tab for the triggers. The screens that read a held d-pad or the right stick each frame see held keys too, with W, A, S and D as the right stick. The held keys come from key events, so the harness's keys work as well as real ones. A screen that takes typing, which is only the name entry screen, says so through WindowBase::takesTypedText, and gets the keys as they are, except Esc. Before, the menus did nothing without a controller, such as when its battery ran out.
- The controller menus work with the mouse. Pointing highlights, a left click chooses, the right button goes back, the wheel moves through lists and scrolls text, and tabs and button hints can be clicked. On the 3D map, dragging pans and a click places or removes a pin. ControllerUi::addPointerTarget makes the clickable areas, and ControllerUi finds the one under the mouse itself, since MyGUI keeps the mouse within modal windows, whose panels the controller menus draw apart from the window. Every scrolling list now uses the shared ControllerList, which owns the focus and the scroll, never scrolls past the end, and takes a scroll position for "next group". See [mouse.md](mouse.md). The harness's injected wheel and text events arrive wrongly under sdl2-compat, so the wheel was checked by reading the code and by logging a wheel event, not by a harness scenario.
- Toggle Sneak is now on by default, so one press of Sneak starts sneaking and the next stops it. It is a setting of the built-in input scripts, kept in the player's script storage, and it now also shows on the Prefs tab of the options, where the controller panel can change it. A check button with the `ScriptCheckButton` setting type reads and writes a boolean in that storage through `MWBase::LuaManager::getPlayerStorageBool` and `setPlayerStorageBool`. The scripts write a setting's default only when the storage has no value, so a player who has already played keeps the stored Off until they change it.
- The 3D map names doors and pins, as the flat map's tooltips do. A label below the marker nearest the crosshair says where a door leads, or what a pin's note says. A door within reach is named before a pin, since most pins have no note. A door's name leaves out everything up to the first comma, which is the town in Morrowind's interior names, so "Balmora, Guild of Mages" reads "Guild of Mages". While the mouse points at the view, the label names the marker under the pointer instead, until the pointer leaves the view or the right stick pans it. Doors under the fog of war aren't named, and pins without a note have no label.
- New setting `[Game] run fatigue`, shown as Run Fatigue on the Prefs tab of the options, with the choices 100%, 50%, 25% and 0%. It multiplies the fatigue the player loses by running on land, which comes from Morrowind's fFatigueRunBase and fFatigueRunMult. 100%, the default, keeps Morrowind's rules. Other actors, swimming and sneaking are unchanged. Walking takes holding a control stick part of the way, so controller players run more than keyboard players do. Tested with the harness: three seconds of running cost about 6 fatigue at 100% and none at 0%.
- Quests can be pinned in the controller journal. X pins the highlighted quest, or unpins it, and pinned quests show in a Pinned group at the top of every tab. The pin is saved with each of the game's quest IDs that share the quest's name, as an optional field of the quest's record from save format version 43. Older saves load with nothing pinned. The harness's virtual d-pad doesn't move the journal's selection while the user's Steam Controller is connected, because held buttons are read from the first controller, so the test used the arrow keys.
- The controller journal links topics, as OpenMW's journal book does. The topics the player has asked about are highlighted in a quest's entries, found with the same keyword search the book uses. A moves the focus into the entries, the d-pad steps between topics, and A opens a topic's page with what each person said about it, newest first. Topic pages link the topics in them too, and B steps back one page at a time. Topics are only reached from quests, with no index of every topic. Directions the player was given, such as the way to Arkngthand, were otherwise out of reach in the controller journal.
- Run Fatigue now scales what running takes from the player after recovery, not the whole running cost. Fatigue comes back all the time it isn't full, about 3 to 4.5 a second, while running costs 5 to 7 a second, so at 50% of the whole cost running drained nothing for nearly every character. Measured over three seconds of running from 100 fatigue: 100% lost 5.6, 50% lost 2.6, and 0% lost none.
- Linux crash reports work without a debugger. A Steam Deck has no gdb or lldb, so a crash log only named the signal. The crash handler now records the crashed thread's return addresses with glibc's backtrace(), and when no debugger can report, the crash log lists each with its file and offset, which the matching release build's debug information turns into function names. The game's raw error output also goes into openmw.log on Linux, so the C library's reason for aborting, such as "free(): double free detected", or the message of an exception that ended the game, is saved. Under Steam, that output went nowhere anyone could see. Both were tested in WSL with a test program built from the real files, since this machine has no Linux build of the game.
- The Linux release now saves its debug information. It builds as RelWithDebInfo rather than Release, the package's programs are still stripped, and the debug information for each program goes into a separate `-symbols.tar.xz` file, stored as an artifact of the workflow run rather than a file of the release. `ui-rework/releasing.md` explains how to name a crash log's frames with it. Only the symbols step was tested locally, in WSL. The full build change waits for the next GitHub run.
- The journal has a Notes tab. Journal IDs with no quest name and no entry that finishes them in the game's data are notes rather than quests, such as each faction's rules, gifts from Caius's Blades contacts, the Threads of the Webspinner returned, and Rerlas Mon's pets. They can never finish, so in the quest list they stayed in progress forever. The quest tabs now leave them out, unless pinned, and the Notes tab lists them newest first. That is 79 of the 82 unnamed IDs. The other 3 can be finished and stay with the quests. Tested in the game with notes added from the console.
- Fixed a crash when opening a book or scroll in controller menus mode. The window manager makes a window visible before it gives the window its new object, and the book and scroll windows drew their reading panel on opening from the book they held before. When that book's cell had been unloaded since, such as after loading a save or travelling, the stored reference pointed at freed memory. Reproduced with the old code by reading a world book, changing cells a few times and quickloading, then reading another: the log showed "Bad LiveCellRef cast to BOOK4 from Static". The windows now forget their book once they are closed rather than hidden under another window, the way OpenMW's container and barter windows forget their object, so the panel has nothing to draw until the window is given its new book.
- Fixed the same crash in the other controller service windows. The companion, enchanting, spellmaking, travel, spell buying, training and merchant repair windows drew their controller panels on opening from the object they held before, and travelling unloads the guide's cell. The dialogue window kept its actor after a guard sent the player to jail. Each now forgets its object once closed rather than hidden, like the book window, and the enchanting and spellmaking panel skips drawing until it has one. Tested by opening a caravaner's travel window, changing cells and quickloading, then opening it again, and by opening travel and persuasion from a conversation and backing out.
- The inventory has a Weight tab, after Misc. It lists every item with its count and the stack's total weight, sorted by the total, heaviest first. One item's weight shows in the details panel. The other tabs only show one item's weight, so a heavy pile of light items was hard to spot. Checked in the game at 1920x1080 and 1280x800, where the seven tabs still fit beside the armour, encumbrance and gold.
