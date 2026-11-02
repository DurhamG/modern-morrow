# Automation harness

The harness lets an agent run the game and work with it the way a player would, without the user's help. It has three parts:

1. an **automation server** built into `openmw.exe`, which accepts commands over a local network connection,
2. a **command-line client** written in Python, which launches the game and sends it commands,
3. a set of **scenarios**: scripts that put the game into a known state, such as the inventory screen with a full set of items.

## What already exists in OpenMW

The engine has no network or other inter-process connection today. It does have several pieces the harness can build on:

- **Screenshots:** the F12 key captures the final frame, including the UI, through `ScreenCaptureHandler` (`apps/openmw/engine.cpp`, `components/sceneutil/screencapture.cpp`). It always writes a numbered file in the user data folder. The harness needs to choose the path and know when the file is written.
- **Startup options:** `--skip-menu`, `--new-game`, `--load-savegame`, `--start <cell>`, `--script-run <file>` (runs console commands once the game is running), `--no-sound`, `--no-grab`, `--user-data` and `--config`.
- **Lua scripts:** menu scripts can start, save and load games. Player scripts can change UI modes, for example `I.UI.setMode('Interface')`. `core.quit()` exits the game cleanly. Lua cannot send raw key presses or click MyGUI buttons.
- **The console:** commands go to the legacy script interpreter. A command starting with `lua` goes to the Lua console instead.
- **Integration tests:** `scripts/integration_tests.py` runs the game with Lua test scripts and reads results from the log. It runs only on Linux, under a virtual display.
- **Input:** all input arrives through one SDL event loop in `components/sdlutil/sdlinputwrapper.cpp`. The game reads most input from these events. It also reads some state directly from SDL, such as which keys are held and the position of controller sticks.

## Automation server

The server is off unless the game is started with an `--automation-port <port>` option. It listens on `127.0.0.1` only.

Each request is one line of JSON, and each reply is one line of JSON with the same `id`. The server reads requests once per frame on the main thread, so commands never run at the same time as the rest of the game update. Commands that take time, such as waiting for frames or taking a screenshot, reply when they finish.

### Commands

| Command | What it does |
| --- | --- |
| `ping` | Replies straight away. Used to check that the game is up. |
| `wait` | Waits for a number of frames, or a number of seconds of game time, before replying. |
| `key` | Presses, releases or taps a key. Can also type text. |
| `mouse` | Moves the mouse, or presses and releases a button, at a position in window pixels. |
| `pad` | Connects or disconnects a virtual gamepad, presses or releases its buttons, and moves its sticks and triggers. |
| `screenshot` | Captures the next rendered frame, including the UI, to a given path. Can scale the image down to save space. |
| `ui` | Returns the MyGUI widget tree as JSON. Each widget includes its type, name, caption text, position and size, whether it is visible and enabled, and its font size. The reply also lists the open UI modes and which widget has focus. |
| `console` | Runs a console command, using the legacy script language. |
| `lua` | Runs Lua code in the player or menu context and returns the result. |
| `state` | Returns the game state: main menu, loading, or playing, and the current cell. |
| `quit` | Exits the game cleanly. |

Key and mouse input is sent with `SDL_PushEvent`. The events then go through the same loop as real input, so MyGUI focus and key bindings behave normally. Code that reads key state directly from SDL will not see these presses. We'll fix that if it turns out to matter.

Gamepad input uses SDL's virtual joystick feature (`SDL_JoystickAttachVirtualEx`). The game sees a real controller device, with real button and stick state. The SDL bundled with OpenMW on Windows is 2.32 and has virtual joystick support built in.

## Client

The client is `ui-rework/tools/omw.py`. It uses only Python's standard library. It is meant to be run by the agent from a shell.

- **`omw.py launch`** starts the game in an isolated profile folder under `.automation/` in the repository, which git ignores. The profile has its own settings, saves, logs and screenshots, so it never touches the user's own files. The profile's `openmw.cfg` points at the Morrowind data files listed in the user's own `openmw.cfg`, and the game only reads those files. The launch options choose the resolution profile (`deck`, `tv1080` or `tv4k`), the UI scaling factor, and the starting point: the main menu, a new game, or a save. The game runs in a window, with sound off, and does not minimize when it loses focus.
- **`omw.py <command>`** sends one command to the running game and prints the reply.
- **`omw.py run <script>`** runs a file of commands, one per line, and stops at the first failure.
- **`omw.py quit`** exits the game. If the game does not exit, the client stops the process.

Each running game gets its own port and profile folder. That means several agents can each run their own copy of the game at the same time, for example from separate worktrees.

## Scenarios

A scenario is a command script that gets the game into a known state and takes screenshots. For example, the `inventory` scenario starts from a fixed save, adds a known set of items, opens the inventory, and takes a screenshot and a UI dump. Scenarios can be run at each resolution profile. The output goes into a folder of screenshots and dumps that both the agent and the user can look through.

Saves used by scenarios are created by setup scripts from a new game. They live in the ignored `.automation/` folder and are not committed.

## Order of work

Each step below is a separate commit that builds.

1. Docs folder, branch, and ignore rules for `.automation/`.
2. Client `launch` and `quit` commands, with isolated profiles. No engine changes yet.
3. Automation server in the engine with `ping`, `wait`, `state` and `quit`.
4. `screenshot`.
5. `key` and `mouse`.
6. `pad`, using a virtual gamepad.
7. `console` and `lua`.
8. `ui`, the widget tree dump.
9. Setup scripts for a fixed save, and the first scenarios: main menu, inventory, dialogue, map, journal, and the controller menus.
10. A smoke test that runs every scenario at every resolution profile. Update `CLAUDE.md` with how to use the harness.

Possible later additions:

- comparing screenshots against saved reference images,
- automatic checks of the UI dump against the size and layout guidance in [plan.md](plan.md),
- pausing animations so screenshots are repeatable.
