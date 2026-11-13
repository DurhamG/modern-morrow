# Touch controls

This is the design for playing on a phone or tablet without a gamepad, agreed with the user, and how it is built. It is for Android. The desktop and Steam Deck builds keep working exactly as they do now.

## Principles

- **No buttons on screen during play.** The whole screen is the game. Icons only appear while the player picks a mode or a quick key.
- **Two thumbs, like two sticks.** The left half of the screen moves and strafes, and the right half looks and acts, as a gamepad's left and right sticks and triggers do. Swipes in from the side edges open the inventory and pick the mode. Three corners have spots of their own.
- **Every gesture does something the game can already do.** The gestures press the same actions as the gamepad, so the game's rules don't change.
- **Menus are tapped.** The controller menus already work with a mouse, as [mouse.md](../mouse.md) describes, so a tap in a menu acts as a click.

## During play

| Gesture | Does |
|---|---|
| Left thumb drags | Moves, like a stick: up the screen is forward, down is back, and sideways strafes. The farther the drag, the faster, from walking to running. |
| Left thumb taps | Nothing, for now. |
| Right thumb drags | Looks around, as the mouse does, within a circle around where the thumb went down. Held past the circle's edge to either side, it keeps turning that way, like a stick. |
| Right thumb taps | Acts in the current mode: uses what's in front of you in use mode (A), attacks in attack mode, casts in magic mode, jumps in jump mode. |
| Right thumb holds still | In attack mode, charges the attack, such as drawing a bow. The thumb can then drag to aim while it charges, and lifting releases it. Does nothing in the other modes. |
| Swipe in from the left edge | Opens the inventory screens (B). |
| Swipe in from the right edge | Opens the mode picker under the thumb. |
| Bottom-right corner taps | Sneaks, on and off. |
| Top-left corner taps | Quicksaves. Modern Morrow keeps every quicksave, so a stray tap does no harm. |
| Top-right corner holds | Quickloads, after about a second. A panel with a filling bar shows while it waits, and lifting early cancels it. |

Jumping is a mode of its own, so a jump in the middle of a fight means switching modes first. Morrowind isn't a game of quick reactions, so that's accepted for touch.

### Modes

- There are four modes: **use**, **attack**, **magic** and **jump**. Attack has the weapon or fists out, magic has the spell readied, and use has both put away. Jump leaves whatever is out as it is.
- **Use, attack and magic are the game's stance.** They aren't kept separately. Equipping a weapon, or casting from the magic screen, changes the stance, and the mode follows. Choosing one of them sends ToggleWeapon (X) or ToggleSpell (Y), as the gamepad does, and leaves jump mode. Jump mode is kept by the touch controls.
- In first person, the drawn weapon, raised fists or glowing hands show the mode. Whether that's clear enough needs checking on the phone.

### The mode picker

- Swiping in from the right edge opens the picker as half a plus around the thumb: **use** under the thumb, **attack** to its left with **quick keys** past it, **magic** above and **jump** below. The current mode is underlined.
- Sliding the thumb highlights the item nearest to where it has moved since the picker opened, and lifting the thumb picks it. Lifting on the current mode changes nothing.
- The game pauses while the picker is open.

### Quick keys

- Sliding past attack to quick keys and lifting opens a radial of the quick keys around the picker. The game stays paused.
- The radial holds the eight d-pad slots from [quick-keys.md](quick-keys.md), in the d-pad's own directions. The four press slots make an inner ring and the four hold slots an outer ring, like the marker's short and long arms.
- Tapping a slot uses it, as the d-pad would. Tapping outside the radial closes it without using anything.
- Assigning slots stays on the inventory and magic screens. It only works with a gamepad today, so it needs tappable controls, which the mouse is missing too.

### Telling gestures apart

These are starting values to tune on the phone. Distances are in millimetres on the glass, so the Fold's two screens and other phones feel the same.

