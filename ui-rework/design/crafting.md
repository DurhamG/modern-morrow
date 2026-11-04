# Crafting

This covers the crafting screens in controller menus mode. The user asked for them to be designed and built without a review first, following the patterns of the other screens, and will give feedback after playing with them. They were done one at a time: alchemy, enchanting, spellmaking, then repairing and recharging.

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

## Enchanting

### How enchanting works

- The player enchants an item themselves by using a soul gem that holds a soul, or pays an enchanter to do it. Enchanting yourself can fail, which uses up the soul. Paying can't fail.
- An enchantment needs an item, a soul gem with a soul, and one to eight effects. The effects come from the spells the player knows, as long as the effect allows enchanting.
- Each effect has a range (self, touch or target), a magnitude from a minimum to a maximum, a duration and an area. Some effects work on one skill or attribute, such as Fortify Attribute.
- How the enchantment casts depends on the item: once for a scroll, when it strikes or when used for a weapon, and when used or all the time for clothing and armour. Casting all the time needs a strong soul, and limits the effects to Self.
- The item holds a limited amount of enchantment, and each effect uses some of it. The soul sets how much charge the item gets.

### Decisions

- **A full screen, with what is being made on the right and a list to pick from on the left.** The right side lists the item, the soul gem, the cast type, the effects, and a line to add an effect. The numbers are at the bottom: the enchantment used out of what the item holds, the cost to cast, the charge, and the price or the chance of success.
- **A on a line on the right fills the list on the left** with what can go there: the items that can be enchanted, with how much they hold; the soul gems with souls, with the creature and its strength; or the effects the player knows, grouped by school, with the ranges each allows. Choosing one returns to the right side, on the next thing still missing. Left and right on the d-pad also move between the two sides.
- **The highlight starts on the first thing still missing.** Enchanting with a soul gem starts on the item, because the gem is already chosen. An enchanter starts on the item too.
- **An effect on a skill or an attribute asks which one first,** in the same list, in place of the old window's separate dialog.
- **Effects are edited on the left side, in place of the list.** The lines are the range, the magnitude's minimum and maximum, the duration and the area, each with its value and a bar. Left and right change a value, faster the longer they are held. L1 and R1 change it by 10. Raising the minimum above the maximum raises the maximum too, as in the old window. A finishes, X deletes an effect that was already added, and B cancels. The numbers on the right follow every change, so the player sees the cost while they change the magnitude.
- **The game's own help text explains each part.** The game already has a sentence for each line on the right and each setting of an effect. The screen shows the one for the highlighted line, so a player new to enchanting can learn it on the screen.
- **The name comes from the item,** as in the old window. Clicking the left stick renames it with the on-screen keyboard.
- **The chance of success shows only when the "show enchant chance" setting is on,** as in the old window. It is off by default.
- **The enchanting window stays hidden and still makes the enchantment.** Its checks and messages are unchanged, such as for too many effects, a soul gem without a soul, or an item stolen from the enchanter. The effect dialog stays hidden too, and hands the screen its buttons while an effect is edited.
- **The effect editor is shared with spellmaking,** which edits effects the same way.

## Spellmaking

### How spellmaking works

- A spellmaker NPC sells new spells made from effects the player already knows from their spells, as long as the effect allows spellmaking.
- A spell has one to eight effects, set the same way as in enchanting. The window works out the magicka cost, the chance to cast it, and the price.
- The old window needed the player to type a name before buying.

### Decisions

- **The same screen as enchanting, without the item, soul gem and cast type.** The right side lists the effects and a line to add one, with the magicka cost, the chance to cast and the price at the bottom. The left side lists the effects the player knows, grouped by school, and edits an effect in place of the list.
- **A new spell is named after its first effect,** such as "Fire Damage" or "Fortify Strength", so it can be bought without typing a name. Clicking the left stick renames it.
- **The spellmaking window stays hidden and still makes the spell.** Its checks and messages are unchanged, such as for a spell without effects or not enough gold.

## Repairing and recharging

### How they work

- The player repairs weapons and armour with their own repair tools, such as hammers and prongs, by using one from the inventory. Each use can fail, wears the tool down, and repairs more with a better tool. The Armorer skill, Strength and Luck decide whether it works.
- The player recharges a magic item by using a soul gem with a soul and choosing to recharge. Each try uses up the gem. The Enchant skill, Intelligence and Luck decide whether it works and how much charge comes back.

### Decisions

- **One full screen for both.** The items that need it are on the left, worst first, each with its condition or charge as numbers and a bar. The tool is on the right, with what is left of it: a repair tool's uses and quality, or a soul gem's soul and its strength. Below it, a sentence says what decides whether a use works, and the highlighted item shows larger.
- **A uses the tool on the highlighted item.** The game's own messages say whether it worked.
- **Y switches to the next tool of the same kind,** in place of the old window's separate picker.
- **When a tool runs out, the next one takes its place,** so the player can keep going without picking again.
- **The repair and recharge windows stay hidden and still do the work.**

## Not done yet

- Mouse input. The screen only responds to a gamepad, like the other screens.
