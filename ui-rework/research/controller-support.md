# OpenMW's gamepad support

This is an assessment of OpenMW's existing gamepad code, made before reworking the menus. It also covers whether OpenMW can use the 2026 Steam Controller's extra inputs. File references are as of upstream commit `16b14001cf`.

## Verdict

OpenMW's gamepad support is workable, but parts of it are hacky. Its device handling is sound for one standard gamepad. Menu navigation with a gamepad was added window by window, rather than built on a shared navigation model.

## What is solid

- **Gamepad API.** It uses SDL's GameController API throughout. Raw joystick events are dropped (`components/sdlutil/sdlinputwrapper.cpp`). It ships a controller mapping database, `files/gamecontrollerdb.txt`, and also loads a user mapping file.
- **Hot-plugging.** Plugging in and unplugging controllers works, through the bundled input binding library in `extern/oics`.
- **Rebinding.** Every gameplay action can be rebound to any gamepad button in Options, Controls. That includes the back paddles, the misc button and the touchpad click, which SDL calls PADDLE1 to PADDLE4, MISC1 and TOUCHPAD. Bindings are saved in `input_v3.xml`.
- **Gyro aiming.** It works when `[Input] enable gyroscope` is on, with axis, threshold and sensitivity settings (`apps/openmw/mwinput/gyromanager.cpp`).
- **Lua access.** Lua scripts can read any button, including the paddles (`input.isControllerButtonPressed`). They can read raw stick axes, and they receive touchpad touch events (`onTouchPress`, `onTouchMove`, `onTouchRelease`).
- **Active development.** About 76 upstream commits since early 2025 touch the input code, including trigger binding, deadzone handling and fixes to controller menus mode.

## What is hacky or missing

- **Only one logical controller.**
  - Every device is reported as device 1 (`bindingsmanager.cpp`: "we only support one controller at a time").
  - Events from all connected pads are merged, while polled state comes only from the first pad. A physical pad plus Steam's virtual pad can produce doubled input.
  - At startup each pad is opened twice and closed once.
- **Menu navigation was added window by window.** With `GUI.controller menus` on, each window handles gamepad buttons in its own `onControllerButtonEvent`. 34 windows have their own version.
  - The default in `WindowBase` returns true, so a window without its own version silently swallows every button, including B.
  - The setting is checked 99 times across 36 files.
- **Escape on every press.** Every gamepad button press also sends Escape to the UI, to skip the intro movie (`controllermanager.cpp`).
- **The menus mode is off by default.** With it off, the left stick moves an emulated mouse cursor and A clicks. The d-pad, face buttons, shoulders and triggers are turned into arrow, Tab and other keys for keyboard navigation. The code calls this a "Temporary mouse binding until keyboard controls are available".
- **Sticks can't be remapped.** Only the triggers can be rebound as axes. Sticks can't be swapped, and look can't be rebound. There is one global, per-axis, linear deadzone, with no response curve.
- **Trackpads only reach Lua.** Touchpad events are passed to Lua and nothing else. The engine doesn't use them for the camera, the cursor or menus. Touches always say device 1.
- **Basic gyro.** It is either on or off. There is no button to hold or ratchet gyro, no calibration, and no gyro in menus. Lua can't read it.
- **No rumble.** There are no rumble, haptics or light bar calls anywhere.
- **Limited button prompts.**
  - The prompts recognize only Xbox, PlayStation and Switch Pro pads. Everything else gets the generic set, whose files are named `omw_steam_*`. That is a fallback, not detection of a Steam controller.
  - Paddles, misc, touchpad and guide have no icons.
  - The button hint bar has 15 fixed slots, with none for paddles or trackpads.
- **No tests** for input or controllers.

## The 2026 Steam Controller

The controller has:

- two sticks, with capacitive touch,
- two pressure-sensitive trackpads,
- a 6-axis gyro,
- four back buttons,
- grip sensors,
- the usual face, shoulder, trigger and d-pad buttons, plus a quick access button.

Sources: [Wikipedia](https://en.wikipedia.org/wiki/Steam_Controller_(2026)), [PC Gamer](https://www.pcgamer.com/hardware/game-pads/steam-controller-2026-review/).

### SDL support

- **SDL3 has a dedicated driver** for it, `SDL_hidapi_steam_triton.c`, which exposes the paddles, trackpads, gyro and touch sensors. Complete support landed in SDL 3.4.10 ([SDL issue #15471](https://github.com/libsdl-org/SDL/issues/15471), [Phoronix](https://www.phoronix.com/news/New-Steam-Controller-SDL)).
- **SDL2 doesn't know the controller.** The SDL2 2.32 that OpenMW bundles on Windows doesn't list its USB IDs ([moonlight-qt PR](https://github.com/Nonary/moonlight-qt/pull/23)). Without help, OpenMW would log it as an unusable controller and ignore it.

### What that means for OpenMW today

- **Through Steam Input:** with Steam running, Steam presents the controller to the game as a standard virtual gamepad. The game sees standard buttons, sticks and triggers. Steam maps the paddles, trackpads and gyro onto those buttons, or onto the mouse and keyboard, according to the player's Steam layout. This works now, with no engine changes.
- **Natively, without Steam:** OpenMW would need SDL3 3.4.10 or later. There are two ways to get it:
  - Replace `SDL2.dll` with `sdl2-compat`, which implements the SDL2 API on top of SDL3. The SDL2 API already has names for paddles, touchpads and controller sensors, so OpenMW should see them. OpenMW's input code already has a workaround for running on `sdl2-compat`. This wasn't tested, because it needs new binaries that this project can't download.
  - Port OpenMW from SDL2 to SDL3. This is a large change.

Even with the device visible natively, the extra inputs need engine work before players get much from them:

- the paddles can already be bound to actions, but have no button prompts,
- the trackpads reach only Lua,
- gyro has no way to toggle it with a button.

## What this means for the UI rework

- **Design for a standard gamepad first:** A/B/X/Y, two sticks, d-pad, two shoulders and two triggers. That works with every pad, through Steam Input with the Steam Controller, and on the Steam Deck. Treat paddles, trackpads and gyro as optional extras for later.
- **Build a shared focus and navigation model for the menus first.** It replaces each window handling buttons itself, the behavior of swallowing unhandled buttons, and the many controller menus checks. This is the foundation the other screens in [../screens.md](../screens.md) need.
- **Make button prompts depend on the controller type,** with room for more buttons than the current 15 fixed slots.
