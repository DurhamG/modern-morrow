# Loot window

This is the design for the reworked loot window, agreed with the user. It shows what a container, a body or an NPC's pockets hold, in controller menus mode. It reuses the inventory screen's look, so it was built without mock-ups.

## The problems with the current window

- The player's inventory shows as a full grid of icons beside the container, though looting rarely needs it.
- The character portrait takes room and isn't needed.
- Items are icons, so the player hovers over each one to learn what it is.
- Nothing compares an item with what the player wears.
- The window is partly see-through and sized by the GUI scaling factor.

## Decisions

- **Looting is immersive,** so the loot window is a panel over the world, not a full screen. It sits on the right side of the screen, nearly full height, so the body or chest stays in view on the left. It's opaque, and the HUD hides while it is open.
- **The container comes first.** The selection starts on the container's items. The player's own inventory doesn't show until they ask for it, since putting items into a container is rare.
- **Your items.** The left stick press swaps the list for the player's own items, under the inventory screen's tabs, and the header changes to "Put in" and the container's name. A puts the highlighted item in, taking it off first if it's worn. There is no "put all", since emptying the inventory by accident would be painful. B or the left stick press goes back. The game doesn't let the player put items into an NPC's pockets, so pickpocketing has no Your items view.
- **Fat entries,** since loot varies too much for columns. Each entry shows the name, what the item is with its main stat, such as "Long Blade, 1-20", and the weight.
- **Groups.** Gold comes first. The rest is grouped by kind: weapons, armour, clothing, consumables, books, tools and misc. The most valuable items come first in each group. Group headers only show when there's more than one group.
- **Upgrades are marked.** Armour or weapons that beat what the player wears in that slot are marked "Better", in green.
- **A details panel** shows the highlighted item in full, beside the equipped items it would replace. Rings and other items that fit two slots are compared with both.
- **Carrying weight.** The header shows the player's encumbrance. An item's weight is red when taking it would go over the limit.
- **Stealing is spelled out.** On an owned container, the header names the owner and warns that taking is stealing. Every entry is marked Stealing, and the button hints say Steal. Taking everything asks first. Pickpocketing has its own warning.
- **No stack splitting.** A takes the whole stack.
- **The window closes on its own** after the last item is taken.

## Controls

| Button | What it does |
| --- | --- |
| D-pad, left stick | Move the selection. Holding repeats. |
| A | Take the highlighted item. |
| Y | Take everything. On an owned container, it asks first. |
| X | Take the item and equip it. Only offered when it's clear where it goes, so not for a ring when both hands wear one. |
| View | Dispose of the corpse, on bodies. |
| Left stick press | Switch to your own items, to put one in, and back. |
| L1, R1 | In your items, switch tabs. |
| B | Close. |

## The first version

`apps/openmw/mwgui/lootpanel.cpp` draws the panel. The container window still owns the container's items, and hides itself while the panel reads them. `apps/openmw/mwgui/itemdetails.cpp` holds the item descriptions, stats and comparisons the inventory screen and the panel share. The `controller-loot` scenario loots Fargoth one item at a time. The `controller-loot-chest` scenario fills a chest with mixed items, puts one of the player's own items in, then opens a crate Arrille owns.

### Not done yet

- Mouse input. The panel only responds to a gamepad.
