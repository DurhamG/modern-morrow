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
- **Tabs** replace the filters. There are six: Recent, Equipment, Consumables, Notes, Tools and Misc. There is no All tab.
  - **Misc** holds whatever doesn't fit elsewhere, such as soul gems, keys and clutter. It gets split into better groups as they become clear.
  - There is no Quest tab for now. Marking quest items needs an analysis of which items the quests refer to, and a quest journal elsewhere may show them better.
- **Recent** lists everything that has entered the inventory, newest first, up to a fixed limit such as 40 items. It doesn't clear items once they've been seen. Recording when each item arrived needs a save format change, which is acceptable.
- **"New until seen"** markers are for other places, such as an equipment area with new items that fit it.
- **Equipment** shows the body areas, each with its top-most equipped item. That is the armour piece if there is one, otherwise the clothing. Areas with new items that fit have a marker.
  - Selecting an area shows its slots on the left. Shoulders, for example, has a left pauldron and a right pauldron. Chest has a cuirass, a shirt and a robe. The right side lists the items that fit the area.
  - Choosing an item moves the selection to the slots, to pick which one it replaces.
  - Choosing a slot first narrows the list to the items that fit that slot.
- **Lists, not grids.** Each row shows the item's name and the stats that matter in that view, so no hovering is needed. For example, weapons show damage. The rows lay their stats out across the row, so the list stays dense. Lists scroll vertically.
- **A details panel** shows the highlighted item in full, next to the equipped item it would replace, with the differences marked.
- **Sorting.** One button cycles the sort order.
- **Row markers** show equipped, new and stolen items. They also show "better than equipped", where a clear number such as the armour rating decides it.
- **Consumables** are grouped by effect, so all Restore Health potions sit together.
- **The details panel says what the main button does** for the highlighted item, such as Equip, Drink, Read or Use.

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

## Open questions

- What the rule for "top-most" is when a robe is worn over a cuirass.
- Which ring a new ring replaces, when both hands are full.
- Whether the inventory should be sized relative to the screen height, as the HUD is, instead of following the GUI scaling factor. The mock-ups do this.
