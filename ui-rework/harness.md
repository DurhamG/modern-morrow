# Automation harness

The harness lets an agent run the game and work with it the way a player would, without the user's help. It has three parts:

1. an **automation server** built into `openmw.exe`, in `apps/openmw/mwautomation/`. It is off unless the game is started with `--automation-port <port>`.
2. a **command-line client**, `ui-rework/tools/omw.py`. It launches the game in an isolated profile, sends it commands, and runs scenarios. It uses only Python's standard library.
3. **scenarios** in `ui-rework/scenarios/`. Each one is a script that puts the game into a known state and saves screenshots and UI dumps.

## Quick start

Run these from the repository root. Python is `python` on this machine.

```bash
python ui-rework/tools/omw.py launch --start "Balmora, Caius Cosades' House"
python ui-rework/tools/omw.py pad button=b
python ui-rework/tools/omw.py screenshot
python ui-rework/tools/omw.py ui
python ui-rework/tools/omw.py quit
```

To check one or more screens at every target resolution:

```bash
python ui-rework/tools/omw.py scenario inventory barter -r all
```

The output goes to `.automation/out/<scenario>/<resolution>-x<scale>/`. Each shot saves `NAME.png` and `NAME.txt`, where the text file is the UI dump. `.automation/out/index.html` shows all saved shots, for a person to look through.

After changing engine code, rebuild only the game, which usually takes 10 to 30 seconds:

```bash
cmake --build MSVC2022_64 --config RelWithDebInfo --target openmw
```

Changing a header that many files include, such as `apps/openmw/mwbase/windowmanager.hpp`, makes that rebuild take about two minutes. The game must not be running while it is rebuilt, because Windows locks `openmw.exe`.

## Profiles

`omw.py launch` starts the game with its config folder replaced by `.automation/profiles/<name>/config/`. Git ignores the `.automation/` folder. The profile holds its own settings, log, saves and screenshots, so it never touches the user's own OpenMW folder in Documents. The profile's `openmw.cfg` is copied from the user's `openmw.cfg`, which says where the Morrowind data files are. The game only reads those files.

- `--profile` (or `-p`, or `$OMW_PROFILE`) picks the profile. The default is `default`. Each running game needs its own profile. Scenarios use `scenario-<name>`.
- `--resolution` (or `-r`) is `deck` (1280x800), `tv1080` (1920x1080), `tv4k` (3840x2160), `fold` (2448x1848) or `fold-cover` (1972x1248). The default is `deck`. The two Fold screens are small and dense, so try them with `--scale 2` as well.
- `--scale` sets the UI scaling factor. The default is 1.
- `--set Section.key=value` sets any other setting, such as `--set "GUI.controller menus=true"`.
- The start point is the main menu by default. `--start <cell>` starts a new game in that cell without character creation. `--skip-chargen` does the same in the default start cell, which is outdoors and dangerous. `--new-game` runs character creation, and `--save <file>` loads a save.
- The window goes on SDL display 1, which is the user's second monitor. Use `--screen` or `$OMW_SCREEN` to change it.
- Sound is off, and the startup and new-game movies are skipped. `--sound` and `--movies` turn them back on. With the movies skipped, the game answers commands about a second after launch.
- The game runs in a window that does not minimize when it loses focus. It does not need focus to receive the harness's input.
- The 2026 Steam Controller works without Steam once the build folder has sdl2-compat and SDL3, as the release builds do. OpenMW's own SDL2 doesn't recognize it. To set up a build folder, run `CI/fetch_sdl2_compat_windows.sh MSVC2022_64/RelWithDebInfo` after building. Later builds keep the new DLLs, because vcpkg only copies its SDL2.dll over an older file.
- `--steam` starts the game through Steam, so a real controller gets Steam Input.
  - Steam needs a non-Steam game shortcut whose target is the build's `openmw.exe`, with the launch options `--replace=config --config "D:\Code\openmw\.automation\steam-config"`. The shortcut must point at the game itself. Steam Input only works in the program Steam starts, so a script that starts the game doesn't work.
  - Those launch options never change. Before each launch, the client points `.automation/steam-config` at the profile's config folder, using a directory junction. It writes the options that would have gone on the command line into the profile's `openmw.cfg`, such as the automation port and the save to load.
  - The client finds the shortcut in Steam's `shortcuts.vdf`, and finds the game's process by the `steam-config` path in its command line. Only one game started through Steam can run at a time.

### Handing the game to the user