- **A tap** lifts within 250 ms and moves less than 3 mm.
- **A drag** is anything that moves farther.
- **The attack's charge** starts after the right thumb stays still for 180 ms in attack mode. A quick tap sends the press and release together, which gives a full swing because Modern Morrow always uses the best attack. A thumb that moves before 180 ms only looks, and lifting it does nothing. In Morrowind an attack can't be cancelled once wound up, so this delay is what keeps looking from swinging.
- **Movement** has a 2 mm dead zone around where the thumb went down, and reaches full speed 15 mm away in any direction, as a stick's circle does.
- **An edge swipe** starts within 5 mm of a side edge, in the middle half of the screen's height, and moves 8 mm inwards, more in than up or down. A thumb that goes down there but moves up or down first, or lifts, does what it would have anywhere else on its half.
- **Pinches are only for the map.** Two fingers that touch down there within 150 ms of each other zoom it.
- **Turning** goes 1.5° for each millimetre the right thumb drags, before the camera sensitivity setting, within 10 mm to either side of where it went down. Past that edge the camera keeps turning that way for as long as the thumb stays out, starting from no speed at the edge, so there's no jump between the two, and rising evenly to 130° a second 20 mm past it. Coming back inside stops it. Looking up and down is always the mouse-style 1.5° a millimetre.
- **The corners** are about 12 mm squares. A touch that starts in one belongs to it, not to the left or right half.
- **Android's back gesture** is also a swipe in from a side edge. The app keeps the middle half of each side edge from it, so the edge swipes there are the game's, and the ends of the edges still go back.

## In menus

- **A tap is a click**: it presses a hint, switches a tab, or presses a dialog's button, through the mouse support.
- **A tap on a list row selects it first.** Touch has nothing to point with before it taps, and a click on a row does what A does, such as eating an ingredient. So the first tap on a row selects it and shows its details, and a tap on the selected row acts. Rows of other kinds, such as the equipment slots, still act on the first tap.
- **Swiping in from either side edge is back (B)**, in the same middle half of the edges as during play.
- **The map** pans when dragged and zooms when pinched, as a phone's maps do. The zoom keeps the ground between the fingers still. A tap places a pin, or removes the one under the finger, as a click does. The name of the door or pin under the crosshair shows, as with a gamepad, rather than the one under the finger, since a finger doesn't hover and covers what's under it.
- **A finger away from the rows, hints and scrolling areas works as the mouse's left button**: it presses when it starts to move, drags, and lets go when it lifts, and a tap clicks. That is what pans the map, and it works the same in the windows that aren't controller menus.
- **Tapping beside a conversation ends it**: a tap on the game beside the dialogue panel, or beside a service the conversation opened, such as barter, says goodbye, as long as the speaker isn't waiting for an answer. A tap on an empty part of a panel does nothing. `ControllerUi::pointerOnPanel` tells the two apart by what the panels draw.
- **Dragging sideways switches tabs**, if the drag is more sideways than up or down: one tab for every 12 mm, towards the side the finger moves, so dragging right goes to the tabs on the right. The tab follows the finger while it's down, so one long drag goes through several tabs, and pulling back undoes an overshoot. On the left half of the screen it switches the big tabs, as the triggers do, and on the right half the screen's own tabs, as the shoulder buttons do. There's no sliding animation, so the swipe moves the tab highlight rather than pulling a page. It doesn't happen on the map, which pans when dragged and whose tabs can be tapped, or on a slider.
- **Dragging up or down scrolls** lists, books, quests and dialogue topics, one row for each 6 mm dragged, the way the mouse wheel does. As with the wheel, the row under the finger takes the selection. A drag that starts on a link in a page, such as a topic in the journal, still scrolls the page.
- **Rest and Menu.** With touch controls on, the tab bar above the inventory screens ends with **Rest**, which opens the rest dialog, and **Menu**, which opens the pause menu. The tabs move left to make room. The journal is already a tab. The desktop and Steam Deck tab bars are unchanged.
- Three things a mouse can't do yet, from [mouse.md](../mouse.md), need touch controls too: bartering one item at a time, haggling, and turning the head on the race screen. Assigning quick keys needs them too.

## Tutorial

Two pages explain the touch controls, drawn over the game itself rather than over screenshots, so they fit any screen.

