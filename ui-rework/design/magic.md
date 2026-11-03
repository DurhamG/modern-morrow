# Magic screen

This is the design for the reworked magic screen, agreed with the user. It replaces the magic menu in controller menus mode. Unlike the inventory, it was built without mock-ups, because it reuses the inventory screen's look.

## What players use the magic screen for

1. **Readying a spell or item to cast.** Only one spell, power, magic item or scroll is ready at a time. Casting happens in the game, not in the menu.
2. **Switching between a few favourite spells,** such as a heal and an attack.
3. **Reading what a spell does.** Each spell has one or more effects. Each effect has a range, a strength, a duration and sometimes an area.
4. **Checking the effects on the character,** such as potions, spells, abilities, diseases and worn magic gear.
5. **Deleting spells they no longer want.** Spells from the character's race or birthsign, and powers, can't be deleted.

## What there is to list

- **Spells** cost magicka, and have a chance to succeed that depends on the character's skill in the spell's school.
- **Powers** are free, always succeed, and can be cast once a day.
- **Magic items** cast an enchantment when used. Each has a charge, and readying one equips it.
- **Scrolls** cast once and are used up.
- **Active effects** come from spells, potions, abilities, diseases, curses and worn gear with constant effects.

A warrior might know 10 to 30 spells, while a dedicated mage can know 100 or more.

## Decisions

- **Tabs, like the inventory.** The tabs are Recent, Active, Spells, Powers, Items and Scrolls. L1 and R1 switch them.
- **The Active tab is the first one shown.** It shows the character's current state, as the Equipment tab does in the inventory.
  - The left side lists the active effects. Each entry shows the effect and its strength, where it came from, and the time left. Effects that run out come first, soonest first.
  - The right side shows the spell or item that is ready to cast, with its cost and effects.
  - Below that, a short list of recently used spells. Picking one readies it. The one that was ready moves to the top of the list, so switching back takes one press.
- **Recent** lists the spells, powers, items and scrolls that the player recently readied or cast, newest first, up to 40. Readying one in any menu counts, and so does casting it in the game. A scroll that's used up, or an item that's no longer carried, drops off the list. The list is saved with the character.
- **Fat entries.** Spell effects don't fit in columns, so each entry has two lines: the name with the cost and chance, or the charge, then the first effect written out. A "+2" shows when there are more effects.
- **A details panel** writes out every effect in full, with the cost and chance, or the charge and casts left.
- **Groups.** Spells are grouped by school, such as Destruction, the way consumables are grouped by effect. Magic items and scrolls are grouped by the range of their first effect: Self, Touch or Target. Entries within a group are in name order.
- **Y jumps to the next group,** and back to the first one after the last, so long lists are quick to get through.
- **Full screen** below the tab bar, as the map and inventory are.

## Controls

| Button | What it does |
| --- | --- |
| L1, R1 | Switch tabs. |
| D-pad, left stick | Move the selection. Holding repeats. On the Active tab, left and right switch between the effects and the recent spells. |
| A | Ready the highlighted spell or item. |
| X | Delete the highlighted spell, after asking. |
| Y | Jump to the next group, such as the next school of spells. |
| B | Close the menu. |
| L2, R2 | Switch between the map, inventory, magic and stats screens. |

## The first version

`apps/openmw/mwgui/magicscreen.cpp` implements the screen. It shares its base class, sizes and drawing code with the inventory screen, in `controllerscreen.cpp`. The `magic-screen` scenario walks through it.

### Not done yet

- Mouse input. The screen only responds to a gamepad.
- The in-game "next spell" and "previous spell" keys still step through the full list. They could step through the recent spells instead. This was deferred until the menus are done.