The `pad` command connects a virtual gamepad, which stays connected. OpenMW reads sticks only from the first gamepad connected, so a real gamepad plugged in later seems not to work. Send `pad action=disconnect` before handing a running game to the user. It works the other way too. When a real controller is already connected, such as the Steam Controller under sdl2-compat, the game ignores the virtual pad's sticks, though its buttons still work. To test stick input, turn the real controller off first.

## Commands

`omw.py <command> key=value ...` sends one command and prints the reply. The values are read as JSON when possible, so `frames=10` is a number and `hidden=true` is a boolean. A `path` value is made absolute, because the game's working folder is the build folder.

| Command | Parameters | What it does |
| --- | --- | --- |
| `ping` | | Replies straight away. |
| `state` | | The game state (`menu`, `running` or `ended`, which means the player died), the open UI modes, the window size, the UI scale and the current cell. |
| `wait` | `frames` or `seconds` | Waits, then replies. |
| `screenshot` | `path`, `scale`, `crop=[x,y,w,h]` | Captures the next frame, including the UI. The format comes from the extension. |
| `key` | `key`, `action` (tap, down, up), `hold`, `mods` | Presses a key by its SDL name, such as `Escape`, `Return`, `A`, `F1` or `` ` ``. A tap holds the key for `hold` frames, 2 by default. |
| `key` | `text` | Types text. |
| `mouse` | `action` (click, move, down, up, wheel), `x`, `y`, `button`, `hold` | Mouse input in window pixels from the top left. A click moves first, then waits a frame before pressing. |
| `pad` | `action` (connect, disconnect) | Plugs the virtual gamepad in or out. Other `pad` commands plug it in first. |
| `pad` | `button`, `action` (tap, down, up), `hold` | Virtual gamepad buttons: a, b, x, y, back, guide, start, leftstick, rightstick, leftshoulder, rightshoulder, dpup, dpdown, dpleft, dpright. |
| `pad` | `axis`, `value`, `hold` | Sticks (`leftx`, `lefty`, `rightx`, `righty`) take -1 to 1, with positive y pointing down. Triggers (`lefttrigger`, `righttrigger`) take 0 to 1. With `hold`, the axis returns to rest after that many frames. Without it, the axis stays where it was set. |
| `touch` | `action` (tap, down, move, up, drag, pinch), `finger`, `x`, `y`, `path`, `frames`, `release`, `from`, `to`, `steps` | Touch screen fingers, in window pixels from the top left, for the touch controls. A tap goes down and up in the same frame. A drag goes down on the first point of `path`, a list of `[x, y]`, moves through the others `frames` frames apart, and lifts unless `release=false`. A pinch puts two fingers `from` pixels apart around `x`, `y` and moves them to `to` pixels apart over `steps` frames. Down, move and up take a `finger` from 0 to 9, for gestures of your own, such as holding still with `wait` between them. |
| `console` | `command`, `frames` | Runs a console command and returns what the console printed. Fails if the console printed an error. |
| `lua` | `code`, `context` (player, global, menu), `frames` | Runs Lua as the console's Lua mode does, and returns what was printed. A lone expression's value is printed. |
| `ui` | `hidden`, `root`, `depth` | The widget tree. See below. |
| `quest-targets` | `path` | Writes JSON to `path` with what the quest markers would mark at every stage of every quest, by both rules: each way forward's next stage, and its objectives with the cells that place each target. `corrected` has the ways the default rule chooses with the stage's entry in `questcorrections.yaml`, and the entry's targets, and `hasCorrection` says whether there is one. For reviewing the markers in bulk. |
| `quest-live-check` | `path`, `out` | Sets every stage of every review record in the folder `path` in the running game, one at a time with only that quest tracked, and writes to `out` the stages whose markers differ from the record: targets shown that the record lacks, and targets the record marks that aren't shown. The game's checks and where the player stands cause some differences, so it is a spot check. It changes the game's journal, so run it in a throwaway session. |
| `quest-review-check` | `path` | Checks the quest markers against the review records in the folder `path`. It lists the reviewed stages whose markers have changed since the review, records that can't be read, correction entries left out because they don't match the content, and entries that change nothing. |
| `quest-review` | `path`, `quests` | Writes a review sheet for each quest into the folder `path`, or only for the quests whose IDs start with one of the comma-separated prefixes in `quests`, such as `A1_,A2_`. A sheet has the quest's journal entries, what the markers show at each stage, the ways forward the analysis found with their checks, the lines and scripts that move the quest on, and a review record of what the markers show now. Sheets hold the game's text, so write them under `.automation/`. |
| `quest-track` | `quest`, `tracked` | Tracks a quest's markers, or stops tracking them with `tracked=false`, as the journal does. |
| `quest-markers` | | The quest markers shown now, worked out again first. It lists the tracked quests and where the player stands, then the markers in the player's place (`local`) and outdoors for the world map (`world`). Each marker has its quest, its task, its target object, the cell the target is in, the cell the marker is in, whether it is on a door on the way to the target, and whether it is optional, for a side step that's drawn hollow and left off the compass, and where the marker is. A target that is a place has no object. |
| `layout` | `path`, or `close` | Shows the widgets from a MyGUI layout file, replacing the ones from the last `layout` command. `close=true` removes them. The file is read each time, so a mock-up can be changed and shown again without rebuilding. Mock-ups go on the `Mockup` layer, which is sized relative to the screen height on a canvas 720 units tall. |
| `quit` | | Exits the game cleanly. |

The client adds these commands of its own:

- `omw.py screenshot [path] [--scale S] [--crop X Y W H]` waits two frames, then captures to `shots/` in the profile folder or to the given path.
- `omw.py ui [--root NAME] [--depth N] [--hidden] [--json]` prints the widget tree, one widget per line. Without `--hidden`, it leaves out hidden widgets, and widgets on no layer, which are never drawn. The options window builds its Lua settings pages that way, before it shows them.
- `omw.py click-text TEXT` clicks the visible widget that shows TEXT. `--name` matches the widget name instead.
- `omw.py run FILE` runs a file of commands against a running game.
- `omw.py scenario NAMES -r RESOLUTIONS` launches, runs and quits scenarios. Use `all` for every scenario or every resolution.
- `omw.py status`, `omw.py log [--errors]`, `omw.py stack` and `omw.py clean` report on, or remove, profiles.

### Reading the UI dump

Each line of `omw.py ui` describes one widget: its type, name, position and size, and its text with font and pixel height. Flags such as `hidden`, `disabled`, `keyFocus` and `mouseFocus` are listed when they apply. Window roots are usually named `_Main`, so the window's caption in `text=` is the easiest way to tell them apart. The inventory screen dumps to about 450 lines. Use `--root` or `--depth` to narrow it.

### Crashes

When the game crashes, OpenMW writes `openmw-crash.dmp` in the profile's config folder. It then shows a "Fatal Error" message box and waits. The client notices the dump while it waits for a reply, and says so. `omw.py stack` prints the crashing call stack using `cdb.exe` from the Windows SDK. Then `omw.py quit --kill` stops the process.

## Scenarios

A scenario file's first line is `launch` followed by launch options. Its other lines are commands, one per line, with the same syntax as on the command line. Lines starting with `#` are comments. Scenarios can also use:

