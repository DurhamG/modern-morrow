# Quick keys

This is the design for quick keys in controller menus mode, agreed with the user. It replaces the quick keys menu for controller players. The menu itself is unchanged, and keyboard players still open it with F1.

## How quick keys worked before

- There are ten slots. Slots 1 to 9 hold an item, a spell or an enchanted item. Slot 10 is always hand-to-hand.
- The quick keys menu shows the ten slots. Picking one opens a small dialog with Item, Magic, Delete and Cancel. Item and Magic each open another list to pick from.
- The number keys 1 to 9 and 0 use the slots during play.
- A controller had no button for the menu or for any slot, and the HUD never showed what was on each slot.

## Decisions

- **The d-pad uses the quick keys during play.** It did nothing during play before.
  - Pressing up, right, down or left uses slots 1 to 4.
  - Holding the same directions for a third of a second uses slots 5 to 8.
  - When a direction has nothing on its hold, its press acts as soon as the button goes down. Otherwise the press acts when the button comes back up, because until then it could still become a hold.
  - Slot 9 and hand-to-hand stay on the keyboard only. X already readies and puts away the weapon.
  - A d-pad direction the player bound to an action of their own keeps doing that action.
- **Assigning happens on the inventory and magic screens, not in a menu of its own.**
  - The d-pad moves the selection on those screens, and the triggers switch between the menus. So clicking the left stick starts assigning. The left stick click did nothing on those screens.
  - The details panel then shows the eight d-pad slots, with what is on each.
  - Pressing a direction puts the highlighted item or spell on that direction's press slot. Holding the direction puts it on the hold slot. So the player assigns a slot with the same gesture that uses it in play.
  - Choosing the slot the item or spell is already on takes it off. Choosing a slot that holds something else replaces it.
  - An item or spell is on one slot at most. Putting it on another slot moves it.
  - B or another left stick click leaves without changing anything.
- **What can go on a quick key.** On the inventory screen, anything the screen can use, equip or read, except books and notes without an enchantment. On the magic screen, spells, powers, magic items and scrolls. Magic items and scrolls from the magic screen go on as their enchantment, so the quick key readies it. The same item from the inventory screen is used or equipped instead, as in the old menu.
- **A marker shows what is on a quick key.** A small d-pad sits under the item's name in the inventory, and at the right of a spell's second line on the magic screen. Its lit arm points the slot's way. A short arm means the press slot, and a long arm the hold slot.
- **The HUD shows what was used.** After a quick key is used, its icon and name show above the weapon and spell boxes for two seconds, then fade. Drinking a potion or reading a scroll gave no sign of what happened before, and the holds are easy to forget.
- **Saves don't change.** The d-pad slots are the same slots 1 to 8 that the number keys use, so keyboard and controller share them, and quick keys from older saves carry over.

## Other games

- Skyrim on consoles is the closest. Players mark favourites, and in the favourites list press d-pad left or right on one to put it on that direction.
- Fallout 4 puts several items on each d-pad direction, and pressing the same direction again moves to the next.
- Dark Souls and Elden Ring cycle through short lists on the d-pad, and a separate button uses the current item.
- The Witcher 3 and Baldur's Gate 3 on consoles hold a button to open a wheel.

Assigning straight to the d-pad is the quickest in a fight: one press, with no cycling and no wheel.

## Not done yet

- Mouse input. The assigning only responds to a gamepad, like the rest of the screens.
- The user hasn't reviewed this design in the game yet.
