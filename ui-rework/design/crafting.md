# Crafting

This covers the crafting screens in controller menus mode. The user asked for them to be designed and built without a review first, following the patterns of the other screens, and will give feedback after playing with them. They are being done one at a time. So far only alchemy is done.

## Alchemy

### How alchemy works

- The player uses a mortar and pestle from the inventory. The best tool of each kind the player carries goes in its slot by itself: the mortar and pestle, which every potion needs, and the alembic, calcinator and retort, which make potions stronger or weaken their bad effects.
- A potion takes two to four ingredients. Each ingredient has up to four effects, and the player only knows the first few, depending on their Alchemy skill. The potion gets every effect that at least two of its ingredients share.
- The potion is named after its first effect, and the player can rename it.
- OpenMW can brew several potions at once, up to the smallest ingredient stack. Each one can fail, which uses up the ingredients.

### Decisions

- **A full screen, like the inventory.** The ingredients are on the left and the potion on the right. The old window stays hidden and keeps the potion and the rules.
- **The ingredient list says what each ingredient does.** Each row shows the ingredient's name, how many the player carries, and the effects the player knows, so they never need a tooltip.
- **A puts the highlighted ingredient in the potion, or takes it out.** The old window's B removed the last ingredient. Now B just leaves, like on every other screen.
- **The list helps find a match.** Once the potion holds an ingredient, the list splits into three groups: In the potion, Shares an effect, and Other ingredients. An ingredient shares an effect when one of its known effects is also known on an ingredient in the potion. Effects the player doesn't know yet don't count, so the list never gives them away. The highlight stays on the same ingredient when it moves between groups.
- **The potion panel** shows the potion's name, its four ingredient slots, the effects it will have, the tools in use, and how many potions to brew. It says what to do first when the potion is empty, and warns in red when the ingredients share no effect the player knows. Unknown effects show as unknown, as in the old window.
- **X brews, and L1 and R1 change how many.** The count can't go above what the ingredients allow.
- **Clicking the left stick renames the potion,** with the on-screen keyboard from character creation. Most players can keep the default name.
- **The tools can't be swapped.** The best ones are always used, as the old window chooses them. Picking a worse tool on purpose is rare.

## Not done yet

- Enchanting, spellmaking, recharging and repairing with your own tools still use the old windows.
- Mouse input. The screen only responds to a gamepad, like the other screens.