- `shot NAME` saves a screenshot and a UI dump,
- `click-text TEXT` clicks a widget by its text,
- `click-name NAME` clicks a widget by its name,
- `relaunch [OPTIONS]` quits the game and launches it again in the same profile, with the launch options given, such as none, which carries on from the newest save.

Scenarios can check what the game shows. A check that fails stops the scenario with the file, the line and what it found, and `omw.py scenario` then exits with 1. Texts match a part of a widget's text, ignoring case, and only visible widgets count. Quote a text with spaces.

- `expect-text TEXT` and `expect-no-text TEXT`: a visible widget shows TEXT, or none does.
- `expect-name NAME` and `expect-no-name NAME`: a visible widget has the name, as the UI dump shows it.
- `expect-focus NAME`: the widget with the key focus has the name, or the type when it has no name, as the UI dump's first line shows it. Typed text goes to that widget.
- `expect-order TEXT TEXT ...`: the texts are on the screen in reading order, each lower than the one before, or on the same row and further right.
- `expect-mode MODE` and `expect-no-mode`: the topmost UI mode, as `state` names it, such as `Inventory` or `MainMenu`, or no menu open.
- `expect-state STATE`: `menu`, `running` or `ended`.
- `expect-lua CODE VALUE [context=NAME]`: running the Lua code prints VALUE, such as `expect-lua "print(1 + 1)" 2 context=player`.
- `expect-marker TARGET [quest=ID] [map=local|world] [optional=yes|no]` and `expect-no-marker ...`: a quest marker is for the target, or none is. The target is an object ID, such as `caius cosades`, or the name of a cell for a place. Both maps count unless `map` picks one, and both kinds of marker unless `optional` picks one. The markers are worked out again first, so a `journal` console command just before is enough.
- `expect-quest-review FOLDER`: every reviewed quest stage in the review records in FOLDER, relative to the repository, still shows what its record says, and the corrections file has no entries that were left out or that change nothing. The `quest-review` scenario runs it on `ui-rework/quests/reviewed`.
- `wait-text TEXT [timeout=SECONDS]`, `wait-mode MODE [timeout=SECONDS]` and `wait-no-mode [timeout=SECONDS]`: wait until the text shows, the mode is on top, or no menu is open, checking every five frames, for up to 10 seconds unless a timeout is given. They are better than a fixed `wait`, which is too long on a fast machine and too short on a slow one.

