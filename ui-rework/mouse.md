# Mouse support for the controller menus

The controller menus work with a mouse, as well as with a controller and the keyboard. With controller menus off, mouse players keep OpenMW's normal interface.

## What the mouse does

- **Pointing** at a row, tile, key or option highlights it, as moving the d-pad onto it does. Only a mouse that moves points at things, so a resting pointer doesn't fight the d-pad.
- **Left-clicking** it chooses it, as A does.
- **Right-clicking** anywhere goes back, as B does.
- **The wheel** works as the d-pad's up and down: it moves through lists, and zooms the map. Over a page of text, such as a book, a quest's entries or a conversation, it scrolls the text instead.
- **Clicking a tab** switches to it, on the top bar and on a screen's own tabs. Clicking a shoulder button's glyph presses it.
- **Clicking a button hint**, such as "X Drop" or "Y Take All", presses that button. The d-pad's and the sticks' hints stand for directions, so they aren't buttons.
- **On the 3D map**, dragging pans the view, and a click places a pin, or removes the pin under the pointer.
- **Sliders** in the options and the effect editor take a click on their bar. The rest dialog's hours and the count dialog have arrows to click.

## How it is built

The controller menus draw everything in code, with `ControllerUi::addRect`, `addText` and the other helpers in controllerscreen.cpp, and draw everything again whenever anything changes. The mouse support sits on top of that.

- **Pointer targets.** `ControllerUi::addPointerTarget` makes an invisible widget over an area of a panel, holding what pointing at it and clicking it do. Screens add one over each thing the d-pad can reach while they draw it. The shared helpers add their own: `drawHints` for each hint, and `drawTabRow` for each tab.
- **Finding the target under the mouse.** The mouse manager tells `ControllerUi` when the mouse moves and when the left button goes down. `ControllerUi` finds the target with MyGUI's `LayerManager::getWidgetFromPoint`, rather than through MyGUI's mouse focus. MyGUI keeps the mouse within a modal window, and the controller menus draw a modal window's panel apart from the window, so MyGUI's focus never reaches it.
- **Dialogs keep the mouse to themselves.** A panel on the ControllerModal layer, where the dialogs draw, gets a target with no actions under its own, covering the screen. So clicks don't reach the screen behind a dialog.
- **Actions run on the next frame.** A target's actions usually draw the panel again, which destroys the target's widget. So they are queued, and the window manager runs them at the start of its next update.
- **The right button and the wheel** go through `MWBase::InputManager::pressControllerMenuButton`, which sends a button to the active menu, as a controller does. The button also counts as held for one frame, for the menus that read the held d-pad each frame. `ControllerUi::addWheelTarget` makes an area that takes the wheel for itself, such as a page of text.
- **`ControllerList`** is the shared scrolling list. It owns the focus and the scroll position, keeps the focused row in view, never scrolls past the end, moves with the held d-pad, and registers each visible row as a pointer target. `setScroll` puts a row at the top, for "Y: next group". The screens with scrolling lists use it in place of their own focus and scrolling code. The screens draw each row themselves, so they look as before.
- **The keyboard** goes through `ControllerManager::keyToMenuButton`, which turns a key into the controller button it stands for. The README lists the keys.

## What the mouse can't do yet

- Bartering one item at a time, and haggling, use the d-pad and the right stick. They would need new controls, such as plus and minus buttons.
- Turning the race preview's head uses the right stick. It would need dragging.
- The automation harness's wheel and text input reach the game wrongly through sdl2-compat, so the harness can't test the wheel or typing.
