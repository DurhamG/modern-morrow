# Touch controls

This is the design for playing on a phone or tablet without a gamepad, agreed with the user, and how it is built. It is for Android. The desktop and Steam Deck builds keep working exactly as they do now.

## Principles

- **No buttons on screen during play.** The whole screen is the game. Icons only appear while the player picks a mode or a quick key.
- **Two thumbs, like two sticks.** The left half of the screen moves and strafes, and the right half looks and acts, as a gamepad's left and right sticks and triggers do. Swipes in from the side edges open the inventory and pick the mode. Three corners have spots of their own.
- **Every gesture does something the game can already do.** The gestures press the same actions as the gamepad, so the game's rules don't change.

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
- **Turning** goes 1.5° for each millimetre the right thumb drags, before the camera sensitivity setting, within 10 mm to either side of where it went down. Past that edge the camera keeps turning that way for as long as the thumb stays out, starting from no speed at the edge, so there's no jump between the two, and rising evenly to 130° a second 20 mm past it. Coming back inside stops it. Looking up and down is always the mouse-style 1.5° a millimetre.
- **The corners** are about 12 mm squares. A touch that starts in one belongs to it, not to the left or right half.
- **Android's back gesture** is also a swipe in from a side edge. The app keeps the middle half of each side edge from it, so the edge swipes there are the game's, and the ends of the edges still go back.

## How it is built

- **The engine reads the fingers.** SDL sends multi-touch finger events, which `SDLUtil::InputWrapper` passes to a `SDLUtil::TouchListener`. `MWInput::TouchManager` is that listener. It recognises the gestures and acts through the paths the other input already uses:
  - Walking and attacking hold actions at a value, as a bound key or stick would. `BindingsManager::setTouchActionValue` holds `A_MoveForwardBackward`, `A_MoveLeftRight` and `A_Use`, and the game's Lua reads them as it reads a stick, so how far the thumb drags picks walking or running.
  - Single actions, such as jumping, activating, sneaking, the inventory, readying a weapon or spell, quick keys, quicksave and quickload, run with `InputManager::executeAction`, as their keys do.
  - Looking turns the player directly, as the mouse does, with the camera sensitivity setting applied.
- **SDL's touch-as-mouse is dropped** while touch controls are on, so that a tap during play doesn't also click, and a left click doesn't attack. The menus get no mouse cursor either, and don't take touch yet.
- **The mode picker, the radial and the quickload panel** are drawn by `MWGui::TouchOverlay` on the ControllerNotification layer, with the controller UI's helpers.
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
- Touch in the menus.
- The tutorial's messages still talk about thumbsticks and buttons. That's left for later.