A `golden NAME` line compares the UI with a stored copy, and fails with a diff when they differ. The comparison keeps each visible widget's depth, type, name and text, and leaves out positions, sizes, fonts, fading and focus, so only a real change in what a screen shows fails it. `omw.py scenario NAMES --update-golden` stores the copies instead of comparing. They go in `.automation/golden/<scenario>/<resolution>/`, which git ignores, because they hold Morrowind's text. So they compare a run before a change with one after it, on the same machine.

Scenarios can read and change save files with `ui-rework/tools/savefile.py`. Paths are relative to the profile's data folder, `.automation/profiles/<profile>/data/`, which holds the `saves` folder, such as `saves/*/*.omwsave`. A `*` matches one name, since save file names come from the save's description. Each pattern must match exactly one file.

- `save-format PATH VERSION`: the save is at that format version.
- `save-has-record PATH NAME` and `save-no-record PATH NAME`: the save has a record of that name, such as `UIST`, the controller menus' record, or has none.
- `save-strip PATH NAME OUT`: writes a copy of the save to OUT without the records of that name. A copy without `UIST` is what an OpenMW version without the controller menus writes.
- `save-set-format PATH VERSION`: changes the save's format version in place, such as to 44 to see that a save too new is refused.

- `expect-file PATTERN` and `expect-no-file PATTERN`: a file in the profile's data folder matches the pattern, or none does, such as `backups/OpenMW/*/*.omwsave`.
- `remove-files PATTERN` deletes the files in the profile's data folder that match the pattern, such as `saves/*/*.omwsave`. A scenario uses it to stand in for a second device with no saves.

`launch --openmw-saves` makes an empty folder in the profile, `data/openmw-saves`, and uses it as OpenMW's saves folder, so the save screens show an OpenMW tab without touching OpenMW's real saves. A scenario puts saves there with `save-strip`.

Cloud saves never reach the real service from a test run. Each launch points the `cloud address` setting at a port on this computer where nothing listens, unless it has `--user-settings`. `launch --cloud` points it at the cloud saves Worker from `cloudsaves/` instead, run on this computer at `http://127.0.0.1:8788`. A scenario with `--cloud` on any of its launch lines starts the Worker before its first launch, with an empty bucket, and stops it at the end. That needs Node.js, and `npm install` run once in `cloudsaves/`. The Worker's log is `.automation/cloud-service/wrangler.log`.

`savefile.py` also works on its own: `savefile.py records FILE`, `savefile.py format FILE [VERSION]` and `savefile.py strip FILE NAME -o OUT`.

A scenario also fails when the game logs an error: a line at OpenMW's error level, or one with `Error in frame`, `Bad LiveCellRef` or `Fatal error`. The check runs at the end of the scenario, and at an `expect-log-clean` line. `ui-rework/tools/log-allowlist.txt` lists errors that are harmless, each with the reason. A scenario that causes an error on purpose says so with `expect-log TEXT`, which fails when no line has TEXT, and lets the lines that have it pass the check.

For example, `barter.txt`:

```
launch --start "Balmora, Clagius Clanler: Outfitter"
console "command=player->additem gold_001 500"
console "command=\"clagius clanler\"->forcegreeting"
wait frames=10
shot dialogue
click-text Barter
wait frames=10
shot barter
```

Each scenario runs in its own profile, so different scenarios could run in parallel.

## Known limits

- Key and mouse input goes through SDL's event queue. Code that reads key state directly from SDL, such as Lua's `input.isKeyPressed`, does not see it. Gamepad input does not have this problem, because the virtual gamepad is a real SDL device.
- Under sdl2-compat, which the build folders use for SDL3, two kinds of injected events arrive wrongly. A `mouse action=wheel` event arrives with the mouse's height as its wheel step, so it always scrolls up. Text from `key text=...` crashes the game in SDL. Keys, clicks and mouse moves work.
- In gameplay, the mouse turns the camera, and the game ignores mouse motion unless the window has focus. Use the right stick to turn instead.
- The `tv4k` window is larger than the user's 2560x1440 monitors. It still renders and captures at full size.
- The main menu buttons show images instead of text. They are named `MainMenu_<id>`, such as `MainMenu_options`, so use `click-name`.
- There is no reference-image comparison yet. There are also no automatic checks against the size and layout guidance in [plan.md](plan.md), and nothing pauses animations to make screenshots repeatable.
