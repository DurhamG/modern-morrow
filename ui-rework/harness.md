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
- `--resolution` (or `-r`) is `deck` (1280x800), `tv1080` (1920x1080) or `tv4k` (3840x2160). The default is `deck`.
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

The `pad` command connects a virtual gamepad, which stays connected. OpenMW reads sticks only from the first gamepad connected, so a real gamepad plugged in later seems not to work. Send `pad action=disconnect` before handing a running game to the user.

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
| `console` | `command`, `frames` | Runs a console command and returns what the console printed. Fails if the console printed an error. |
| `lua` | `code`, `context` (player, global, menu), `frames` | Runs Lua as the console's Lua mode does, and returns what was printed. A lone expression's value is printed. |
| `ui` | `hidden`, `root`, `depth` | The widget tree. See below. |
| `layout` | `path`, or `close` | Shows the widgets from a MyGUI layout file, replacing the ones from the last `layout` command. `close=true` removes them. The file is read each time, so a mock-up can be changed and shown again without rebuilding. Mock-ups go on the `Mockup` layer, which is sized relative to the screen height on a canvas 720 units tall. |
| `quit` | | Exits the game cleanly. |

The client adds these commands of its own:

- `omw.py screenshot [path] [--scale S] [--crop X Y W H]` waits two frames, then captures to `shots/` in the profile folder or to the given path.
- `omw.py ui [--root NAME] [--depth N] [--hidden] [--json]` prints the widget tree, one widget per line.
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
- `click-name NAME` clicks a widget by its name.

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
- In gameplay, the mouse turns the camera, and the game ignores mouse motion unless the window has focus. Use the right stick to turn instead.
- The `tv4k` window is larger than the user's 2560x1440 monitors. It still renders and captures at full size.
- The main menu buttons show images instead of text. They are named `MainMenu_<id>`, such as `MainMenu_options`, so use `click-name`.
- There is no reference-image comparison yet. There are also no automatic checks against the size and layout guidance in [plan.md](plan.md), and nothing pauses animations to make screenshots repeatable.