- **The play page** waits until the player can move and look, so a new game's opening scene, whose scripts and voices run on regardless of a pause, plays out first. It then shows over the paused game: the three corners, the swipes in from the left edge (the inventory) and the right edge (the mode picker, drawn small), and what each thumb does. A tap, or a gamepad's A or B, closes it. A menu that a script opens over it, such as a message, hides it until the menu closes.
- **The menus page** shows the next time the inventory screens open, which can be much later, since a new game's inventory stays locked until the tutorial dialogue gives it. It shows switching screens on the left half, tabs on the right half, going back from either edge, and that the button hints can be tapped. A tap, A or B closes it and leaves the inventory open.
- **When.** The play page shows on a new game, on loading a character that hasn't seen it, and on loading any game after 14 days without playing with touch. `touch-tutorial.txt` in the user data folder records the characters that have seen it, by their folder of saves, and when the game was last played with touch. A new game's character has no folder until it saves, so it's recorded then. With touch controls off, the tutorial never shows.

## How it is built

- **The engine reads the fingers.** SDL sends multi-touch finger events, which `SDLUtil::InputWrapper` passes to a `SDLUtil::TouchListener`. `MWInput::TouchManager` is that listener. It recognises the gestures and acts through the paths the other input already uses:
  - Walking and attacking hold actions at a value, as a bound key or stick would. `BindingsManager::setTouchActionValue` holds `A_MoveForwardBackward`, `A_MoveLeftRight` and `A_Use`, and the game's Lua reads them as it reads a stick, so how far the thumb drags picks walking or running.
  - Single actions, such as jumping, activating, sneaking, the inventory, readying a weapon or spell, quick keys, quicksave and quickload, run with `InputManager::executeAction`, as their keys do.
  - Looking turns the player directly, as the mouse does, with the camera sensitivity setting applied.
  - In menus, a tap goes to `ControllerUi::pointerTapped`, or to MyGUI as a click for the windows that aren't controller menus, such as the main menu's buttons. A drag turns the wheel through `ControllerUi::pointerWheel`.
- **SDL's touch-as-mouse is dropped** while touch controls are on, so that a tap during play doesn't also click, and a left click doesn't attack. The menus get no mouse cursor either.
- **The tutorial, the mode picker, the radial and the quickload panel** are drawn by `MWGui::TouchOverlay` on the ControllerNotification layer, with the controller UI's helpers.
- **A setting turns touch controls on**, `[Input] touch controls`. It's off in `settings-default.cfg`. The Android app ships `android/settings.cfg` next to `defaults.bin`, and the game reads it as more defaults, which turns it on there while a player's own `settings.cfg` can still turn it off. With it off, finger events are ignored as before, so the desktop and Steam Deck builds behave as they did.
- **Pausing** for the picker and the radial adds a "touch" pause tag to the time manager, the same way Lua's `world.pause` does.
- **The screen's edges.** `GameActivity` asks Android to keep the middle half of each side edge, 48 dp wide, free of the back gesture. Android normally allows only 200 dp of each edge, but lifts that for an immersive sticky window, which SDL's fullscreen window is. `dumpsys window` showed the whole middle half granted in the emulator (Android 15) and on the Fold 8 (Android 17).
- **Testing.** The harness's `touch` command sends finger events: a tap, a drag along a path, a pinch, or single fingers going down, moving and lifting. It works in the emulator, where every gesture above was tried. How it feels still needs the phone.

## Other games

- Most mobile shooters put a virtual stick on the left and buttons on the right. The user wants no buttons on screen.
- Infinity Blade and other swipe games act on gestures alone, but they don't have free movement.
- The radial is close to the weapon wheels in console games, opened by a gesture instead of a held button.

## Not done yet

- Trying it on the phone. The timings and distances above are starting values to tune there.
- Bartering one item at a time, haggling and turning the race preview's head, by touch.
- The menus show gamepad glyphs on their hints, which mean nothing to a player without a gamepad, though tapping them works.
- The tutorial's messages still talk about thumbsticks and buttons. That's left for later.
