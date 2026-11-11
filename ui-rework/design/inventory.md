# Inventory screen

This is the design for the reworked inventory screen, agreed with the user. It is a work in progress. The mock-ups in [../mockups/](../mockups/) show it.

## What players use the inventory for

1. **Changing gear.** This means equipping, unequipping and swapping weapons, armour, clothing and jewellery. Players also choose between several items of one kind, and swap gear for a situation, such as Speechcraft clothing before a conversation.
2. **Checking loot.** Players want to find what's new, compare it with what they wear, and decide what to keep.
3. **Managing weight.** Players find heavy items that are worth little, drop them, and split stacks.
4. **Using consumables.** That means drinking potions, eating ingredients, and finding the right potion among many.
5. **Reading** books, notes and letters. Skill books raise a skill on the first read.
6. **Using tools.** Lockpicks and probes go into the hand, repair tools open the repair menu, apparatus opens the alchemy menu, and torches go into the off hand.
7. **Managing magic items.** This covers scrolls, enchanted items and their charges, and filled soul gems.
8. **Finding a specific item,** such as a quest item, a key or an ingredient.
9. **Setting up for later.** That means quick keys, and dropping items in the world to decorate a house.
10. **Checking the character's** armour rating, gold and encumbrance.

## The problems with the current screen

- An item's name and stats only show in a tooltip, so players hover over each item.
- There is no sorting.
- The five filters are too broad. "Magic" mixes potions, scrolls and enchanted gear, and "Misc" holds everything else.
- Nothing compares an item with what is equipped.
- Nothing marks items as new, unread or stolen.
- What "use" does depends on the item type, and the screen never says.

## Decisions

- **No character portrait.** It takes too much room on a handheld. The armour rating, encumbrance and gold go in free space on the screen.
- **The screen fills everything below the top tab bar.** The bottom button bar is gone, as on the map. Button hints come back later, where they're needed.
- **Tabs** replace the filters. There are seven: Recent, Equipment, Consumables, Notes, Tools, Misc and Weight. Weight is the only tab that lists every item.
  - **Weight** lists every item, the heaviest stacks first, so the player can see what makes them weigh so much. Its columns are the count and the total weight of the stack, which is what it sorts by. One item's weight shows in the details panel. The other tabs show one item's weight, so a stack of 30 light items doesn't look heavy there. The loot and barter screens share the other tabs but have no Weight tab.
  - **Misc** holds whatever doesn't fit elsewhere. It is split into keys, soul gems, and other things, in that order, each under its own header.
  - **Notes** is split the same way into skill books, other books, and letters and notes.
  - There is no Quest tab for now. Marking quest items needs an analysis of which items the quests refer to, and a quest journal elsewhere may show them better.
- **Recent** lists everything that has entered the inventory, newest first, up to a fixed limit such as 40 items. It doesn't clear items once they've been seen. Recording when each item arrived needs a save format change, which is acceptable.
- **"New until seen"** markers are for other places, such as an equipment area with new items that fit it.
- **Equipment** shows the body areas, each with its top-most equipped item. That is the armour piece if there is one, otherwise the clothing. Areas with new items that fit have a marker.
  - Selecting an area shows its slots on the left. Shoulders, for example, has a left pauldron and a right pauldron. Chest has a cuirass, a shirt and a robe. The right side lists the items that fit the area.
  - Choosing an item moves the selection to the slots, to pick which one it replaces.
  - Choosing a slot first narrows the list to the items that fit that slot.
- **Lists, not grids.** Each row shows the item's name and the stats that matter in that view, so no hovering is needed. For example, weapons show damage. The rows lay their stats out across the row, so the list stays dense. Lists scroll vertically.
- **A details panel** shows the highlighted item in full, next to the equipped item it would replace, with the differences marked.
- **Sorting.** One button cycles the sort order through the list's columns, then the name. Each list starts sorted by its main stat: armor lists by armor rating, weapon and ammunition lists by damage, tools by quality, and everything else by value.
- **Armor ratings and damage in the lists are green or red** where they beat or fall short of what the item's slot holds now, so the player sees at a glance what to consider equipping.
- **Row markers** show equipped, new and stolen items. They also show "better than equipped", where a clear number such as the armour rating decides it.
- **Consumables** are grouped by effect, so all Restore Health potions sit together.
- **The details panel says what the main button does** for the highlighted item, such as Equip, Drink, Read or Use.
- **Books and notes can be read in place.** In the Notes tab, the list and the details panel share the width equally. The details panel shows the highlighted book's text on a page, laid out as the book window does, instead of its weight and value. The right stick scrolls the text, as sub-panels scroll in modern games.

## Equipment areas

There are twelve areas:

| Area | Slots |
| --- | --- |
| Head | Helmet |
| Shoulders | Left pauldron, right pauldron |
| Chest | Cuirass, shirt, robe |
| Arms | Left and right gauntlet, bracer or glove |
| Legs | Greaves, pants, skirt |
| Feet | Boots or shoes |
| Weapon | Right hand |
| Off hand | Shield or light |
| Ammunition | Arrows and bolts |
| Amulet | Amulet |
| Rings | Left ring, right ring |
| Belt | Belt |

## Implementation decisions

These were chosen when the first version was built, and can be revisited.

- **Top-most item in an area.** Armour comes first, then the outer clothing layer: robe over shirt, and skirt over pants.
- **Picking a slot.** If an item fits exactly one slot, choosing it equips it straight away. The slot step only appears when there's a real choice, such as a ring when both hands are full. If one ring hand is free, a new ring goes there without asking.
- **Recent and "new".** The player's save keeps a log of the items that entered the inventory, newest first. Each entry also records whether the player has seen it. An item counts as seen once it is highlighted in a list. "New" markers show unseen items, and the overview marks each area that has a new item that fits it. The Recent tab lists the 40 newest entries. Older entries stay in the log only while they are unseen, up to 200, so the first items of a big haul still show as new.
- **Scope.** The new screen replaces the inventory only in controller menus mode. Container, barter and companion screens, and the mouse-driven inventory, keep the old window for now.
- **Size.** The screen is sized relative to the screen height, as the HUD is, and ignores the GUI scaling factor.
- **Skill books.** Showing a book's text in the Notes tab doesn't count as reading it, so it doesn't raise a skill or run the game's reading logic. A still reads the book in the normal book window. For an unread skill book, the A hint says which skill it raises, such as "Read (+1 Enchant)".

## The first version

`apps/openmw/mwgui/inventoryscreen.cpp` implements the screen. It replaces the inventory window in the controller tab cycle, and the old window still handles using items, and the container and barter screens. The `inventory-screen` scenario walks through it.

### Controls

| Button | What it does |
| --- | --- |
| L1, R1 | Switch tabs. |
| D-pad, left stick | Move the selection. Holding repeats. In the equipment overview, left and right switch columns. In an area, left and right move between the slots and the item list. |
| A | Open an area, use or equip the highlighted item, or unequip it if it's equipped. On a slot, show only the items for that slot. While choosing a ring's hand, put the ring there. |
| B | Go back: cancel a ring choice, show every slot again, return to the overview, or close the inventory. |
| X | Drop the highlighted item, asking how many for a stack. On a slot, unequip it. |
| Y | Cycle the sort order through the list's columns, then the name. |
| Right stick | In the Notes tab, scroll the highlighted book's text. |
| L2, R2 | Switch between the map, inventory, magic and stats screens, as before. |

### Not done yet

- Mouse input. The screen only responds to a gamepad.
- Sorting the Recent tab, which always lists the newest items first.
