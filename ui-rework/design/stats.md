# Stats screen

This is the design for the reworked stats screen, agreed with the user. It replaces the stats menu in controller menus mode. It reuses the inventory and magic screens' look, so it was built without mock-ups.

## What players use the stats screen for

1. **Checking the character at a glance:** race, class, birthsign, level, health, magicka, fatigue and attributes.
2. **Planning level ups.** Every ten increases of major and minor skills earn a level. At each level up, the player picks three attributes to raise. Each one rises by a bonus of up to five points, which grows with how often the player raised the skills it governs. The normal menu only shows this in a tooltip on the level.
3. **Checking skills,** and how close each one is to its next increase.
4. **Checking factions:** the player's rank in each, and what the next rank needs.
5. **Checking reputation and bounty.**

## Decisions

- **Three tabs:** Character, Skills and Factions. L1 and R1 switch them. The screen opens on Character.
- **The Character tab is a set of tiles, not a list.**
  - The character's name heads the tab.
  - Tiles for race, class and birthsign come first, then a wide level tile with a bar of progress to the next level.
  - Health, magicka and fatigue have bars in the HUD's colours.
  - The eight attributes are tiles in two rows of four, with large numbers. A tile shows its level-up bonus, such as "x3", when it is more than one point.
  - The d-pad moves to the nearest tile in each direction.
- **Every tile and entry can be selected,** and the details panel on the right describes it:
  - race and birthsign: the description, and the abilities and powers they give,
  - class: the specialization, favoured attributes, and major and minor skills,
  - level: progress to the next level, and every attribute's bonus at the next level up, with how levelling works,
  - attributes: the base and current values, the bonus at the next level, how often its skills were raised, and the skills it governs,
  - skills: the value, the progress to the next increase, whether raising it counts toward the next level, and which attribute's bonus it adds to,
  - factions: the rank, and what the next rank needs, in green where the player meets it and red where they don't.
- **Raised and lowered stats are green and red,** such as Strength under a Fortify Strength potion.
- **The Skills tab** groups skills into major, minor and misc. Each entry shows the governing attribute and a bar of progress to the next increase.
- **The Factions tab** lists reputation and bounty under Standing, then the factions with their ranks.
- **Y jumps to the next group** on the Skills and Factions tabs, as on the magic screen.

## Controls

| Button | What it does |
| --- | --- |
| L1, R1 | Switch tabs. |
| D-pad, left stick | Move the selection. Holding repeats. |
| Y | Jump to the next group of a list. |
| B | Close the menu. |
| L2, R2 | Switch between the map, inventory, magic and stats screens. |

## The first version

`apps/openmw/mwgui/statsscreen.cpp` implements the screen. It shares its base class, sizes and drawing code with the inventory and magic screens. The `stats-screen` scenario walks through it.

### Not done yet

- Mouse input. The screen only responds to a gamepad.
